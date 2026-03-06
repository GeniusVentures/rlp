// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include "discv4/discv4_client.hpp"
#include "discv4/discv4_error.hpp"
#include "discv4/discv4_ping.hpp"
#include "discv4/discv4_pong.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/keccak.hpp>
#include <iostream>

namespace discv4 {

discv4_client::discv4_client(asio::io_context& io_context, const discv4Config& config)
    : io_context_(io_context)
    , config_(config)
    , socket_(io_context, udp::endpoint(udp::v4(), config.bind_port)) {
}

discv4_client::~discv4_client() {
    stop();
}

rlpx::VoidResult discv4_client::start() {
    if (running_.exchange(true)) {
        return rlp::outcome::success();  // Already running
    }

    // Start receive loop
    asio::co_spawn(io_context_, receive_loop(), asio::detached);

    return rlp::outcome::success();
}

void discv4_client::stop() {
    running_ = false;
    boost::system::error_code ec;
    socket_.close(ec);
}

boost::asio::awaitable<void> discv4_client::receive_loop() {
    std::array<uint8_t, 2048> buffer;

    while (running_) {
        udp::endpoint sender_endpoint;
        boost::system::error_code ec;

        size_t bytes_received = co_await socket_.async_receive_from(
            asio::buffer(buffer),
            sender_endpoint,
            asio::redirect_error(asio::use_awaitable, ec)
        );

        if (ec) {
            if (ec == asio::error::operation_aborted) {
                break;  // Socket closed
            }
            if (error_callback_) {
                error_callback_("Receive error: " + ec.message());
            }
            continue;
        }

        if (bytes_received > 0) {
            handle_packet(buffer.data(), bytes_received, sender_endpoint);
        }
    }
}

void discv4_client::handle_packet(const uint8_t* data, size_t length, const udp::endpoint& sender) {
    if (length < 98) {  // Minimum packet size: 32 (hash) + 65 (signature) + 1 (type)
        return;
    }

    // Packet format: hash(32) || signature(65) || packet-type(1) || packet-data
    const uint8_t packet_type = data[97];

    switch (packet_type) {
        case 0x01:  // PING
            handle_ping(data, length, sender);
            break;
        case 0x02:  // PONG
            handle_pong(data, length, sender);
            break;
        case 0x03:  // FIND_NODE
            handle_find_node(data, length, sender);
            break;
        case 0x04:  // NEIGHBOURS
            handle_neighbours(data, length, sender);
            break;
        default:
            if (error_callback_) {
                error_callback_("Unknown packet type: " + std::to_string(packet_type));
            }
            break;
    }
}

void discv4_client::handle_ping(const uint8_t* data, size_t length, const udp::endpoint& sender) {
    // TODO: Implement PING handling
    // For now, just log that we received it
    (void)data;
    (void)length;
    (void)sender;
}

void discv4_client::handle_pong(const uint8_t* data, size_t length, const udp::endpoint& sender) {
    // Parse PONG packet
    // Skip hash(32) + signature(65) + type(1) = 98 bytes
    if (length < 98) {
        return;
    }

    rlp::ByteView payload(data + 98, length - 98);
    auto pong_result = discv4_pong::Parse(payload);

    if (!pong_result) {
        if (error_callback_) {
            error_callback_("Failed to parse PONG packet");
        }
        return;
    }

    const auto& pong = pong_result.value();

    // Extract node ID from signature
    auto node_id_result = verify_packet(data, length);
    if (!node_id_result) {
        return;
    }

    // Update peer table
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);

        DiscoveredPeer peer;
        peer.node_id = node_id_result.value();
        peer.ip = sender.address().to_string();
        peer.udp_port = sender.port();
        peer.tcp_port = pong.toEndpoint.tcpPort;
        peer.last_seen = std::chrono::steady_clock::now();

        // Convert node_id to hex string for map key
        std::string key;
        key.reserve(128);
        for (uint8_t byte : peer.node_id) {
            const char* hex = "0123456789abcdef";
            key += hex[byte >> 4];
            key += hex[byte & 0x0f];
        }

        peers_[key] = peer;

        if (peer_callback_) {
            peer_callback_(peer);
        }
    }
}

void discv4_client::handle_find_node(const uint8_t* data, size_t length, const udp::endpoint& sender) {
    // TODO: Implement FIND_NODE handling
    (void)data;
    (void)length;
    (void)sender;
}

void discv4_client::handle_neighbours(const uint8_t* data, size_t length, const udp::endpoint& sender) {
    // TODO: Implement NEIGHBOURS handling
    (void)data;
    (void)length;
    (void)sender;
}

boost::asio::awaitable<discv4::Result<discv4_pong>> discv4_client::ping(
    const std::string& ip,
    uint16_t port,
    const NodeId& node_id
) {
    // Create PING packet
    discv4_ping ping_packet(
        config_.bind_ip, config_.bind_port, config_.tcp_port,
        ip, port, port
    );

    auto payload = ping_packet.RlpPayload();
    if (payload.empty()) {
        co_return discv4Error::kRlpPayloadEmpty;
    }

    // Sign the packet
    auto signed_packet = sign_packet(payload);
    if (!signed_packet) {
        co_return discv4Error::kSigningFailed;
    }

    // Send packet
    udp::endpoint destination(asio::ip::make_address(ip), port);
    auto send_result = co_await send_packet(signed_packet.value(), destination);
    if (!send_result) {
        co_return discv4Error::kNetworkSendFailed;
    }

    // Wait for PONG response (simplified - in production use proper async waiting)
    // For now, return success - PONG will be handled in receive_loop
    co_return discv4Error::kPongTimeout;  // Placeholder
}

