// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/rlpx_session.hpp>
#include <rlpx/auth/auth_handshake.hpp>
#include <rlpx/framing/frame_cipher.hpp>
#include <rlpx/protocol/messages.hpp>
#include <rlpx/socket/socket_transport.hpp>
#include <base/logger.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
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
Result<std::shared_ptr<RlpxSession>>
RlpxSession::connect(const SessionConnectParams& params, asio::yield_context yield) noexcept {
    // Step 1: Establish TCP connection with timeout
    auto executor = yield.get_executor();

    auto transport_result = socket::connect_with_timeout(
        executor,
        params.remote_host,
        params.remote_port,
        kTcpConnectionTimeout,
        yield
    );
    if (!transport_result) {
        return transport_result.error();
    }

    // Step 2: Run the real RLPx auth handshake (auth → ack)
    auth::HandshakeConfig hs_config;
    std::copy(params.local_public_key.begin(),  params.local_public_key.end(),  hs_config.local_public_key.begin());
    std::copy(params.local_private_key.begin(), params.local_private_key.end(), hs_config.local_private_key.begin());
    hs_config.client_id    = std::string(params.client_id);
    hs_config.listen_port  = params.listen_port;
    hs_config.peer_public_key = params.peer_public_key;

    auth::AuthHandshake handshake(hs_config, std::move(transport_result.value()));
    auto hs_result = handshake.execute(yield);
    if (!hs_result) {
        return hs_result.error();
    }
    auto& hs = hs_result.value();

    // Step 3: Build MessageStream with derived frame secrets
    if (!hs.transport) {
        return SessionError::kAuthenticationFailed;
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

    auto session = std::shared_ptr<RlpxSession>(new RlpxSession(
        std::move(stream),
        std::move(peer_info),
        true  // is_initiator
    ));

    // Step 5: Send our HELLO (initiator sends first)
    protocol::HelloMessage hello;
    hello.protocol_version = kProtocolVersion;
    hello.client_id        = std::string(params.client_id);
    // Advertise both ETH/68 and ETH/69 — peers negotiate the highest common version.
    hello.capabilities     = { protocol::Capability{ "eth", 68 }, protocol::Capability{ "eth", 69 } };
    hello.listen_port      = params.listen_port;
    std::copy(params.local_public_key.begin(),
              params.local_public_key.end(),
              hello.node_id.begin());

    auto hello_encoded = hello.encode();
    if (!hello_encoded) {
        return SessionError::kHandshakeFailed;
    }

    framing::MessageSendParams hello_send{
        .message_id = kHelloMessageId,
        .payload    = std::move(hello_encoded.value()),
        .compress   = false
    };
    auto send_result = session->stream_->send_message(hello_send, yield);
    if (!send_result) {
        return send_result.error();
    }

    // Step 6: Receive peer HELLO
    auto recv_result = session->stream_->receive_message(yield);
    if (!recv_result) {
        return recv_result.error();
    }
    auto& peer_msg = recv_result.value();
    {
        static auto log = rlp::base::createLogger("rlpx.session");
        SPDLOG_LOGGER_DEBUG(log, "connect: first message from peer, id=0x{:02x} payload_size={}", peer_msg.id, peer_msg.payload.size());
    }
    if (peer_msg.id == kHelloMessageId) {
        auto peer_hello = protocol::HelloMessage::decode(
            ByteView(peer_msg.payload.data(), peer_msg.payload.size()));
        if (peer_hello) {
            session->peer_info_.client_id     = peer_hello.value().client_id;
            session->peer_info_.listen_port   = peer_hello.value().listen_port;
            static auto log = rlp::base::createLogger("rlpx.session");
            SPDLOG_LOGGER_DEBUG(log, "connect: peer HELLO ok, client='{}' port={} caps={}",
                peer_hello.value().client_id,
                peer_hello.value().listen_port,
                peer_hello.value().capabilities.size());

            // RLPx spec: enable Snappy compression if both sides advertise p2p version >= 5.
            if (peer_hello.value().protocol_version >= kProtocolVersion) {
                session->stream_->enable_compression();
                SPDLOG_LOGGER_DEBUG(log, "connect: Snappy compression enabled (peer p2p v{})",
                    peer_hello.value().protocol_version);
            }

            if (session->hello_handler_) {
                session->hello_handler_(peer_hello.value());
            }
        } else {
            static auto log = rlp::base::createLogger("rlpx.session");
            SPDLOG_LOGGER_WARN(log, "connect: peer HELLO decode failed, payload_size={}", peer_msg.payload.size());
        }
    } else if (peer_msg.id == kDisconnectMessageId) {
        static auto log = rlp::base::createLogger("rlpx.session");
        auto disc = protocol::DisconnectMessage::decode(peer_msg.payload);
        SPDLOG_LOGGER_DEBUG(log, "connect: peer sent Disconnect before HELLO, reason={}",
            disc ? static_cast<int>(disc.value().reason) : -1);
        return SessionError::kHandshakeFailed;
    } else {
        static auto log = rlp::base::createLogger("rlpx.session");
        SPDLOG_LOGGER_WARN(log, "connect: expected HELLO (0x00) but got id=0x{:02x}", peer_msg.id);
    }

    // Step 7: Activate session and start I/O loops
    session->state_.store(SessionState::kActive, std::memory_order_release);

    asio::spawn(
        executor,
        [s = session](asio::yield_context yc) {
            auto result = s->run_send_loop(yc);
            (void)result;
        }
    );

    asio::spawn(
        executor,
        [s = session](asio::yield_context yc) {
            auto result = s->run_receive_loop(yc);
            (void)result;
        }
    );

    return std::move(session);
}

// Factory for inbound connections
Result<std::shared_ptr<RlpxSession>>
RlpxSession::accept(const SessionAcceptParams& params, asio::yield_context /*yield*/) noexcept {
    (void)params;
    // TODO: Phase 3.5 - Implement inbound connection acceptance
    return SessionError::kConnectionFailed;
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
Result<framing::Message>
RlpxSession::receive_message(asio::yield_context yield) noexcept {
    auto current_state = state();
    
    if (current_state != SessionState::kActive) {
        if (current_state == SessionState::kClosed || current_state == SessionState::kError) {
            return SessionError::kConnectionFailed;
        }
        return SessionError::kNotConnected;
    }
    
    // Check if there's a message in the receive channel
    auto msg = recv_channel_->try_pop();
    if (!msg) {
        (void)yield;
        return SessionError::kNotConnected; // Would be timeout in real impl
    }
    
    return std::move(*msg);
}

// Graceful disconnect (sync)
VoidResult
RlpxSession::disconnect(DisconnectReason reason) noexcept {
    (void)reason;
    auto current_state = state_.load(std::memory_order_acquire);

    if (current_state == SessionState::kDisconnecting ||
        current_state == SessionState::kClosed ||
        current_state == SessionState::kError) {
        return outcome::success();
    }

    SessionState expected = current_state;
    while (!state_.compare_exchange_weak(
        expected,
        SessionState::kDisconnecting,
        std::memory_order_release,
        std::memory_order_acquire)) {
        if (expected == SessionState::kDisconnecting ||
            expected == SessionState::kClosed ||
            expected == SessionState::kError) {
            return outcome::success();
        }
    }

    state_.store(SessionState::kClosed, std::memory_order_release);
    if (stream_)
    {
        stream_->close();
    }
    return outcome::success();
}

// Graceful disconnect (coroutine overload)
VoidResult
RlpxSession::disconnect(DisconnectReason reason, asio::yield_context /*yield*/) noexcept {
    return disconnect(reason);
}

// Access to cipher secrets
const auth::FrameSecrets& RlpxSession::cipher_secrets() const noexcept {
    return stream_->cipher_secrets();
}

// Internal send loop
VoidResult RlpxSession::run_send_loop(asio::yield_context yield) noexcept {
    // Continuously send messages while session is active
    while (state() == SessionState::kActive) {
        // Check if there are pending messages to send
        auto msg = send_channel_->try_pop();
        
        if (!msg) {
            // No messages pending — yield and check again
            // TODO: Replace polling with proper async condition variable
            boost::system::error_code ec;
            asio::steady_timer(
                yield.get_executor(),
                kSendLoopPollInterval
            ).async_wait(asio::redirect_error(yield, ec));
            
            if (ec) {
                force_error_state();
                return SessionError::kConnectionFailed;
            }
            continue;
        }
        
        // Compress if stream compression is enabled (set after HELLO negotiation)
        framing::MessageSendParams send_params{
            .message_id = msg->id,
            .payload = msg->payload,
            .compress = stream_->is_compression_enabled()
        };
        
        auto send_result = stream_->send_message(send_params, yield);
        
        if (!send_result) {
            // Network error - transition to error state
            force_error_state();
            return send_result.error();
        }
    }
    
    return outcome::success();
}

// Internal receive loop
VoidResult RlpxSession::run_receive_loop(asio::yield_context yield) noexcept {
    static auto log = rlp::base::createLogger("rlpx.session");
    // Continuously receive messages while session is active
    while (state() == SessionState::kActive) {
        // Receive message from network stream
        auto msg_result = stream_->receive_message(yield);
        
        if (!msg_result) {
            // Network error or connection closed
            SPDLOG_LOGGER_DEBUG(log, "receive_loop: stream error, closing session");
            force_error_state();
            return msg_result.error();
        }
        
        auto& msg = msg_result.value();
        SPDLOG_LOGGER_DEBUG(log, "receive_loop: msg id=0x{:02x} payload_size={}", msg.id, msg.payload.size());
        
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
    
    return outcome::success();
}

// Message routing
void RlpxSession::route_message(const protocol::Message& msg) noexcept {
    static auto log = rlp::base::createLogger("rlpx.session");
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

        case kDisconnectMessageId: {
            auto disconnect_result = protocol::DisconnectMessage::decode(msg.payload);
            if (disconnect_result.has_value()) {
                SPDLOG_LOGGER_DEBUG(log, "route: peer sent Disconnect reason={}", static_cast<int>(disconnect_result.value().reason));
                if (disconnect_handler_) {
                    disconnect_handler_(disconnect_result.value());
                }
            } else {
                SPDLOG_LOGGER_DEBUG(log, "route: peer sent Disconnect (decode failed)");
            }
            break;
        }

        case kPingMessageId:
            SPDLOG_LOGGER_DEBUG(log, "route: Ping received");
            if (ping_handler_) {
                auto ping_result = protocol::PingMessage::decode(msg.payload);
                if (ping_result.has_value()) {
                    ping_handler_(ping_result.value());
                }
            }
            break;

        case kPongMessageId:
            SPDLOG_LOGGER_DEBUG(log, "route: Pong received");
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
