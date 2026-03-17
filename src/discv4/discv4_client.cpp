// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include "discv4/discv4_client.hpp"
#include "discv4/discv4_constants.hpp"
#include "discv4/discv4_error.hpp"
#include "discv4/discv4_ping.hpp"
#include "discv4/discv4_pong.hpp"
#include "base/logger.hpp"
#include <boost/asio/spawn.hpp>
#include <boost/asio/redirect_error.hpp>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/keccak.hpp>

namespace discv4 {

// All wire-protocol constants live in discv4_constants.hpp.
// The following are implementation-only aliases for local brevity.
static constexpr size_t  kHashSize           = kWireHashSize;
static constexpr size_t  kSigSize            = kWireSigSize;
static constexpr size_t  kPacketTypeSize     = kWirePacketTypeSize;
static constexpr size_t  kPacketHeaderSize   = kWireHeaderSize;
static constexpr size_t  kPacketTypeOffset   = kWirePacketTypeOffset;
static constexpr size_t  kUncompressedPubKey = kUncompressedPubKeySize;

discv4_client::discv4_client(asio::io_context& io_context, const discv4Config& config)
    : io_context_(io_context)
    , config_(config)
    , socket_(io_context) {
    boost::system::error_code ec;
    const auto bind_address = asio::ip::make_address(config_.bind_ip, ec);
    if (ec) {
        throw std::runtime_error("Invalid bind_ip: " + config_.bind_ip + " (" + ec.message() + ")");
    }

    socket_.open(bind_address.is_v4() ? udp::v4() : udp::v6(), ec);
    if (ec) {
        throw std::runtime_error("Failed to open UDP socket: " + ec.message());
    }

    socket_.bind(udp::endpoint(bind_address, config_.bind_port), ec);
    if (ec) {
        throw std::runtime_error("Failed to bind UDP socket to " + config_.bind_ip + ":" +
                                 std::to_string(config_.bind_port) + " (" + ec.message() + ")");
    }
}

discv4_client::~discv4_client() {
    stop();
}

rlpx::VoidResult discv4_client::start() {
    if (running_.exchange(true)) {
        return rlp::outcome::success();  // Already running
    }

    // Start receive loop
    asio::spawn(io_context_, [this](asio::yield_context yield) {
        receive_loop(yield);
    });

    return rlp::outcome::success();
}

void discv4_client::stop() {
    running_ = false;
    boost::system::error_code ec;
    socket_.close(ec);
}

void discv4_client::receive_loop(asio::yield_context yield) {
    std::array<uint8_t, kUdpBufferSize> buffer{};

    while (running_) {
        udp::endpoint sender_endpoint;
        boost::system::error_code ec;

        size_t bytes_received = socket_.async_receive_from(
            asio::buffer(buffer),
            sender_endpoint,
            asio::redirect_error(yield, ec)
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
    if (length < kPacketHeaderSize) {
        return;
    }

    const uint8_t packet_type = data[kPacketTypeOffset];

    switch (packet_type) {
        case kPacketTypePing:
            handle_ping(data, length, sender);
            break;
        case kPacketTypePong:
            handle_pong(data, length, sender);
            break;
        case kPacketTypeFindNode:
            handle_find_node(data, length, sender);
            break;
        case kPacketTypeNeighbours:
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
    if (length < kPacketHeaderSize) {
        return;
    }

    // The ping_hash field in PONG = the first kHashSize bytes of the incoming PING wire packet
    std::array<uint8_t, kHashSize> ping_hash{};
    std::copy(data, data + kHashSize, ping_hash.begin());
    logger_->debug("PING from {}:{} — sending PONG", sender.address().to_string(), sender.port());

    // Build PONG payload: packet-type(kPacketTypePong) || RLP([to_endpoint, ping_hash, expiration])
    // to_endpoint = [sender_ip(4), udp_port, tcp_port]
    rlp::RlpEncoder encoder;
    if (!encoder.BeginList()) { return; }

    // to_endpoint sub-list
    if (!encoder.BeginList()) { return; }
    const auto addr     = sender.address().to_v4().to_bytes();
    const rlp::ByteView ip_bv(addr.data(), addr.size());
    if (!encoder.add(ip_bv))                      { return; }
    if (!encoder.add(sender.port()))               { return; }
    if (!encoder.add(config_.tcp_port))            { return; }
    if (!encoder.EndList())                        { return; }

    // ping_hash
    if (!encoder.add(rlp::ByteView(ping_hash.data(), ping_hash.size()))) { return; }

    // expiration = now + 60s
    const uint32_t expiration = static_cast<uint32_t>(std::time(nullptr)) + kPacketExpirySeconds;
    if (!encoder.add(expiration)) { return; }

    if (!encoder.EndList()) { return; }

    auto bytes_result = encoder.MoveBytes();
    if (!bytes_result) { return; }

    // Prepend packet-type byte
    std::vector<uint8_t> payload;
    payload.reserve(kPacketTypeSize + bytes_result.value().size());
    payload.push_back(kPacketTypePong);
    payload.insert(payload.end(), bytes_result.value().begin(), bytes_result.value().end());

    auto signed_packet = sign_packet(payload);
    if (!signed_packet) { return; }

    const std::string ip   = sender.address().to_string();
    const uint16_t    port = sender.port();
    asio::spawn(io_context_,
        [this, ip, port, pkt = std::move(signed_packet.value())](asio::yield_context yield)
        {
            const udp::endpoint dest(asio::ip::make_address(ip), port);
            auto send_result = send_packet(pkt, dest, yield);
            (void)send_result;

            // Bond is now complete — send FIND_NODE asking for peers
            // closest to our own node ID
            logger_->debug("Bond complete with {}:{} — sending FIND_NODE", ip, port);
            auto find_result = find_node(ip, port, config_.public_key, yield);
            (void)find_result;
        });
}

void discv4_client::handle_pong(const uint8_t* data, size_t length, const udp::endpoint& sender) {
    if (length < kPacketHeaderSize) {
        return;
    }

    // discv4_pong::Parse expects the full wire packet (hash || sig || type || rlp)
    const rlp::ByteView full_packet(data, length);
    auto pong_result = discv4_pong::Parse(full_packet);
    if (!pong_result) {
        if (error_callback_) {
            error_callback_("Failed to parse PONG packet");
        }
        return;
    }

    // PONG received — bond will complete when we respond to the bootnode's
    // return PING.  FIND_NODE is sent from handle_ping after our PONG is sent.
    logger_->debug("PONG from {}:{}", sender.address().to_string(), sender.port());
}

void discv4_client::handle_find_node(const uint8_t* data, size_t length, const udp::endpoint& sender) {
    // TODO: Implement FIND_NODE handling — respond with NEIGHBOURS
    (void)data;
    (void)length;
    (void)sender;
}

void discv4_client::handle_neighbours(const uint8_t* data, size_t length, const udp::endpoint& sender) {
    (void)sender;

    if (length < kPacketHeaderSize) {
        return;
    }

    const rlp::ByteView raw(data + kPacketHeaderSize, length - kPacketHeaderSize);
    rlp::RlpDecoder decoder(raw);

    // Outer list header
    auto outer_len_result = decoder.ReadListHeaderBytes();
    if (!outer_len_result) {
        return;
    }

    // Nodes list header
    auto nodes_len_result = decoder.ReadListHeaderBytes();
    if (!nodes_len_result) {
        return;
    }

    const rlp::ByteView after_nodes_start = decoder.Remaining();
    const size_t        nodes_byte_len    = nodes_len_result.value();

    while (!decoder.IsFinished())
    {
        const size_t consumed = after_nodes_start.size() - decoder.Remaining().size();
        if (consumed >= nodes_byte_len) {
            break;
        }

        // Each node is a flat list: [ ip(4), udp, tcp, pubkey(64) ]
        // (go-ethereum RpcNode — no endpoint sub-list wrapper)
        auto node_len_result = decoder.ReadListHeaderBytes();
        if (!node_len_result) {
            break;
        }
        const rlp::ByteView node_start = decoder.Remaining();
        const size_t        node_len   = node_len_result.value();

        rlp::Bytes ip_bytes;
        if (!decoder.read(ip_bytes) || ip_bytes.size() != kIPv4Size) {
            break;
        }

        uint16_t udp_port = 0;
        if (!decoder.read(udp_port)) {
            break;
        }

        uint16_t tcp_port = 0;
        if (!decoder.read(tcp_port)) {
            break;
        }

        rlp::Bytes pubkey_bytes;
        if (!decoder.read(pubkey_bytes) || pubkey_bytes.size() != kNodeIdSize) {
            break;
        }

        // Skip any remaining fields in this node (e.g. per-node expiry)
        const size_t node_consumed = node_start.size() - decoder.Remaining().size();
        if (node_consumed < node_len) {
            const size_t        remaining_in_node = node_len - node_consumed;
            const rlp::ByteView skip_view         = decoder.Remaining().substr(0, remaining_in_node);
            rlp::RlpDecoder     skip_decoder(skip_view);
            while (!skip_decoder.IsFinished()) {
                if (!skip_decoder.SkipItem()) { break; }
            }
            const size_t        actually_skipped = skip_view.size() - skip_decoder.Remaining().size();
            const rlp::ByteView after_node       = decoder.Remaining().substr(actually_skipped);
            decoder = rlp::RlpDecoder(after_node);
        }

        if (tcp_port == 0) {
            continue;
        }

        DiscoveredPeer peer;
        std::copy(pubkey_bytes.begin(), pubkey_bytes.end(), peer.node_id.begin());
        peer.ip = std::to_string(ip_bytes[0]) + "." +
                  std::to_string(ip_bytes[1]) + "." +
                  std::to_string(ip_bytes[2]) + "." +
                  std::to_string(ip_bytes[3]);
        peer.udp_port  = udp_port;
        peer.tcp_port  = tcp_port;
        peer.last_seen = std::chrono::steady_clock::now();

        {
            const std::lock_guard<std::mutex> lock(peers_mutex_);
            std::string key;
            key.reserve(kNodeIdHexSize);
            for (const uint8_t byte : peer.node_id) {
                const char* hex_chars = "0123456789abcdef";
                key += hex_chars[byte >> 4];
                key += hex_chars[byte & 0x0fu];
            }
            peers_[key] = peer;
        }

        if (peer_callback_) {
            logger_->debug("Neighbour peer: {}:{}", peer.ip, peer.tcp_port);
            peer_callback_(peer);
        }
    }
}


discv4::Result<discv4_pong> discv4_client::ping(
    const std::string& ip,
    uint16_t port,
    const NodeId& /*node_id*/,
    asio::yield_context yield
) {
    // Create PING packet
    discv4_ping ping_packet(
        config_.bind_ip, config_.bind_port, config_.tcp_port,
        ip, port, port
    );

    auto payload = ping_packet.RlpPayload();
    if (payload.empty()) {
        return discv4Error::kRlpPayloadEmpty;
    }

    // Sign the packet
    auto signed_packet = sign_packet(payload);
    if (!signed_packet) {
        return discv4Error::kSigningFailed;
    }

    // Send packet
    udp::endpoint destination(asio::ip::make_address(ip), port);
    auto send_result = send_packet(signed_packet.value(), destination, yield);
    if (!send_result) {
        return discv4Error::kNetworkSendFailed;
    }

    // Wait for PONG response (simplified - in production use proper async waiting)
    // For now, return success - PONG will be handled in receive_loop
    return discv4Error::kPongTimeout;  // Placeholder
}

rlpx::VoidResult discv4_client::find_node(
    const std::string& ip,
    uint16_t port,
    const NodeId& target_id,
    asio::yield_context yield
) {
    // FIND_NODE payload: packet-type(0x03) || RLP([target(64), expiration])
    rlp::RlpEncoder encoder;
    if (!encoder.BeginList()) {
        return rlp::outcome::success();
    }
    if (!encoder.add(rlp::ByteView(target_id.data(), target_id.size()))) {
        return rlp::outcome::success();
    }
    const uint32_t expiration = static_cast<uint32_t>(std::time(nullptr)) + kPacketExpirySeconds;
    if (!encoder.add(expiration)) {
        return rlp::outcome::success();
    }
    if (!encoder.EndList()) {
        return rlp::outcome::success();
    }

    auto bytes_result = encoder.MoveBytes();
    if (!bytes_result) {
        return rlp::outcome::success();
    }

    // Prepend packet type byte
    std::vector<uint8_t> payload;
    payload.reserve(kWirePacketTypeSize + bytes_result.value().size());
    payload.push_back(kPacketTypeFindNode);
    payload.insert(payload.end(),
                   bytes_result.value().begin(),
                   bytes_result.value().end());

    auto signed_packet = sign_packet(payload);
    if (!signed_packet) {
        return rlp::outcome::success();
    }

    const udp::endpoint destination(asio::ip::make_address(ip), port);
    auto send_result = send_packet(signed_packet.value(), destination, yield);
    (void)send_result;
    return rlp::outcome::success();
}

discv4::Result<void> discv4_client::send_packet(
    const std::vector<uint8_t>& packet,
    const udp::endpoint& destination,
    asio::yield_context yield
) {
    boost::system::error_code ec;

    socket_.async_send_to(
        asio::buffer(packet),
        destination,
        asio::redirect_error(yield, ec)
    );

    if (ec) {
        if (error_callback_) {
            error_callback_("Send error: " + ec.message());
        }
        return discv4Error::kNetworkSendFailed;
    }

    return rlp::outcome::success();
}

discv4::Result<std::vector<uint8_t>> discv4_client::sign_packet(const std::vector<uint8_t>& payload) {
    // payload = packet-type(1) || RLP(packet-data)
    //
    // Correct discv4 packet construction (EIP-8 / go-ethereum discv4):
    //   sig  = sign( keccak256( packet-type || packet-data ) )   [65 bytes, recoverable]
    //   hash = keccak256( sig || packet-type || packet-data )    [32 bytes]
    //   wire = hash || sig || packet-type || packet-data

    // Step 1: sign keccak256(payload)
    const auto msg_hash = discv4_packet::Keccak256(payload);

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    if (!ctx) {
        return discv4Error::kContextCreationFailed;
    }

    secp256k1_ecdsa_recoverable_signature sig;
    if (!secp256k1_ecdsa_sign_recoverable(ctx, &sig, msg_hash.data(),
                                          config_.private_key.data(), nullptr, nullptr)) {
        secp256k1_context_destroy(ctx);
        return discv4Error::kSigningFailed;
    }

    std::array<uint8_t, kSigSize> sig_bytes{};
    int recid = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(
        ctx, sig_bytes.data(), &recid, &sig);
    sig_bytes[kSigSize - 1] = static_cast<uint8_t>(recid);

    secp256k1_context_destroy(ctx);

    // Step 2: outer hash = keccak256( sig || payload )
    std::vector<uint8_t> to_outer_hash;
    to_outer_hash.reserve(kSigSize + payload.size());
    to_outer_hash.insert(to_outer_hash.end(), sig_bytes.begin(), sig_bytes.end());
    to_outer_hash.insert(to_outer_hash.end(), payload.begin(), payload.end());
    const auto outer_hash = discv4_packet::Keccak256(to_outer_hash);

    // Step 3: assemble wire packet
    std::vector<uint8_t> packet;
    packet.reserve(kHashSize + kSigSize + payload.size());
    packet.insert(packet.end(), outer_hash.begin(), outer_hash.end());
    packet.insert(packet.end(), sig_bytes.begin(), sig_bytes.end());
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;
}

discv4::Result<NodeId> discv4_client::verify_packet(const uint8_t* data, size_t length) {
    if (length < kPacketHeaderSize) {
        return discv4Error::kPacketTooSmall;
    }

    // Wire layout: hash(kHashSize) || sig(kSigSize) || type(1) || packet-data
    const uint8_t* hash_data      = data;
    const uint8_t* sig_data       = data + kHashSize;
    const uint8_t* payload_data   = data + kHashSize + kSigSize;
    const size_t   payload_length = length - kHashSize - kSigSize;

    // Verify outer hash = keccak256( sig || payload )
    std::vector<uint8_t> to_outer_hash;
    to_outer_hash.reserve(kSigSize + payload_length);
    to_outer_hash.insert(to_outer_hash.end(), sig_data,     sig_data + kSigSize);
    to_outer_hash.insert(to_outer_hash.end(), payload_data, payload_data + payload_length);
    const auto computed_hash = discv4_packet::Keccak256(to_outer_hash);

    if (std::memcmp(hash_data, computed_hash.data(), kHashSize) != 0) {
        return discv4Error::kHashMismatch;
    }

    // Recover public key: sig was made over keccak256( payload )
    const std::vector<uint8_t> payload_vec(payload_data, payload_data + payload_length);
    const auto msg_hash = discv4_packet::Keccak256(payload_vec);

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    if (!ctx) {
        return discv4Error::kContextCreationFailed;
    }

    secp256k1_ecdsa_recoverable_signature sig;
    const int recid = static_cast<int>(sig_data[kSigSize - 1]);
    if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &sig, sig_data, recid)) {
        secp256k1_context_destroy(ctx);
        return discv4Error::kSignatureParseFailed;
    }

    secp256k1_pubkey pubkey;
    if (!secp256k1_ecdsa_recover(ctx, &pubkey, &sig, msg_hash.data())) {
        secp256k1_context_destroy(ctx);
        return discv4Error::kSignatureRecoveryFailed;
    }

    std::array<uint8_t, kUncompressedPubKey> pubkey_bytes{};
    size_t pubkey_len = kUncompressedPubKey;
    secp256k1_ec_pubkey_serialize(ctx, pubkey_bytes.data(), &pubkey_len,
                                  &pubkey, SECP256K1_EC_UNCOMPRESSED);
    secp256k1_context_destroy(ctx);

    // Skip the 0x04 uncompressed-point prefix byte
    NodeId node_id;
    std::copy(pubkey_bytes.begin() + kPacketTypeSize, pubkey_bytes.end(), node_id.begin());
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

