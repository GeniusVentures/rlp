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
        encoder.add(protocol_version);
        
        // Client ID (as string bytes)
        encoder.add(detail::to_rlp_view(ByteView(
            reinterpret_cast<const uint8_t*>(client_id.data()), 
            client_id.size()
        )));
        
        // Capabilities (list of [name, version] pairs)
        encoder.BeginList();
        for ( const auto& cap : capabilities ) {
            encoder.BeginList();
            encoder.add(detail::to_rlp_view(ByteView(
                reinterpret_cast<const uint8_t*>(cap.name.data()),
                cap.name.size()
            )));
            encoder.add(cap.version);
            encoder.EndList();
        }
        encoder.EndList();
        
        // Listen port
        encoder.add(listen_port);
        
        // Node ID (64 bytes)
        encoder.AddRaw(detail::to_rlp_view(ByteView(node_id.data(), node_id.size())));
        
        encoder.EndList();
        
        return detail::from_rlp_bytes(encoder.GetBytes());
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

Result<HelloMessage> HelloMessage::decode(ByteView rlp_data) noexcept {
    // TODO: Implement proper RLP decoding with explicit error handling
    // This is a stub to allow compilation
    return SessionError::kInvalidMessage;
}

// DisconnectMessage implementation
Result<ByteBuffer> DisconnectMessage::encode() const noexcept {
    try {
        rlp::RlpEncoder encoder;
        
        // Disconnect message is a list with single element: [reason]
        encoder.BeginList();
        encoder.add(reason_to_byte(reason));
        encoder.EndList();
        
        return detail::from_rlp_bytes(encoder.GetBytes());
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

Result<DisconnectMessage> DisconnectMessage::decode(ByteView rlp_data) noexcept {
    // TODO: Implement proper RLP decoding with explicit error handling
    // This is a stub to allow compilation
    return SessionError::kInvalidMessage;
}

// PingMessage implementation
Result<ByteBuffer> PingMessage::encode() const noexcept {
    try {
        rlp::RlpEncoder encoder;
        
        // Ping is an empty list
        encoder.BeginList();
        encoder.EndList();
        
        return detail::from_rlp_bytes(encoder.GetBytes());
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

Result<PingMessage> PingMessage::decode(ByteView rlp_data) noexcept {
    // Ping is just an empty list - minimal validation
    return PingMessage{};
}

// PongMessage implementation
Result<ByteBuffer> PongMessage::encode() const noexcept {
    try {
        rlp::RlpEncoder encoder;
        
        // Pong is an empty list
        encoder.BeginList();
        encoder.EndList();
        
        return detail::from_rlp_bytes(encoder.GetBytes());
    } catch ( ... ) {
        return SessionError::kInvalidMessage;
    }
}

Result<PongMessage> PongMessage::decode(ByteView rlp_data) noexcept {
    // Pong is just an empty list - minimal validation
    return PongMessage{};
}

} // namespace rlpx::protocol
