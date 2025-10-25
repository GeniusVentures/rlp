// packet_factory.cpp
#include <PeerDiscovery/packet_factory.hpp>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <iostream>
#include <boost/outcome/try.hpp>

#include <PeerDiscovery/Discv4Ping.hpp>
#include <PeerDiscovery/Discv4Pong.hpp>

namespace discv4 {

PacketResult PacketFactory::send_ping_and_wait(
    asio::io_context& io,
    const std::string& from_ip, uint16_t f_udp, uint16_t f_tcp,
    const std::string& to_ip,   uint16_t t_udp, uint16_t t_tcp,
    const std::vector<uint8_t>& priv_key_hex,
    SendCallback callback) {

    auto ping = std::make_unique<Discv4Ping>(from_ip, f_udp, f_tcp, to_ip, t_udp, t_tcp);

    std::vector<uint8_t> msg;
    auto sign_result = sign_and_build_packet(ping.get(), priv_key_hex, msg);
    if (!sign_result) {
        std::cerr << "Failed to sign and build packet: " << sign_result.error().message() << std::endl;
        return outcome::failure(sign_result.error());
    }

    // Create socket
    udp::socket socket(io, udp::v4());
    socket.set_option(udp::socket::reuse_address(true));
    socket.bind(udp::endpoint(boost::asio::ip::address_v4::any(), 53093));

    // Send
    udp::endpoint target(boost::asio::ip::address_v4::from_string(to_ip), t_udp);
    udp::endpoint sender;
    send_packet(socket, msg, target);

    rlp::ByteView msb_bv(msg.data(), msg.size());
    std::cout<<"Sending PING: "<<rlp::hexToString(msb_bv)<<"\n\n";

    // Receive async
    std::array<uint8_t, 2048> arrayBuffer;
    boost::system::error_code ec;
    size_t bytes_transferred = socket.receive_from(boost::asio::buffer(arrayBuffer), sender, 0, ec);

    if (!ec) {
        std::cout<<"Received "<<bytes_transferred<<" bytes\n\n";
        std::vector<uint8_t> data(arrayBuffer.data(), arrayBuffer.data() + bytes_transferred);
        callback(data, sender);
    }

    // Run io_context (in a loop)
    io.run();

    return outcome::success();
}

PacketResult PacketFactory::sign_and_build_packet(
    Discv4Packet* packet,
    const std::vector<uint8_t>& priv_key_hex,
    std::vector<uint8_t>& out) {

    if (packet == nullptr) {
        return outcome::failure(PacketError::kNullPacket);
    }

    auto payload = packet->rlp_payload();

    // Hash with keccak-256
    std::array<uint8_t, 32> hash = Discv4Packet::keccak_256(payload);

    // Sign with secp256k1
    auto ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_ecdsa_recoverable_signature sig;
    int success = secp256k1_ecdsa_sign_recoverable(ctx, &sig, hash.data(), priv_key_hex.data(),
                                                  secp256k1_nonce_function_rfc6979, nullptr);


    if (!success) {
        secp256k1_context_destroy(ctx);
        return outcome::failure(PacketError::kSignFailure);
    }

    uint8_t serialized[65];
    int recid;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, serialized, &recid, &sig);
    secp256k1_context_destroy(ctx);

    out.reserve(32 + 65 + payload.size());
    out.insert(out.end(), 32, 0); // Skip for hash for whole out message
    out.insert(out.end(), serialized, serialized + 64);
    out.push_back(recid);
    out.insert(out.end(), payload.begin(), payload.end());

    // Hash for whole out message
    auto payload_hash = nil::crypto3::hash<nil::crypto3::hashes::keccak_1600<256>>(out.begin() + 32, out.end());
    std::array<uint8_t, 32> payload_array = payload_hash;
    std::copy(payload_array.begin(), payload_array.end(), out.begin());

    return outcome::success();
}

void PacketFactory::send_packet(
    asio::ip::udp::socket& socket,
    const std::vector<uint8_t>& msg,
    const udp::endpoint& target) {
    socket.send_to(boost::asio::buffer(msg), target);
}

} // namespace discv4
