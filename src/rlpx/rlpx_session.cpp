// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/rlpx_session.hpp>
#include <rlpx/auth/auth_handshake.hpp>
#include <rlpx/framing/frame_cipher.hpp>
#include <rlpx/protocol/messages.hpp>
#include <rlpx/socket/socket_transport.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <queue>
#include <mutex>
#include <chrono>

namespace rlpx {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Message channel for lock-free communication
class RlpxSession::MessageChannel {
public:
    MessageChannel() = default;

    void push(framing::Message msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(msg));
    }

    std::optional<framing::Message> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        auto msg = std::move(queue_.front());
        queue_.pop();
        return msg;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::queue<framing::Message> queue_;
};

// Private constructor
RlpxSession::RlpxSession(
    std::unique_ptr<framing::MessageStream> stream,
    PeerInfo peer_info,
    bool is_initiator
) noexcept
    : stream_(std::move(stream))
    , peer_info_(std::move(peer_info))
    , is_initiator_(is_initiator)
    , send_channel_(std::make_unique<MessageChannel>())
    , recv_channel_(std::make_unique<MessageChannel>())
{
}

RlpxSession::~RlpxSession() {
    // Ensure we're in a terminal state
    auto current_state = state_.load(std::memory_order_acquire);
    if (current_state != SessionState::kClosed && current_state != SessionState::kError) {
        // Force transition to closed state
        state_.store(SessionState::kClosed, std::memory_order_release);
    }
    
    // Channels and stream will be cleaned up automatically via unique_ptr
}

// Move operations - need special handling for atomic
RlpxSession::RlpxSession(RlpxSession&& other) noexcept
    : state_(other.state_.load(std::memory_order_acquire))
    , stream_(std::move(other.stream_))
    , peer_info_(std::move(other.peer_info_))
    , is_initiator_(other.is_initiator_)
    , send_channel_(std::move(other.send_channel_))
    , recv_channel_(std::move(other.recv_channel_))
{
}

RlpxSession& RlpxSession::operator=(RlpxSession&& other) noexcept {
    if (this != &other) {
        state_.store(other.state_.load(std::memory_order_acquire), std::memory_order_release);
        stream_ = std::move(other.stream_);
        peer_info_ = std::move(other.peer_info_);
        is_initiator_ = other.is_initiator_;
        send_channel_ = std::move(other.send_channel_);
        recv_channel_ = std::move(other.recv_channel_);
    }
    return *this;
}

// Factory for outbound connections
Awaitable<Result<std::unique_ptr<RlpxSession>>>
RlpxSession::connect(const SessionConnectParams& params) noexcept {
    try {
        // Step 1: Establish TCP connection with timeout
        auto executor = co_await boost::asio::this_coro::executor;
        
        constexpr auto kConnectionTimeout = std::chrono::seconds(10);
        auto transport_result = co_await socket::connect_with_timeout(
            executor,
            params.remote_host,
            params.remote_port,
            kConnectionTimeout
        );
        
        if (!transport_result) {
            co_return transport_result.error();
        }
        
        auto transport = std::move(transport_result.value());
        
        // Step 2: Perform RLPx handshake as initiator
        // TODO: Implement full handshake with AuthHandshake class
        // For now, create dummy secrets for testing connection flow
        auth::FrameSecrets secrets;
        std::memset(secrets.aes_secret.data(), 0, secrets.aes_secret.size());
        std::memset(secrets.mac_secret.data(), 0, secrets.mac_secret.size());
        std::memset(secrets.ingress_mac_seed.data(), 0, secrets.ingress_mac_seed.size());
        std::memset(secrets.egress_mac_seed.data(), 0, secrets.egress_mac_seed.size());
        
        // Step 3: Create frame cipher with handshake secrets
        auto cipher = std::make_unique<framing::FrameCipher>(std::move(secrets));
        
        // Step 4: Create message stream
        auto stream = std::make_unique<framing::MessageStream>(
            std::move(cipher),
            std::move(transport)
        );
        
        // Step 5: Create session with peer info
        PeerInfo peer_info{
            .public_key = params.peer_public_key,
            .client_id = std::string(params.client_id),
            .listen_port = params.listen_port,
            .remote_address = "",  // TODO: Get from transport
            .remote_port = params.remote_port
        };
        
        auto session = std::unique_ptr<RlpxSession>(new RlpxSession(
            std::move(stream),
            std::move(peer_info),
            true  // is_initiator
        ));
        
        // Transition to active state
        session->state_.store(SessionState::kActive, std::memory_order_release);
        
        // TODO: Step 6: Exchange Hello messages after handshake
        // TODO: Step 7: Start send/receive loops
        // co_spawn(executor, session->run_send_loop(), detached);
        // co_spawn(executor, session->run_receive_loop(), detached);
        
        co_return std::move(session);
        
    } catch (...) {
        co_return SessionError::kConnectionFailed;
    }
}

