// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include "discv5/discv5_client.hpp"
#include "discv5/discv5_constants.hpp"
#include "discv5/discv5_enr.hpp"

#include <rlp/rlp_encoder.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>
#include <cstring>

namespace discv5
{

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

discv5_client::discv5_client(asio::io_context& io_context, const discv5Config& config)
    : io_context_(io_context)
    , config_(config)
    , socket_(io_context, udp::endpoint(udp::v4(), config.bind_port))
    , crawler_(config)
{
}

discv5_client::~discv5_client()
{
    stop();
}

// ---------------------------------------------------------------------------
// add_bootnode
// ---------------------------------------------------------------------------

void discv5_client::add_bootnode(const std::string& enr_uri) noexcept
{
    config_.bootstrap_enrs.push_back(enr_uri);
}

// ---------------------------------------------------------------------------
// set_peer_discovered_callback / set_error_callback
// ---------------------------------------------------------------------------

void discv5_client::set_peer_discovered_callback(PeerDiscoveredCallback callback) noexcept
{
    crawler_.set_peer_discovered_callback(std::move(callback));
}

void discv5_client::set_error_callback(ErrorCallback callback) noexcept
{
    crawler_.set_error_callback(std::move(callback));
}

// ---------------------------------------------------------------------------
// start
// ---------------------------------------------------------------------------

VoidResult discv5_client::start() noexcept
{
    if (running_.exchange(true))
    {
        return rlp::outcome::success();  // Idempotent: already running
    }

    // Start the receive loop on the io_context.
    // M013: co_spawn(..., detached) requires awaitable<void>.
    asio::co_spawn(io_context_,
        [this]() -> asio::awaitable<void>
        {
            co_await receive_loop();
        },
        asio::detached);

    // Start the crawler loop.
    asio::co_spawn(io_context_,
        [this]() -> asio::awaitable<void>
        {
            co_await crawler_loop();
        },
        asio::detached);

    // Seed the crawler with configured bootstrap entries.
    auto crawler_start = crawler_.start();
    if (!crawler_start)
    {
        logger_->warn("discv5_client: crawler start returned: {}",
                      to_string(crawler_start.error()));
    }

    logger_->info("discv5_client started on port {}", socket_.local_endpoint().port());
    return rlp::outcome::success();
}

// ---------------------------------------------------------------------------
// stop
// ---------------------------------------------------------------------------

void discv5_client::stop() noexcept
{
    if (!running_.exchange(false))
    {
        return;
    }

    boost::system::error_code ec;
    socket_.close(ec);
    if (ec)
    {
        logger_->warn("discv5_client: socket close error: {}", ec.message());
    }

    auto stop_result = crawler_.stop();
    (void)stop_result;
}

// ---------------------------------------------------------------------------
// stats / local_node_id
// ---------------------------------------------------------------------------

CrawlerStats discv5_client::stats() const noexcept
{
    return crawler_.stats();
}

const NodeId& discv5_client::local_node_id() const noexcept
{
    return config_.public_key;
}

// ---------------------------------------------------------------------------
// receive_loop
// ---------------------------------------------------------------------------

asio::awaitable<void> discv5_client::receive_loop()
{
    // Receive buffer sized to the maximum valid discv5 packet.
    std::vector<uint8_t> buf(kMaxPacketBytes);

    while (running_.load())
    {
        udp::endpoint sender;
        boost::system::error_code ec;

        const size_t received = co_await socket_.async_receive_from(
            asio::buffer(buf),
            sender,
            asio::redirect_error(asio::use_awaitable, ec));

        if (ec)
        {
            if (!running_.load())
            {
                break;  // Normal shutdown
            }
            logger_->warn("discv5 recv error: {}", ec.message());
            continue;
        }

        if (received < kMinPacketBytes)
        {
            logger_->debug("discv5: dropping undersized packet ({} bytes) from {}",
                           received, sender.address().to_string());
            continue;
        }

        handle_packet(buf.data(), received, sender);
    }
}

// ---------------------------------------------------------------------------
// crawler_loop
// ---------------------------------------------------------------------------

asio::awaitable<void> discv5_client::crawler_loop()
{
    const auto interval = std::chrono::seconds(config_.query_interval_sec);
    asio::steady_timer timer(io_context_);

    while (running_.load())
    {
        // Drain the queued peer set: issue concurrent FINDNODE requests.
        size_t queries_issued = 0U;

        while (queries_issued < config_.max_concurrent_queries)
        {
            auto next = crawler_.dequeue_next();
            if (!next.has_value())
            {
                break;
            }

            const ValidatedPeer peer = next.value();

            // M013: wrap Result-returning coroutine in void lambda.
            asio::co_spawn(io_context_,
                [this, peer]() -> asio::awaitable<void>
                {
                    auto result = co_await send_findnode(peer);
                    if (!result)
                    {
                        crawler_.mark_failed(peer.node_id);
                        logger_->debug("discv5 FINDNODE failed for {}:{}",
                                       peer.ip, peer.udp_port);
                    }
                    else
                    {
                        crawler_.mark_measured(peer.node_id);
                    }
                },
                asio::detached);

            ++queries_issued;
        }

        if (queries_issued > 0U)
        {
            logger_->debug("discv5 crawler: {} FINDNODE queries issued", queries_issued);
        }

        // Sleep until next round.
        boost::system::error_code ec;
        timer.expires_after(interval);
        co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));

