// discv4_ping.cpp

#include <rlp/PeerDiscovery/Discv4Ping.hpp>

namespace discv4 {

std::vector<uint8_t> Discv4Ping::RlpPayload()
{
    rlp::RlpEncoder encoder;
    encoder.BeginList();
    encoder.add( Version() );
    encoder.AddRaw( fromEp.encode() );
    encoder.AddRaw( toEp.encode() );
    auto data = fromEp.encode();
    uint32_t now = static_cast<std::uint32_t>( std::time( nullptr ) );
    uint32_t expire_in_1_minute = now + 60;
    encoder.add( expire_in_1_minute );
    encoder.EndList();

    auto bytes_result = encoder.MoveBytes();
    if (!bytes_result) {
        return std::vector<uint8_t>(); // Return empty on error
    }
    rlp::Bytes bytes = std::move(bytes_result.value());
    bytes.insert( bytes.begin(), PacketType() );
    return std::vector<uint8_t>( bytes.begin(), bytes.end() );
}

} // namespace discv4
