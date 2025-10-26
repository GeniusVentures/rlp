// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "rlpx_types.hpp"
#include "rlpx_error.hpp"
#include "framing/message_stream.hpp"
#include <boost/asio/awaitable.hpp>
#include <atomic>
#include <memory>

namespace rlpx {

// Hide Boost types (Law of Demeter)
template<typename T>
using Awaitable = boost::asio::awaitable<T>;

// Session creation parameters for outbound connections
struct SessionConnectParams {
    std::string_view remote_host;
    uint16_t remote_port;
    gsl::span<const uint8_t, kPublicKeySize> local_public_key;
    gsl::span<const uint8_t, kPrivateKeySize> local_private_key;
    PublicKey peer_public_key;
    std::string_view client_id;
    uint16_t listen_port;
};

// Session creation parameters for inbound connections
struct SessionAcceptParams {
    gsl::span<const uint8_t, kPublicKeySize> local_public_key;
    gsl::span<const uint8_t, kPrivateKeySize> local_private_key;
    std::string_view client_id;
    uint16_t listen_port;
    // Socket passed separately via executor context
};

// Peer information (grouped values stored as member)
struct PeerInfo {
    PublicKey public_key;
    std::string client_id;
    uint16_t listen_port;
    std::string remote_address;
    uint16_t remote_port;
};

// RLPx session managing encrypted P2P communication
class RlpxSession {
public:
    // Factory for outbound connections
    [[nodiscard]] static Awaitable<Result<std::unique_ptr<RlpxSession>>>
    connect(const SessionConnectParams& params) noexcept;

    // Factory for inbound connections
    [[nodiscard]] static Awaitable<Result<std::unique_ptr<RlpxSession>>>
    accept(const SessionAcceptParams& params) noexcept;

    ~RlpxSession();

    // Non-copyable, moveable
    RlpxSession(const RlpxSession&) = delete;
    RlpxSession& operator=(const RlpxSession&) = delete;
    RlpxSession(RlpxSession&&) noexcept;
    RlpxSession& operator=(RlpxSession&&) noexcept;

    // Send message (takes ownership via move)
    [[nodiscard]] VoidResult
    post_message(framing::Message message) noexcept;

    // Receive message (coroutine pull model)
    [[nodiscard]] Awaitable<Result<framing::Message>>
    receive_message() noexcept;

    // Graceful disconnect
    [[nodiscard]] Awaitable<VoidResult>
    disconnect(DisconnectReason reason) noexcept;

    // State queries
    [[nodiscard]] SessionState state() const noexcept {
        return state_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool is_active() const noexcept {
        return state() == SessionState::kActive;
    }

    // Return const reference to grouped peer info
    [[nodiscard]] const PeerInfo& peer_info() const noexcept {
        return peer_info_;
    }

    // Access to cipher secrets if needed (grouped values)
    [[nodiscard]] const auth::FrameSecrets& cipher_secrets() const noexcept;

private:
    // Private constructor - use factories
    RlpxSession(
        std::unique_ptr<framing::MessageStream> stream,
        PeerInfo peer_info,
        bool is_initiator
    ) noexcept;

    // Internal coroutine loops
    [[nodiscard]] Awaitable<VoidResult> run_send_loop() noexcept;
    [[nodiscard]] Awaitable<VoidResult> run_receive_loop() noexcept;

    // State management
    std::atomic<SessionState> state_{SessionState::kUninitialized};
    
    // Message stream (owns cipher and socket abstraction)
    std::unique_ptr<framing::MessageStream> stream_;
    
    // Peer metadata - stored as member for const reference access
    PeerInfo peer_info_;
    bool is_initiator_;

    // Message channels (lock-free, hidden Boost types)
    class MessageChannel;
    std::unique_ptr<MessageChannel> send_channel_;
    std::unique_ptr<MessageChannel> recv_channel_;
};

} // namespace rlpx