        if (ec && running_.load())
        {
            logger_->warn("discv5 crawler timer error: {}", ec.message());
        }
    }
}

// ---------------------------------------------------------------------------
// handle_packet
// ---------------------------------------------------------------------------

void discv5_client::handle_packet(
    const uint8_t*       data,
    size_t               length,
    const udp::endpoint& sender) noexcept
{
    // First-pass packet inspection: discv5 packets are AES-GCM encrypted after
    // the handshake, so full decode requires session state.  For the bootstrap
    // phase we log the receipt so callers can observe activity.
    logger_->debug("discv5: packet ({} bytes) from {}:{}",
                   length,
                   sender.address().to_string(),
                   sender.port());

    // Full WHOAREYOU/HANDSHAKE/message decode is left for the next sprint
    // (requires per-session AES-GCM keys).  For now we mark all received
    // packets as "measured" activity on their sender node_id if it is known.
    (void)data;
}

// ---------------------------------------------------------------------------
// send_findnode
// ---------------------------------------------------------------------------

asio::awaitable<VoidResult> discv5_client::send_findnode(const ValidatedPeer& peer)
{
    // FINDNODE message body: RLP([req_id(bytes), distances(list of uint)])
    // Distances 0–256 represent XOR bucket distances; querying distance 256
    // asks for any known nodes (catch-all for bootstrap).
    static constexpr uint32_t kBootstrapDistance = 256U;

    // Use a simple 4-byte request ID derived from the peer's IP+port to
    // correlate responses without a full session layer.
    rlp::RlpEncoder enc;

    // Begin outer list.
    if (!enc.BeginList())
    {
        co_return discv5Error::kNetworkSendFailed;
    }

    // req_id (4 bytes) — use lower 4 bytes of target udp_port || ip hash as
    // a deterministic but lightweight request correlator.
    const std::array<uint8_t, sizeof(uint32_t)> req_id =
    {
        static_cast<uint8_t>((peer.udp_port >> 8U) & 0xFFU),
        static_cast<uint8_t>( peer.udp_port        & 0xFFU),
        static_cast<uint8_t>((peer.tcp_port >> 8U) & 0xFFU),
        static_cast<uint8_t>( peer.tcp_port        & 0xFFU),
    };
    if (!enc.add(rlp::ByteView(req_id.data(), req_id.size())))
    {
        co_return discv5Error::kNetworkSendFailed;
    }

    // Distances list.
    if (!enc.BeginList() ||
        !enc.add(static_cast<uint32_t>(kBootstrapDistance)) ||
        !enc.EndList())
    {
        co_return discv5Error::kNetworkSendFailed;
    }

    if (!enc.EndList())
    {
        co_return discv5Error::kNetworkSendFailed;
    }

    auto bytes_result = enc.MoveBytes();
    if (!bytes_result)
    {
        co_return discv5Error::kNetworkSendFailed;
    }

    const rlp::Bytes& payload = bytes_result.value();

    // Prepend the FINDNODE message type byte.
    std::vector<uint8_t> packet;
    packet.reserve(1U + payload.size());
    packet.push_back(kMsgFindNode);
    packet.insert(packet.end(), payload.begin(), payload.end());

    // Send UDP datagram.
    boost::system::error_code ec;
    const udp::endpoint destination(
        asio::ip::make_address(peer.ip), peer.udp_port);

    co_await socket_.async_send_to(
        asio::buffer(packet),
        destination,
        asio::redirect_error(asio::use_awaitable, ec));

    if (ec)
    {
        logger_->warn("discv5 FINDNODE send to {}:{} failed: {}",
                      peer.ip, peer.udp_port, ec.message());
        co_return discv5Error::kNetworkSendFailed;
    }

    co_return rlp::outcome::success();
}

} // namespace discv5
