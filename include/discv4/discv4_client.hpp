// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include "discv4/discv4_pong.hpp"
#include "discv4/discv4_error.hpp"
#include <rlp/result.hpp>
#include <rlpx/rlpx_error.hpp>
#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace discv4 {

namespace asio = boost::asio;
using udp = asio::ip::udp;

// Node identifier (64 bytes - uncompressed secp256k1 public key)
using NodeId = std::array<uint8_t, 64>;

// Discovered peer information
struct DiscoveredPeer {
    NodeId node_id;
    std::string ip;
    uint16_t udp_port;
    uint16_t tcp_port;
    std::chrono::steady_clock::time_point last_seen;
};

// Discovery client configuration
struct discv4Config {
    std::string bind_ip = "0.0.0.0";
    uint16_t bind_port = 30303;
    uint16_t tcp_port = 30303;
    std::array<uint8_t, 32> private_key{};  // secp256k1 private key
    NodeId public_key{};                     // secp256k1 public key (uncompressed, 64 bytes)
    std::chrono::seconds ping_timeout{5};
    std::chrono::seconds peer_expiry{300};   // 5 minutes
};

// Callback types
using PeerDiscoveredCallback = std::function<void(const DiscoveredPeer&)>;
using ErrorCallback = std::function<void(const std::string&)>;

/**
 * @brief Discovery v4 protocol client
 *
 * Implements the Ethereum Discovery v4 protocol for peer discovery.
 * Uses UDP for communication with bootstrap nodes and discovered peers.
 *
 * Protocol flow:
 * 1. Send PING to bootstrap nodes
 * 2. Receive PONG responses
 * 3. Send FIND_NODE to discover more peers
 * 4. Receive NEIGHBOURS responses with peer lists
 * 5. Maintain K-bucket routing table
 */
class discv4_client {
public:
    explicit discv4_client(asio::io_context& io_context, const discv4Config& config);
    ~discv4_client();

    // Start discovery process
    rlpx::VoidResult start();

    // Stop discovery
    void stop();

    // Send PING to a specific node
    boost::asio::awaitable<discv4::Result<discv4_pong>> ping(
        const std::string& ip,
        uint16_t port,
        const NodeId& node_id
    );

    // Send FIND_NODE to discover peers near a target
    boost::asio::awaitable<rlpx::VoidResult> find_node(
        const std::string& ip,
        uint16_t port,
        const NodeId& target_id
    );

    // Get list of discovered peers
    std::vector<DiscoveredPeer> get_peers() const;

    // Set callback for when new peers are discovered
    void set_peer_discovered_callback(PeerDiscoveredCallback callback);

    // Set error callback
    void set_error_callback(ErrorCallback callback);

    // Get local node ID
    const NodeId& local_node_id() const { return config_.public_key; }

private:
    // Receive loop
    boost::asio::awaitable<void> receive_loop();

    // Handle incoming packet
    void handle_packet(const uint8_t* data, size_t length, const udp::endpoint& sender);

    // Handle specific packet types
    void handle_ping(const uint8_t* data, size_t length, const udp::endpoint& sender);
    void handle_pong(const uint8_t* data, size_t length, const udp::endpoint& sender);
    void handle_find_node(const uint8_t* data, size_t length, const udp::endpoint& sender);
    void handle_neighbours(const uint8_t* data, size_t length, const udp::endpoint& sender);

    // Send packet
    boost::asio::awaitable<discv4::Result<void>> send_packet(
        const std::vector<uint8_t>& packet,
        const udp::endpoint& destination
    );

    // Sign packet with ECDSA
    discv4::Result<std::vector<uint8_t>> sign_packet(const std::vector<uint8_t>& payload);

    // Verify packet signature
    discv4::Result<NodeId> verify_packet(const uint8_t* data, size_t length);

    // Compute node ID from public key (keccak256 hash)
    static NodeId compute_node_id(const std::array<uint8_t, 64>& public_key);

    asio::io_context& io_context_;
    discv4Config config_;
    udp::socket socket_;

    // Peer table
    mutable std::mutex peers_mutex_;
    std::unordered_map<std::string, DiscoveredPeer> peers_;  // key: node_id_hex

    // Callbacks
    PeerDiscoveredCallback peer_callback_;
    ErrorCallback error_callback_;

    // Pending requests
    struct PendingRequest {
        std::chrono::steady_clock::time_point sent_time;
        std::function<void(const uint8_t*, size_t)> callback;
    };
    std::unordered_map<std::string, PendingRequest> pending_pings_;  // key: endpoint_string

    // Running state
    std::atomic<bool> running_{false};
};

} // namespace discv4

