// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/rlpx_session.hpp>
#include <rlpx/auth/auth_handshake.hpp>
#include <rlpx/framing/frame_cipher.hpp>
#include <rlpx/protocol/messages.hpp>
#include <rlpx/socket/socket_transport.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/system/error_code.hpp>
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
    // Step 1: Establish TCP connection with timeout
    auto executor = co_await boost::asio::this_coro::executor;

    auto transport_result = co_await socket::connect_with_timeout(
        executor,
        params.remote_host,
        params.remote_port,
        kTcpConnectionTimeout
    );
    if (!transport_result) {
        co_return transport_result.error();
    }

    // Step 2: Run the real RLPx auth handshake (auth → ack)
    auth::HandshakeConfig hs_config;
    std::copy(params.local_public_key.begin(),  params.local_public_key.end(),  hs_config.local_public_key.begin());
    std::copy(params.local_private_key.begin(), params.local_private_key.end(), hs_config.local_private_key.begin());
    hs_config.client_id    = std::string(params.client_id);
    hs_config.listen_port  = params.listen_port;
    hs_config.peer_public_key = params.peer_public_key;

    auth::AuthHandshake handshake(hs_config, std::move(transport_result.value()));
    auto hs_result = co_await handshake.execute();
    if (!hs_result) {
        co_return hs_result.error();
    }
    auto& hs = hs_result.value();

    // Step 3: Build MessageStream with derived frame secrets
    if (!hs.transport) {
        co_return SessionError::kAuthenticationFailed;
    }
    auto cipher = std::make_unique<framing::FrameCipher>(hs.frame_secrets);
    auto stream = std::make_unique<framing::MessageStream>(
        std::move(cipher),
        std::move(hs.transport.value())
    );

    // Step 4: Build session with peer info
    PeerInfo peer_info{
        .public_key      = params.peer_public_key,
        .client_id       = std::string(params.client_id),
        .listen_port     = params.listen_port,
        .remote_address  = "",
        .remote_port     = params.remote_port
    };

    auto session = std::unique_ptr<RlpxSession>(new RlpxSession(
        std::move(stream),
        std::move(peer_info),
        true  // is_initiator
    ));

    // Step 5: Send our HELLO (initiator sends first)
    protocol::HelloMessage hello;
    hello.protocol_version = kProtocolVersion;
    hello.client_id        = std::string(params.client_id);
    hello.capabilities     = { protocol::Capability{ "eth", 68 },
                                protocol::Capability{ "eth", 67 },
                                protocol::Capability{ "eth", 66 } };
    hello.listen_port      = params.listen_port;
    std::copy(params.local_public_key.begin(),
              params.local_public_key.end(),
              hello.node_id.begin());

    auto hello_encoded = hello.encode();
    if (!hello_encoded) {
        co_return SessionError::kHandshakeFailed;
    }

    framing::MessageSendParams hello_send{
        .message_id = kHelloMessageId,
        .payload    = std::move(hello_encoded.value()),
        .compress   = false
    };
    auto send_result = co_await session->stream_->send_message(hello_send);
    if (!send_result) {
        co_return send_result.error();
    }

    // Step 6: Receive peer HELLO
    auto recv_result = co_await session->stream_->receive_message();
    if (!recv_result) {
        co_return recv_result.error();
    }
    auto& peer_msg = recv_result.value();
    if (peer_msg.id == kHelloMessageId) {
        auto peer_hello = protocol::HelloMessage::decode(
            ByteView(peer_msg.payload.data(), peer_msg.payload.size()));
        if (peer_hello) {
            session->peer_info_.client_id     = peer_hello.value().client_id;
            session->peer_info_.listen_port   = peer_hello.value().listen_port;
            // Fire handler if already registered (unlikely here, but safe)
            if (session->hello_handler_) {
                session->hello_handler_(peer_hello.value());
            }
        }
    }

    // Step 7: Activate session and start I/O loops
    session->state_.store(SessionState::kActive, std::memory_order_release);

    boost::asio::co_spawn(
        executor,
        [session_ptr = session.get()]() -> Awaitable<void> {
            auto result = co_await session_ptr->run_send_loop();
            (void)result;
        },
        [](std::exception_ptr) noexcept {}
    );

    boost::asio::co_spawn(
        executor,
        [session_ptr = session.get()]() -> Awaitable<void> {
            auto result = co_await session_ptr->run_receive_loop();
            (void)result;
        },
        [](std::exception_ptr) noexcept {}
    );

    co_return std::move(session);
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
    // Continuously send messages while session is active
    while (state() == SessionState::kActive) {
        // Check if there are pending messages to send
        auto msg = send_channel_->try_pop();
        
        if (!msg) {
            // No messages pending — yield and check again
            // TODO: Replace polling with proper async condition variable
            boost::system::error_code ec;
            co_await boost::asio::steady_timer(
                co_await boost::asio::this_coro::executor,
                kSendLoopPollInterval
            ).async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            
            if (ec) {
                force_error_state();
                co_return SessionError::kConnectionFailed;
            }
            continue;
        }
        
        // Send message through stream
        framing::MessageSendParams send_params{
            .message_id = msg->id,
            .payload = msg->payload,
            .compress = false  // TODO: Enable compression based on capabilities
        };
        
        auto send_result = co_await stream_->send_message(send_params);
        
        if (!send_result) {
            // Network error - transition to error state
            force_error_state();
            co_return send_result.error();
        }
    }
    
    co_return outcome::success();
}

// Internal receive loop
Awaitable<VoidResult> RlpxSession::run_receive_loop() noexcept {
    // Continuously receive messages while session is active
    while (state() == SessionState::kActive) {
        // Receive message from network stream
        auto msg_result = co_await stream_->receive_message();
        
        if (!msg_result) {
            // Network error or connection closed
            force_error_state();
            co_return msg_result.error();
        }
        
        auto& msg = msg_result.value();
        
        // Convert framing::Message to protocol::Message for routing
        protocol::Message proto_msg{
            .id = msg.id,
            .payload = std::move(msg.payload)
        };
        
        // Route message to appropriate handler (if registered)
        route_message(proto_msg);
        
        // Also push to receive channel for pull-based consumption
        framing::Message frame_msg{
            .id = proto_msg.id,
            .payload = std::move(proto_msg.payload)
        };
        recv_channel_->push(std::move(frame_msg));
    }
    
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
