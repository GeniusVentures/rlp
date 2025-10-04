// discv4_packet.cpp
#include <PeerDiscovery/Discv4Packet.hpp>
#include <stdexcept>

namespace discv4 {

// Forward declaration
class Discv4Ping;

// ----------------------------
//  keccak_256 (using nil::crypto3)
// ----------------------------
std::array<uint8_t, 32> Discv4Packet::keccak_256(const std::vector<uint8_t>& payload) {
    auto hash = nil::crypto3::hash<nil::crypto3::hashes::keccak_1600<256>>(payload.cbegin(), payload.cend());
    std::array<uint8_t, 32> hash_array = hash;
    return hash_array;
}

// ----------------------------
//  validate_hash
// ----------------------------
bool Discv4Packet::validate_hash(const std::vector<uint8_t>& payload, const uint8_t* hash) {
    std::array<uint8_t, 32> computed = keccak_256(payload);
    return std::equal(computed.begin(), computed.end(), hash);
}

} // namespace discv4