boost::asio::awaitable<rlpx::VoidResult> discv4_client::find_node(
    const std::string& ip,
    uint16_t port,
    const NodeId& target_id
) {
    // TODO: Implement FIND_NODE
    (void)ip;
    (void)port;
    (void)target_id;
    co_return rlp::outcome::success();
}

boost::asio::awaitable<discv4::Result<void>> discv4_client::send_packet(
    const std::vector<uint8_t>& packet,
    const udp::endpoint& destination
) {
    boost::system::error_code ec;

    co_await socket_.async_send_to(
        asio::buffer(packet),
        destination,
        asio::redirect_error(asio::use_awaitable, ec)
    );

    if (ec) {
        if (error_callback_) {
            error_callback_("Send error: " + ec.message());
        }
        co_return discv4Error::kNetworkSendFailed;
    }

    co_return rlp::outcome::success();
}

discv4::Result<std::vector<uint8_t>> discv4_client::sign_packet(const std::vector<uint8_t>& payload) {
    // Packet format: hash(32) || signature(65) || packet-type(1) || packet-data

    // Compute hash of signature || packet-type || packet-data
    std::vector<uint8_t> to_hash;
    to_hash.reserve(65 + payload.size());
    to_hash.resize(65);  // Placeholder for signature
    to_hash.insert(to_hash.end(), payload.begin(), payload.end());

    // Compute keccak256 hash
    auto hash_array = discv4_packet::Keccak256(to_hash);

    // Sign the hash with secp256k1
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    if (!ctx) {
        return discv4Error::kContextCreationFailed;
    }

    secp256k1_ecdsa_recoverable_signature sig;
    if (!secp256k1_ecdsa_sign_recoverable(ctx, &sig, hash_array.data(),
                                          config_.private_key.data(), nullptr, nullptr)) {
        secp256k1_context_destroy(ctx);
        return discv4Error::kSigningFailed;
    }

    // Serialize signature
    std::array<uint8_t, 65> signature_bytes;
    int recid;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(
        ctx, signature_bytes.data(), &recid, &sig
    );
    signature_bytes[64] = static_cast<uint8_t>(recid);

    secp256k1_context_destroy(ctx);

    // Build final packet: hash || signature || payload
    std::vector<uint8_t> packet;
    packet.reserve(32 + 65 + payload.size());
    packet.insert(packet.end(), hash_array.begin(), hash_array.end());
    packet.insert(packet.end(), signature_bytes.begin(), signature_bytes.end());
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;
}

discv4::Result<NodeId> discv4_client::verify_packet(const uint8_t* data, size_t length) {
    if (length < 98) {
        return discv4Error::kPacketTooSmall;
    }

    // Extract signature (bytes 32-96)
    const uint8_t* signature_data = data + 32;

    // Extract payload (bytes 97+)
    const uint8_t* payload_data = data + 97;
    size_t payload_length = length - 97;

    // Compute hash of signature || payload
    std::vector<uint8_t> to_hash;
    to_hash.insert(to_hash.end(), signature_data, signature_data + 65);
    to_hash.insert(to_hash.end(), payload_data, payload_data + payload_length);

    auto computed_hash = discv4_packet::Keccak256(to_hash);

    // Verify hash matches
    if (std::memcmp(data, computed_hash.data(), 32) != 0) {
        return discv4Error::kHashMismatch;
    }

    // Recover public key from signature
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    if (!ctx) {
        return discv4Error::kContextCreationFailed;
    }

    secp256k1_ecdsa_recoverable_signature sig;
    int recid = signature_data[64];
    if (!secp256k1_ecdsa_recoverable_signature_parse_compact(
            ctx, &sig, signature_data, recid)) {
        secp256k1_context_destroy(ctx);
        return discv4Error::kSignatureParseFailed;
    }

    secp256k1_pubkey pubkey;
    if (!secp256k1_ecdsa_recover(ctx, &pubkey, &sig, computed_hash.data())) {
        secp256k1_context_destroy(ctx);
        return discv4Error::kSignatureRecoveryFailed;
    }

    // Serialize public key (uncompressed, 65 bytes)
    std::array<uint8_t, 65> pubkey_bytes;
    size_t pubkey_len = 65;
    secp256k1_ec_pubkey_serialize(ctx, pubkey_bytes.data(), &pubkey_len, &pubkey, SECP256K1_EC_UNCOMPRESSED);

    secp256k1_context_destroy(ctx);

    // Convert to NodeId (skip first byte which is 0x04 for uncompressed)
    NodeId node_id;
    std::copy(pubkey_bytes.begin() + 1, pubkey_bytes.end(), node_id.begin());

    return node_id;
}

NodeId discv4_client::compute_node_id(const std::array<uint8_t, 64>& public_key) {
    // Node ID is keccak256(public_key)
    // But for discv4, node ID IS the public key itself (64 bytes)
    return public_key;
}

std::vector<DiscoveredPeer> discv4_client::get_peers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);

    std::vector<DiscoveredPeer> result;
    result.reserve(peers_.size());

    for (const auto& [key, peer] : peers_) {
        result.push_back(peer);
    }

    return result;
}

void discv4_client::set_peer_discovered_callback(PeerDiscoveredCallback callback) {
    peer_callback_ = std::move(callback);
}

void discv4_client::set_error_callback(ErrorCallback callback) {
    error_callback_ = std::move(callback);
}

} // namespace discv4

