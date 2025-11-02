#include <boost/asio.hpp>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <rlp/common.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <thread>


#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/sha2.hpp>
#include <nil/crypto3/hash/keccak.hpp>

boost::system::error_code ec;

namespace asio = boost::asio;
using udp = asio::ip::udp;
// Types
using NodeID = std::vector<uint8_t>; // 512-bit (64 bytes) SHA3-256 of public key
using IPAddress = std::string;
constexpr uint16_t DEFAULT_DISCV4_PORT = 30303;
using namespace boost;


// Test class for NodeID Hash
struct NodeIDHash {
    std::size_t operator()(const NodeID& id) const {
        std::size_t hash = 0xcbf29ce484222325ULL;
        for ( uint8_t byte : id ) {
            hash ^= byte;
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }
};

// WIP Untested code
struct Peer {
    std::string ip;
    uint16_t udp_port = DEFAULT_DISCV4_PORT;
    uint16_t tcp_port = 30303; // Default, may be updated via PONG
    NodeID node_id;
    std::chrono::steady_clock::time_point last_seen;

    // For Kademlia: XOR distance
    size_t xor_distance(const NodeID& other) const {
        size_t dist = 0;
        for ( size_t i = 0; i < std::min(node_id.size(), other.size()); ++i ) {
            dist ^= (node_id[i] ^ other[i]);
        }
        return dist;
    }
};

    // Encode PING message (RLP)
rlp::Bytes EncodePing(const NodeID& target_id);

// WIP Untested code
class Discv4Discovery {
public:
    explicit Discv4Discovery() : io_context_(1), socket_(io_context_, asio::ip::udp::v4()) {
        socket_.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 53093), ec);

        // Initialize secp256k1 context
        ctx_ = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

        // Bootstrap nodes (Ethereum Mainnet)
        bootstrap_nodes_ = {
            {"45.76.138.208", 30303, "a571c194e8b2f0369d4bc105a87726e7c98b7f9d3412925ff3a0e4c668d4f7b0149d50239a8e7da6fd7f6c310b4d3325dc8a901b7f61e8c34dabbc2359dc79d0"},
            {"5.179.48.203", 30303, "5e9d7c8a2fb164aa361b3a0f580e58972c1d4e96e353a90f850e6b72f124d2c9bc6aef23be8e0b7bb51b4d0c9f0a67d39e28efee31b5ecde4029f3b7c1a6d8dc"},
            {"157.90.35.166", 30303, "4aeb4ab6c14b23e2c4cfdce879c04b0748a20d8e9b59e25ded2a08143e265c6c25936e74cbc8e641e3312ca288673d91f2f93f8e277de3cfa444ecdaaf982052"}
        };
    }

    ~Discv4Discovery() {
        if ( ctx_ ) secp256k1_context_destroy(ctx_);
    }

