// discv4_packet.h
#ifndef RLP_PEERDISCOVERY_DISCV4_PACKET_HPP
#define RLP_PEERDISCOVERY_DISCV4_PACKET_HPP

#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>

// SuperGenius RLP
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>

// nil::crypto3 keccak
#include <nil/crypto3/hash/algorithm/hash.hpp>
#include <nil/crypto3/hash/sha2.hpp>
#include <nil/crypto3/hash/keccak.hpp>

// Boost.Asio
namespace asio = boost::asio;
using udp = asio::ip::udp;

namespace discv4 {

// Forward declaration
class Discv4Packet;

/**
 * @brief Base class for all Discovery V4 packets
 */
class Discv4Packet
{
public:
    virtual ~Discv4Packet() = default;

    // Return the RLP-encoded payload (bytes)
    virtual std::vector<uint8_t> RlpPayload() = 0;

    // Return the packet type (e.g., 0x01 for Ping)
    uint8_t PacketType() const noexcept { return packetType_; }

    // Return the protocol version (e.g., 0x04)
    uint8_t Version() const noexcept { return version_; }

    // Return the name of this packet type
    const std::string& Name() const { return name_; }

    // Static helper: validate packet hash (used to verify incoming packets)
    static bool ValidateHash( const std::vector<uint8_t>& payload, const uint8_t* hash );

    // Static helper: get Keccak-256 digest
    static std::array<uint8_t, 32> Keccak256( const std::vector<uint8_t>& payload );

protected:
    Discv4Packet( uint8_t packetType, uint8_t version, std::string name );

private:
    uint8_t packetType_;
    uint8_t version_;
    std::string name_;
};

} // namespace discv4

#endif // RLP_PEERDISCOVERY_DISCV4_PACKET_HPP
