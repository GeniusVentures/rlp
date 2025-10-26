// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

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
    try {
        rlp::RlpEncoder encoder;
        
        // Hello message is an RLP list: [version, client_id, capabilities, port, node_id]
        encoder.BeginList();
        
        // Protocol version
        encoder.Add(protocol_version);
        
        // Client ID
        encoder.Add(client_id);
        
        // Capabilities (list of [name, version] pairs)
        encoder.BeginList();
        for ( const auto& cap : capabilities ) {
            encoder.BeginList();
            encoder.Add(cap.name);
            encoder.Add(cap.version);
            encoder.EndList();
        }
        encoder.EndList();
        
        // Listen port
        encoder.Add(listen_port);
        
        // Node ID (64 bytes)
        encoder.AddRaw(gsl::span<const uint8_t>(node_id.data(), node_id.size()));
        
        encoder.EndList();
        
        return encoder.MoveBytes();
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

Result<HelloMessage> HelloMessage::decode(ByteView rlp_data) noexcept {
    try {
        rlp::RlpDecoder decoder(rlp_data);
        
        if ( !decoder.IsList() ) {
            return SessionError::kInvalidMessage;
        }
        
        HelloMessage msg;
        
        // Read protocol version
        BOOST_OUTCOME_TRY(version_bytes, decoder.ReadListHeaderBytes());
        if ( version_bytes.size() == 1 ) {
            msg.protocol_version = version_bytes[0];
        }
        
        // Read client ID
        BOOST_OUTCOME_TRY(client_id_bytes, decoder.ReadListHeaderBytes());
        msg.client_id = std::string(client_id_bytes.begin(), client_id_bytes.end());
        
        // Read capabilities list
        BOOST_OUTCOME_TRY(caps_header, decoder.PeekHeader());
        if ( caps_header.is_list ) {
            auto caps_decoder_result = decoder.ReadSubDecoder();
            if ( caps_decoder_result ) {
                auto& caps_decoder = caps_decoder_result.value();
                
                while ( !caps_decoder.IsFinished() ) {
                    BOOST_OUTCOME_TRY(cap_header, caps_decoder.PeekHeader());
                    if ( cap_header.is_list ) {
                        auto cap_decoder_result = caps_decoder.ReadSubDecoder();
                        if ( cap_decoder_result ) {
                            auto& cap_decoder = cap_decoder_result.value();
                            
                            Capability cap;
                            
                            // Read capability name
                            BOOST_OUTCOME_TRY(name_bytes, cap_decoder.ReadListHeaderBytes());
                            cap.name = std::string(name_bytes.begin(), name_bytes.end());
                            
                            // Read capability version
                            BOOST_OUTCOME_TRY(ver_bytes, cap_decoder.ReadListHeaderBytes());
                            if ( ver_bytes.size() == 1 ) {
                                cap.version = ver_bytes[0];
                            }
                            
                            msg.capabilities.push_back(std::move(cap));
                        }
                    } else {
                        caps_decoder.SkipItem();
                    }
                }
            }
        }
        
        // Read listen port
        BOOST_OUTCOME_TRY(port_bytes, decoder.ReadListHeaderBytes());
        if ( port_bytes.size() == 2 ) {
            msg.listen_port = (static_cast<uint16_t>(port_bytes[0]) << 8) |
                            static_cast<uint16_t>(port_bytes[1]);
        }
        
        // Read node ID (64 bytes)
        BOOST_OUTCOME_TRY(node_id_bytes, decoder.ReadListHeaderBytes());
        if ( node_id_bytes.size() == kPublicKeySize ) {
            std::memcpy(msg.node_id.data(), node_id_bytes.data(), kPublicKeySize);
        } else {
            return SessionError::kInvalidMessage;
        }
        
        return msg;
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

// DisconnectMessage implementation
Result<ByteBuffer> DisconnectMessage::encode() const noexcept {
    try {
        rlp::RlpEncoder encoder;
        
        // Disconnect message is a list with single element: [reason]
        encoder.BeginList();
        encoder.Add(reason_to_byte(reason));
        encoder.EndList();
        
        return encoder.MoveBytes();
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

Result<DisconnectMessage> DisconnectMessage::decode(ByteView rlp_data) noexcept {
    try {
        rlp::RlpDecoder decoder(rlp_data);
        
        if ( !decoder.IsList() ) {
            return SessionError::kInvalidMessage;
        }
        
        DisconnectMessage msg;
        
        // Read reason byte
        BOOST_OUTCOME_TRY(reason_bytes, decoder.ReadListHeaderBytes());
        if ( reason_bytes.size() == 1 ) {
            msg.reason = byte_to_reason(reason_bytes[0]);
        } else {
            msg.reason = DisconnectReason::kProtocolError;
        }
        
        return msg;
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

// PingMessage implementation
Result<ByteBuffer> PingMessage::encode() const noexcept {
    try {
        rlp::RlpEncoder encoder;
        
        // Ping is an empty list
        encoder.BeginList();
        encoder.EndList();
        
        return encoder.MoveBytes();
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

Result<PingMessage> PingMessage::decode(ByteView rlp_data) noexcept {
    try {
        rlp::RlpDecoder decoder(rlp_data);
        
        if ( !decoder.IsList() ) {
            return SessionError::kInvalidMessage;
        }
        
        return PingMessage{};
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

// PongMessage implementation
Result<ByteBuffer> PongMessage::encode() const noexcept {
    try {
        rlp::RlpEncoder encoder;
        
        // Pong is an empty list
        encoder.BeginList();
        encoder.EndList();
        
        return encoder.MoveBytes();
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

Result<PongMessage> PongMessage::decode(ByteView rlp_data) noexcept {
    try {
        rlp::RlpDecoder decoder(rlp_data);
        
        if ( !decoder.IsList() ) {
            return SessionError::kInvalidMessage;
        }
        
        return PongMessage{};
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

} // namespace rlpx::protocol