private:
    asio::io_context io_context_;
    asio::ip::udp::socket socket_;
    secp256k1_context* ctx_;

    // Bootstrap nodes (IP, port, enode ID)
    struct BootstrapNode {
        std::string ip;
        uint16_t port;
        std::string node_id_hex;
    };
    std::vector<BootstrapNode> bootstrap_nodes_;

    // Local peer table (Kademlia)
    std::unordered_map<NodeID, Peer, NodeIDHash> peers_;

    // Send PING to a target
    void SendPing(const NodeID& target_id, const std::string& ip, uint16_t port) {
        // Prepare and send valid Discv4 packet
        // Test version is in EncodePing function
    }



    // Generate your own node ID from public key (SHA3-256 of pubkey)
    NodeID GetNodeID() {
        // In real code: use secp256k1 to generate keypair
        // For demo: fake ID (64 bytes)
        return std::vector<uint8_t>(64, 0x12); // Placeholder
    }

    // Handle incoming packet (PONG, NEIGHBOURS)
    void HandlePacket(const uint8_t* data, size_t len) {
        // TODO: Parse RLP
        std::vector<uint8_t> packet(data, data + len);

        // Simulate parsing
        if ( len > 3 && packet[0] == 0xc2 ) { // RLP PONG
            std::cout << "Received PONG (simulated)\n";
        } else if ( len > 3 && packet[0] == 0xc4 ) { // RLP NEIGHBOURS
       
        }
        std::cout << "Received NEIGHBOURS (simulated)\n";
    }

    // Run discovery loop
    void Run() {
        std::cout << "Starting Discv4 Discovery...\n";

        // Bootstrap: send PING to all bootstrap nodes
        for ( const auto& node : bootstrap_nodes_ ) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            SendPing(ParseNodeID(node.node_id_hex), node.ip, node.port);
        }

        // Wait for the timer or any incoming packet
        while ( true ) {
            std::vector<uint8_t> buffer(1024);
            asio::ip::udp::endpoint sender_endpoint;

            // Use async_wait to check if timer expired OR packet arrived
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

            // Wait for either a packet or the timer
            try {
                std::future<void> future = std::async(std::launch::async, [&]() {
                    socket_.receive_from(asio::buffer(buffer), sender_endpoint, 0, ec);
                    if ( ec ) { std::cerr << "bind error: " << ec.message() << '\n'; }
                });

                // Wait for 15 seconds or until packet arrives
                if ( future.wait_for(std::chrono::seconds(15)) == std::future_status::ready ) {
                    // Packet arrived
                    HandlePacket(buffer.data(), buffer.size());
                } else {
                    // Timer expired — exit loop
                    std::cout << "15 seconds elapsed. Stopping discovery.\n";
                    break;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
                break;
            }
        }

        // Cleanup
        std::cout << "Discovery finished.\n";
    }

    // Parse hex string to NodeID
    static NodeID ParseNodeID(const std::string& hex) {
        std::vector<uint8_t> id;
        for ( size_t i = 0; i < hex.size(); i += 2 ) {
            std::string byte_str = hex.substr(i, 2);
            char* end;
            uint8_t byte = static_cast<uint8_t>(std::strtol(byte_str.c_str(), &end, 16));
            id.push_back(byte);
        }
        return id;
    }
};

rlp::Bytes EncodeEndpoint(rlp::ByteView ip, uint16_t udpPort, uint16_t tcpPort) {
    rlp::RlpEncoder encoder;
    encoder.BeginList();
    encoder.add(ip);
    encoder.add(udpPort);
    encoder.add(tcpPort);
    encoder.EndList();
    auto result = encoder.MoveBytes();
    if (!result) return rlp::Bytes{};
    rlp::Bytes endpoint_msg = std::move(result.value());

    return endpoint_msg;
}

rlp::Bytes EncodePing() {
    uint8_t packet_type = 0x01;

    uint8_t from_ip_bytes[4];
    uint8_t to_ip_bytes[4];
    inet_pton(AF_INET, "10.0.2.15", from_ip_bytes);

    // Hardcoded Eth Boot Node IP
    inet_pton(AF_INET, "146.190.13.128", to_ip_bytes);

    // TODO Find better format to pass IPv4
    rlp::ByteView sv_from(
        from_ip_bytes,
        sizeof(from_ip_bytes)
    );
    rlp::ByteView sv_to(
        to_ip_bytes,
        sizeof(to_ip_bytes)
    );

    uint8_t version = {0x04}; // Discv4
    rlp::Bytes endpoint_from = EncodeEndpoint(sv_from, 30303, 30303);
    rlp::Bytes endpoint_to = EncodeEndpoint(sv_to, 30303, 30303);

    uint32_t now = static_cast<std::uint32_t>(std::time(nullptr));
    uint32_t expire_in_1_minute = now + 60;

    rlp::RlpEncoder encoder;
    encoder.BeginList();
    encoder.add(version);
    encoder.AddRaw(rlp::ByteView(endpoint_from));
    encoder.AddRaw(rlp::ByteView(endpoint_to));
    encoder.add(expire_in_1_minute);
    encoder.EndList();
    auto result = encoder.MoveBytes();
    if (!result) return rlp::Bytes{};
    rlp::Bytes ping_msg = std::move(result.value());

    ping_msg.insert(ping_msg.begin(), packet_type);

    return ping_msg;
}

