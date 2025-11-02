// discv4_ping.cpp

#include <rlp/PeerDiscovery/Discv4Ping.hpp>

namespace discv4 {

std::vector<uint8_t> Discv4Ping::RlpPayload()
{
    rlp::RlpEncoder encoder;
    if (auto res = encoder.BeginList(); !res) {
        return std::vector<uint8_t>(); // Return empty on error
    }
    if (auto res = encoder.add( Version() ); !res) {
        return std::vector<uint8_t>();
    }
    auto from_encoded = fromEp.encode();
    if (!from_encoded) {
        return std::vector<uint8_t>();
    }
    if (auto res = encoder.AddRaw( from_encoded.value() ); !res) {
        return std::vector<uint8_t>();
    }
    auto to_encoded = toEp.encode();
    if (!to_encoded) {
        return std::vector<uint8_t>();
    }
    if (auto res = encoder.AddRaw( to_encoded.value() ); !res) {
        return std::vector<uint8_t>();
    }
    uint32_t now = static_cast<std::uint32_t>( std::time( nullptr ) );
    uint32_t expire_in_1_minute = now + 60;
    if (auto res = encoder.add( expire_in_1_minute ); !res) {
        return std::vector<uint8_t>();
    }
    if (auto res = encoder.EndList(); !res) {
        return std::vector<uint8_t>();
    }

    auto bytes_result = encoder.MoveBytes();
    if (!bytes_result) {
        return std::vector<uint8_t>(); // Return empty on error
    }
    rlp::Bytes bytes = std::move(bytes_result.value());
    bytes.insert( bytes.begin(), PacketType() );
    return std::vector<uint8_t>( bytes.begin(), bytes.end() );
}

} // namespace discv4
