// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <base/logger.hpp>
#include <discv5/discv5_crawler.hpp>
#include <discv5/discv5_error.hpp>
#include <discv5/discv5_types.hpp>

namespace discv5
{

using rlp::base::Logger;
using rlp::base::createLogger;

namespace asio = boost::asio;
using udp = asio::ip::udp;

// ---------------------------------------------------------------------------
// discv5_client
// ---------------------------------------------------------------------------

/// @brief Discovery v5 protocol client.
///
/// Owns the UDP socket, drives the receive loop, and delegates peer lifecycle
/// management to an internal @p discv5_crawler.
///
/// The public interface is deliberately narrow and mirrors the discv4_client
/// shape so that callers can adopt either protocol with minimal friction:
///
/// @code{.cpp}
///   discv5::discv5Config cfg;
///   cfg.bootstrap_enrs = ChainBootnodeRegistry::for_chain(ChainId::kEthereumSepolia)->fetch();
///   auto client = std::make_unique<discv5::discv5_client>(io, cfg);
///   client->set_peer_discovered_callback([](const discovery::ValidatedPeer& p){ … });
///   client->start();
///   io.run();
/// @endcode
///
/// Thread safety
/// -------------
/// All public methods must be called from the thread that drives the supplied
/// @p io_context.  The peer-discovered callback is invoked on that same thread.
class discv5_client
{
public:
    /// @brief Construct the client.
    ///
    /// Does NOT bind the socket or start the receive loop.  Call start().
    ///
    /// @param io_context  Boost.Asio io_context.  Must outlive this object.
    /// @param config      Client configuration.  A copy is taken.
    explicit discv5_client(asio::io_context& io_context, const discv5Config& config);

    ~discv5_client();

    // Non-copyable, non-movable.
    discv5_client(const discv5_client&)            = delete;
    discv5_client& operator=(const discv5_client&) = delete;
    discv5_client(discv5_client&&)                 = delete;
    discv5_client& operator=(discv5_client&&)      = delete;

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    /// @brief Add a single bootstrap ENR URI.
    ///
    /// May be called before or after start().
    ///
    /// @param enr_uri  "enr:…" or "enode://…" string.
    void add_bootnode(const std::string& enr_uri) noexcept;

    /// @brief Register the callback invoked for each newly discovered peer.
    ///
    /// @param callback  Called with a ValidatedPeer on every new discovery.
    void set_peer_discovered_callback(PeerDiscoveredCallback callback) noexcept;

    /// @brief Register the error callback.
    ///
    /// @param callback  Called with a diagnostic string on non-fatal errors.
    void set_error_callback(ErrorCallback callback) noexcept;

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /// @brief Bind the UDP socket and start the receive + crawler loops.
    ///
    /// @return  success, or a discv5Error on socket/bind failure.
    VoidResult start() noexcept;

    /// @brief Close the UDP socket and signal the crawler to stop.
    void stop() noexcept;

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// @brief Return a snapshot of current crawler activity counters.
    [[nodiscard]] CrawlerStats stats() const noexcept;

    /// @brief Return the local node identifier (64-byte public key).
    [[nodiscard]] const NodeId& local_node_id() const noexcept;

private:
    // -----------------------------------------------------------------------
    // Internal coroutines
    // -----------------------------------------------------------------------

    /// @brief Async receive loop — reads UDP packets and dispatches them.
    ///
    /// @param yield  Boost.ASIO stackful coroutine context.
    void receive_loop(asio::yield_context yield);

    /// @brief Async crawler loop — drains the queued peer set and issues
    ///        FINDNODE requests at the configured interval.
    ///
    /// @param yield  Boost.ASIO stackful coroutine context.
    void crawler_loop(asio::yield_context yield);

    /// @brief Handle a raw incoming UDP packet.
    ///
    /// @param data    Pointer to packet bytes.
    /// @param length  Byte count.
    /// @param sender  Remote endpoint.
    void handle_packet(
        const uint8_t*     data,
        size_t             length,
        const udp::endpoint& sender) noexcept;

    /// @brief Send a FINDNODE request to @p target.
    ///
    /// @param peer    Peer to query.
    /// @param yield   Boost.ASIO stackful coroutine context.
    VoidResult send_findnode(const ValidatedPeer& peer, asio::yield_context yield);

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    asio::io_context& io_context_;
    discv5Config      config_;
    udp::socket       socket_;
    discv5_crawler    crawler_;
    Logger            logger_ = createLogger("discv5");

    std::atomic<bool> running_{false};
};

} // namespace discv5
