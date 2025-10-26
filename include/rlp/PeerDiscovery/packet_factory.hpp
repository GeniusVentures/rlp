// packet_factory.h
#ifndef RLP_PEERDISCOVERY_PACKET_FACTORY_HPP
#define RLP_PEERDISCOVERY_PACKET_FACTORY_HPP

#include <memory>
#include <vector>
#include <functional>

#include <boost/outcome/result.hpp>

#include <boost/asio.hpp>

// Boost.Asio
namespace asio = boost::asio;
using udp = asio::ip::udp;

// RLP
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>

// nil::crypto3
#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/sha2.hpp>
#include <nil/crypto3/hash/keccak.hpp>

namespace discv4 {

// Forward declarations
class Discv4Packet;

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

enum class PacketError {
    kNullPacket,
    kSignFailure,
};

using PacketResult = outcome::result<void, PacketError>;

using SendCallback = std::function<void(const std::vector<uint8_t>&, const udp::endpoint&)>;

class PacketFactory
{
public:
    // Send Ping and await Pong asynchronously
    static PacketResult SendPingAndWait(
        asio::io_context& io,
        const std::string& fromIp, uint16_t fUdp, uint16_t fTcp,
        const std::string& toIp, uint16_t tUdp, uint16_t tTcp,
        const std::vector<uint8_t>& privKeyHex,
        SendCallback callback );

private:
    static PacketResult SignAndBuildPacket(
        Discv4Packet* packet,
        const std::vector<uint8_t>& privKeyHex,
        std::vector<uint8_t>& out );

    static void SendPacket(
        asio::ip::udp::socket& socket,
        const std::vector<uint8_t>& msg,
        const udp::endpoint& target );
};

} // namespace discv4

#endif // RLP_PEERDISCOVERY_PACKET_FACTORY_HPP