// Factory for inbound connections
Awaitable<Result<std::unique_ptr<RlpxSession>>>
RlpxSession::accept(const SessionAcceptParams& params) noexcept {
    // TODO: Phase 3.5 - Implement inbound connection acceptance
    // 1. Accept incoming TCP socket
    // 2. Perform RLPx handshake as responder
    // 3. Exchange Hello messages
    // 4. Create MessageStream with FrameCipher
    // 5. Start send/receive loops
    
    // For now, return error indicating not implemented
    co_return SessionError::kConnectionFailed;
}

// Send message
VoidResult RlpxSession::post_message(framing::Message message) noexcept {
    auto current_state = state();
    
    // Only allow sending in active state
    if (current_state != SessionState::kActive) {
        if (current_state == SessionState::kClosed || current_state == SessionState::kError) {
            return SessionError::kConnectionFailed;
        }
        return SessionError::kNotConnected;
    }
    
    send_channel_->push(std::move(message));
    return outcome::success();
}

// Receive message
Awaitable<Result<framing::Message>>
RlpxSession::receive_message() noexcept {
    auto current_state = state();
    
    // Can only receive in active state
    if (current_state != SessionState::kActive) {
        if (current_state == SessionState::kClosed || current_state == SessionState::kError) {
            co_return SessionError::kConnectionFailed;
        }
        co_return SessionError::kNotConnected;
    }
    
    // Check if there's a message in the receive channel
    auto msg = recv_channel_->try_pop();
    if (!msg) {
        co_return SessionError::kNotConnected; // Would be timeout in real impl
    }
    
    co_return std::move(*msg);
}

// Graceful disconnect
Awaitable<VoidResult>
RlpxSession::disconnect(DisconnectReason reason) noexcept {
    auto current_state = state_.load(std::memory_order_acquire);
    
    // Check if already disconnecting or closed
    if (current_state == SessionState::kDisconnecting ||
        current_state == SessionState::kClosed ||
        current_state == SessionState::kError) {
        // Already in terminal or transitioning state
        co_return outcome::success();
    }
    
    // Transition to disconnecting state
    SessionState expected = current_state;
    while (!state_.compare_exchange_weak(
        expected,
        SessionState::kDisconnecting,
        std::memory_order_release,
        std::memory_order_acquire)) {
        // If state changed, check again
        if (expected == SessionState::kDisconnecting ||
            expected == SessionState::kClosed ||
            expected == SessionState::kError) {
            co_return outcome::success();
        }
    }
    
    // TODO: Phase 3.5 - Implement graceful disconnect
    // 1. Send Disconnect message with reason
    // 2. Flush pending messages
    // 3. Close socket
    
    // Transition to closed state
    state_.store(SessionState::kClosed, std::memory_order_release);
    
    co_return outcome::success();
}

// Access to cipher secrets
const auth::FrameSecrets& RlpxSession::cipher_secrets() const noexcept {
    return stream_->cipher_secrets();
}

// Internal send loop
Awaitable<VoidResult> RlpxSession::run_send_loop() noexcept {
    // TODO: Phase 3.5 - Implement send loop
    // While active, pop messages from send_channel_ and send via stream_
    co_return outcome::success();
}

// Internal receive loop
Awaitable<VoidResult> RlpxSession::run_receive_loop() noexcept {
    // TODO: Phase 3.5 - Implement receive loop
    // While active, receive messages from stream_ and push to recv_channel_
    co_return outcome::success();
}

// Message routing
void RlpxSession::route_message(const protocol::Message& msg) noexcept {
    // Route based on message ID
    switch (msg.id) {
        case kHelloMessageId:
            if (hello_handler_) {
                auto hello_result = protocol::HelloMessage::decode(msg.payload);
                if (hello_result.has_value()) {
                    hello_handler_(hello_result.value());
                }
            }
            break;

        case kDisconnectMessageId:
            if (disconnect_handler_) {
                auto disconnect_result = protocol::DisconnectMessage::decode(msg.payload);
                if (disconnect_result.has_value()) {
                    disconnect_handler_(disconnect_result.value());
                }
            }
            break;

        case kPingMessageId:
            if (ping_handler_) {
                auto ping_result = protocol::PingMessage::decode(msg.payload);
                if (ping_result.has_value()) {
                    ping_handler_(ping_result.value());
                }
            }
            break;

        case kPongMessageId:
            if (pong_handler_) {
                auto pong_result = protocol::PongMessage::decode(msg.payload);
                if (pong_result.has_value()) {
                    pong_handler_(pong_result.value());
                }
            }
            break;

        default:
            // Unknown message type - call generic handler if set
            if (generic_handler_) {
                generic_handler_(msg);
            }
            break;
    }
}

// State transition helpers
bool RlpxSession::try_transition_state(SessionState from, SessionState to) noexcept {
    SessionState expected = from;
    return state_.compare_exchange_strong(
        expected,
        to,
        std::memory_order_release,
        std::memory_order_acquire
    );
}

bool RlpxSession::is_terminal_state(SessionState state) const noexcept {
    return state == SessionState::kClosed || state == SessionState::kError;
}

void RlpxSession::force_error_state() noexcept {
    state_.store(SessionState::kError, std::memory_order_release);
}

} // namespace rlpx
