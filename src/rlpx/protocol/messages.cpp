#include <rlpx/protocol/messages.hpp>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <cstring>

namespace rlpx::protocol {

// Helper to convert DisconnectReason to uint8_t
static uint8_t reason_to_byte(DisconnectReason reason) noexcept {
    return static_cast<uint8_t>(reason);
}

// Helper to convert uint8_t to DisconnectReason
static DisconnectReason byte_to_reason(uint8_t byte) noexcept {
    return static_cast<DisconnectReason>(byte);
}

// HelloMessage implementation
Result<ByteBuffer> HelloMessage::encode() const noexcept {
    rlp::RlpEncoder encoder;
    
    // Hello message is an RLP list: [version, client_id, capabilities, port, node_id]
    if (!encoder.BeginList()) return SessionError::kInvalidMessage;
    
    // Protocol version
    if (!encoder.add(protocol_version)) return SessionError::kInvalidMessage;
    
    // Client ID (as string bytes)
    if (!encoder.add(detail::to_rlp_view(ByteView(
        reinterpret_cast<const uint8_t*>(client_id.data()), 
        client_id.size()
    )))) return SessionError::kInvalidMessage;
    
    // Capabilities (list of [name, version] pairs)
    if (!encoder.BeginList()) return SessionError::kInvalidMessage;
    for ( const auto& cap : capabilities ) {
        if (!encoder.BeginList()) return SessionError::kInvalidMessage;
        if (!encoder.add(detail::to_rlp_view(ByteView(
            reinterpret_cast<const uint8_t*>(cap.name.data()),
            cap.name.size()
        )))) return SessionError::kInvalidMessage;
        if (!encoder.add(cap.version)) return SessionError::kInvalidMessage;
        if (!encoder.EndList()) return SessionError::kInvalidMessage;
    }
    if (!encoder.EndList()) return SessionError::kInvalidMessage;
    
    // Listen port
    if (!encoder.add(listen_port)) return SessionError::kInvalidMessage;
    
    // Node ID (64 bytes)
    if (!encoder.AddRaw(detail::to_rlp_view(ByteView(node_id.data(), node_id.size())))) 
        return SessionError::kInvalidMessage;
    
    if (!encoder.EndList()) return SessionError::kInvalidMessage;
    
    auto result = encoder.GetBytes();
    if (!result) return SessionError::kInvalidMessage;
    return detail::from_rlp_bytes(*result.value());
}

Result<HelloMessage> HelloMessage::decode(ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(detail::to_rlp_view(rlp_data));
    
    // Read the list header
    auto list_size_result = decoder.ReadListHeaderBytes();
    if ( !list_size_result ) {
        return SessionError::kInvalidMessage;
    }
    
    HelloMessage msg;
    
    // Read protocol version as bytes (to handle potential 0x00 case)
    rlp::Bytes version_bytes;
    auto version_read_result = decoder.read(version_bytes);
    if ( !version_read_result ) {
        return SessionError::kInvalidMessage;
    }
    msg.protocol_version = version_bytes.empty() ? 0 : version_bytes[0];
    
    // Read client ID
    rlp::Bytes client_id_bytes;
    auto client_id_read_result = decoder.read(client_id_bytes);
    if ( !client_id_read_result ) {
        return SessionError::kInvalidMessage;
    }
    msg.client_id = std::string(
        reinterpret_cast<const char*>(client_id_bytes.data()),
        client_id_bytes.size()
    );
    
    // Read capabilities list
    auto caps_list_size_result = decoder.ReadListHeaderBytes();
    if ( !caps_list_size_result ) {
        return SessionError::kInvalidMessage;
    }
    
    // Read each capability (which is itself a list of [name, version])
    while ( !decoder.IsFinished() ) {
        // Peek to see if this is still part of the capabilities list or the next field
        // We need to track how many items we've read
        // For simplicity, try to read a list and break if it's not a list
        auto cap_is_list = decoder.IsList();
        if ( !cap_is_list || !cap_is_list.value() ) {
            // Not a list - must be the listen_port
            break;
        }
        
        auto cap_list_size = decoder.ReadListHeaderBytes();
        if ( !cap_list_size ) {
            break;
        }
        
        Capability cap;
        
        // Read capability name
        rlp::Bytes name_bytes;
        if ( !decoder.read(name_bytes) ) {
            continue;
        }
        cap.name = std::string(
            reinterpret_cast<const char*>(name_bytes.data()),
            name_bytes.size()
        );
        
        // Read capability version as bytes
        rlp::Bytes ver_bytes;
        if ( !decoder.read(ver_bytes) ) {
            continue;
        }
        cap.version = ver_bytes.empty() ? 0 : ver_bytes[0];
        
        msg.capabilities.push_back(std::move(cap));
    }
    
    // Read listen port
    uint16_t port;
    auto port_read_result = decoder.read(port);
    if ( !port_read_result ) {
        return SessionError::kInvalidMessage;
    }
    msg.listen_port = port;
    
    // Read node ID (64 bytes) - added with AddRaw so it's just the remaining bytes
    auto remaining = decoder.Remaining();
    if ( remaining.size() != kPublicKeySize ) {
        return SessionError::kInvalidMessage;
    }
    std::memcpy(msg.node_id.data(), remaining.data(), kPublicKeySize);
    
    return msg;
}

// DisconnectMessage implementation
Result<ByteBuffer> DisconnectMessage::encode() const noexcept {
    rlp::RlpEncoder encoder;
    
    // Disconnect message is a list with single element: [reason]
    if (!encoder.BeginList()) return SessionError::kInvalidMessage;
    if (!encoder.add(reason_to_byte(reason))) return SessionError::kInvalidMessage;
    if (!encoder.EndList()) return SessionError::kInvalidMessage;
    
    auto result = encoder.GetBytes();
    if (!result) return SessionError::kInvalidMessage;
    return detail::from_rlp_bytes(*result.value());
}

Result<DisconnectMessage> DisconnectMessage::decode(ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(detail::to_rlp_view(rlp_data));
    
    // Read the list header
    auto list_size_result = decoder.ReadListHeaderBytes();
    if ( !list_size_result ) {
        return SessionError::kInvalidMessage;
    }
    
    DisconnectMessage msg;
    
    // Read reason code as bytes (to handle 0x00 case)
    rlp::Bytes reason_bytes;
    auto reason_read_result = decoder.read(reason_bytes);
    if ( !reason_read_result ) {
        return SessionError::kInvalidMessage;
    }
    
    uint8_t reason_code = reason_bytes.empty() ? 0 : reason_bytes[0];
    msg.reason = byte_to_reason(reason_code);
    
    return msg;
}

// PingMessage implementation
Result<ByteBuffer> PingMessage::encode() const noexcept {
    rlp::RlpEncoder encoder;
    
    // Ping is an empty list
    if (!encoder.BeginList()) return SessionError::kInvalidMessage;
    if (!encoder.EndList()) return SessionError::kInvalidMessage;
    
    auto result = encoder.GetBytes();
    if (!result) return SessionError::kInvalidMessage;
    return detail::from_rlp_bytes(*result.value());
}

Result<PingMessage> PingMessage::decode(ByteView rlp_data) noexcept {
    // Ping is just an empty list - minimal validation
    return PingMessage{};
}

// PongMessage implementation
Result<ByteBuffer> PongMessage::encode() const noexcept {
    rlp::RlpEncoder encoder;
    
    // Pong is an empty list
    if (!encoder.BeginList()) return SessionError::kInvalidMessage;
    if (!encoder.EndList()) return SessionError::kInvalidMessage;
    
    auto result = encoder.GetBytes();
    if (!result) return SessionError::kInvalidMessage;
    return detail::from_rlp_bytes(*result.value());
}

Result<PongMessage> PongMessage::decode(ByteView rlp_data) noexcept {
    // Pong is just an empty list - minimal validation
    return PongMessage{};
}

} // namespace rlpx::protocol
