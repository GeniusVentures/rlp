// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../rlpx_types.hpp"
#include "../rlpx_error.hpp"
#include "auth_keys.hpp"
#include <boost/asio/awaitable.hpp>

namespace rlpx::auth {

// Hide Boost types (Law of Demeter)
template<typename T>
using Awaitable = boost::asio::awaitable<T>;

// Handshake configuration parameters
struct HandshakeConfig {
    gsl::span<const uint8_t, kPublicKeySize> local_public_key;
    gsl::span<const uint8_t, kPrivateKeySize> local_private_key;
    std::string_view client_id;
    uint16_t listen_port;
    // Optional for initiator (outbound), empty for recipient (inbound)
    std::optional<PublicKey> peer_public_key;
};

// Handshake result containing all derived material
struct HandshakeResult {
    AuthKeyMaterial key_material;
    FrameSecrets frame_secrets;
    std::string peer_client_id;
    uint16_t peer_listen_port;
    
    // Grouped access to crypto material
    [[nodiscard]] const AuthKeyMaterial& keys() const noexcept { 
        return key_material; 
    }
    
    [[nodiscard]] const FrameSecrets& secrets() const noexcept { 
        return frame_secrets; 
    }
};

// Authentication handshake coordinator
class AuthHandshake {
public:
    explicit AuthHandshake(const HandshakeConfig& config) noexcept;

    // Execute full handshake (auth + hello exchange)
    // Socket operations handled via internal abstraction
    [[nodiscard]] Awaitable<Result<HandshakeResult>>
    execute() noexcept;

    // State query
    [[nodiscard]] bool is_initiator() const noexcept { 
        return config_.peer_public_key.has_value(); 
    }

private:
    // Internal auth phase (sends/receives auth messages)
    [[nodiscard]] Awaitable<AuthResult<AuthKeyMaterial>>
    perform_auth() noexcept;

    // Internal hello exchange (capability negotiation)
    [[nodiscard]] Awaitable<Result<void>>
    exchange_hello(ByteView aes_key, ByteView mac_key) noexcept;

    // Derive frame secrets from auth keys
    [[nodiscard]] static FrameSecrets
    derive_frame_secrets(const AuthKeyMaterial& keys, bool is_initiator) noexcept;

    HandshakeConfig config_;
    // Socket abstraction injected via executor context
};

} // namespace rlpx::auth
