// discv4_packet.cpp
#include <rlp/PeerDiscovery/Discv4Packet.hpp>
#include <utility>

namespace discv4 {

Discv4Packet::Discv4Packet( uint8_t packetType, uint8_t version, std::string name )
    : packetType_( packetType ), version_( version ), name_( std::move( name ) ) {}

// ----------------------------
//  Keccak256 (using nil::crypto3)
// ----------------------------
std::array<uint8_t, 32> Discv4Packet::Keccak256( const std::vector<uint8_t>& payload )
{
    auto hash = nil::crypto3::hash<nil::crypto3::hashes::keccak_1600<256>>( payload.cbegin(), payload.cend() );
    std::array<uint8_t, 32> hashArray = hash;
    return hashArray;
}

// ----------------------------
//  ValidateHash
// ----------------------------
bool Discv4Packet::ValidateHash( const std::vector<uint8_t>& payload, const uint8_t* hash )
{
    std::array<uint8_t, 32> computed = Keccak256( payload );
    return std::equal( computed.begin(), computed.end(), hash );
}

} // namespace discv4
