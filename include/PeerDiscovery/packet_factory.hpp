// packet_factory.h
#ifndef PACKET_FACTORY_H
#define PACKET_FACTORY_H

#include <memory>
#include <vector>
#include <functional>

#include <boost/asio.hpp>

// Boost.Asio
namespace asio = boost::asio;
using udp = asio::ip::udp;

// RLP
#include <rlp_encoder.hpp>
#include <rlp_decoder.hpp>

// nil::crypto3
#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/sha2.hpp>
#include <nil/crypto3/hash/keccak.hpp>

namespace discv4 {

// Forward declarations
class Discv4Packet;

using SendCallback = std::function<void(const std::vector<uint8_t>&, const udp::endpoint&)>;

class PacketFactory {
public:
    // Send Ping and await Pong asynchronously
    static void send_ping_and_wait(
        asio::io_context& io,
        const std::string& from_ip, uint16_t f_udp, uint16_t f_tcp,
        const std::string& to_ip,   uint16_t t_udp, uint16_t t_tcp,
        const std::vector<uint8_t>& priv_key_hex,
        SendCallback callback);

private:
    static void sign_and_build_packet(
        Discv4Packet* packet,
        const std::vector<uint8_t>& priv_key_hex,
        std::vector<uint8_t>& out);

    static void send_packet(
        asio::ip::udp::socket& socket,
        const std::vector<uint8_t>& msg,
        const udp::endpoint& target);
};

} // namespace discv4

#endif // PACKET_FACTORY_H