int test_ping()
{
    try {
        auto packet = EncodePing();
        auto hash_result = nil::crypto3::hash<nil::crypto3::hashes::keccak_1600<256>>(packet.begin(), packet.end());
        std::array<uint8_t, 32> hash_array = hash_result;

        // Create context for signing (only sign operations needed)
        secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
        if ( !ctx ) {
            std::cerr << "failed to create secp context\n";
            return 1;
        }

        // TODO Generate new and valid seckey
        const unsigned char seckey[32] = {
            0xe6, 0xb1, 0x81, 0x2f, 0x04, 0xe3, 0x45, 0x19,
            0x00, 0x43, 0x4f, 0x5a, 0xbd, 0x33, 0x03, 0xb5,
            0x3d, 0x28, 0x4b, 0xd4, 0x2f, 0x42, 0x5c, 0x07,
            0x61, 0x0a, 0x82, 0xc4, 0x2b, 0x8d, 0x29, 0x77
        };

        // Validate secret key
        if ( !secp256k1_ec_seckey_verify(ctx, seckey) ) {
            std::cerr << "invalid seckey\n";
            secp256k1_context_destroy(ctx);
            return 1;
        }

        // Sign recoverably
        secp256k1_ecdsa_recoverable_signature sig;
        int sign_ok = secp256k1_ecdsa_sign_recoverable(
            ctx,
            &sig,
            hash_array.data(),
            seckey,
            nullptr, // optional nonce function
            nullptr  // optional data for nonce function
        );
        if ( !sign_ok ) {
            std::cerr << "sign failed\n";
            secp256k1_context_destroy(ctx);
            return 1;
        }

        // Serialize compact (64 bytes) + recovery id
        unsigned char sig64[64];
        int recid = -1;
        secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, sig64, &recid, &sig);
        if ( recid < 0 || recid > 3 ) {
            std::cerr << "unexpected recid\n";
            secp256k1_context_destroy(ctx);
            return 1;
        }

        std::vector<uint8_t> sigvec(sig64, sig64 + 64);
        std::vector<uint8_t> msg_cpp;
        msg_cpp.reserve(32 + 64 + 1 + packet.size());
        msg_cpp.insert(msg_cpp.end(), 32, 0);
        msg_cpp.insert(msg_cpp.end(), sigvec.begin(), sigvec.end());
        msg_cpp.insert(msg_cpp.end(), recid);
        msg_cpp.insert(msg_cpp.end(), packet.begin(), packet.end());
        auto payload_hash = nil::crypto3::hash<nil::crypto3::hashes::keccak_1600<256>>(msg_cpp.begin() + 32, msg_cpp.end());
        std::array<uint8_t, 32> payload_array = payload_hash;
        std::copy(payload_array.begin(), payload_array.end(), msg_cpp.begin());
    
        asio::io_context io;
        udp::socket socket(io, udp::v4());

        // 1. Bind to the local port you want
        socket.bind(udp::endpoint(udp::v4(), 53093));

        // 2. Send a packet

        udp::endpoint target(asio::ip::address_v4::from_string("157.90.35.166"), 30303);
        socket.send_to(asio::buffer(msg_cpp), target);

        // 3. Receive the reply
        std::array<char, 2048> recv_buf;
        udp::endpoint sender_endpoint;
        boost::system::error_code ec;

        // TODO Parse PONG and validate it's integrity
        std::size_t n = socket.receive_from(asio::buffer(recv_buf), sender_endpoint, 0, ec);
        if ( ec ) {
            std::cerr << "receive error: " << ec.message() << '\n';
        } else {
            std::cout << "received " << n << " bytes from "
                      << sender_endpoint.address() << ":"
                      << sender_endpoint.port() << '\n';
        }
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << '\n';
    }

    return 0;
}