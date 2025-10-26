// discv4_ping.cpp

#include <rlp/PeerDiscovery/Discv4Ping.hpp>

namespace discv4 {

std::vector<uint8_t> Discv4Ping::RlpPayload()
{
    rlp::RlpEncoder encoder;
    encoder.begin_list();
    encoder.add( Version() );
    encoder.addRaw( fromEp.encode() );
    encoder.addRaw( toEp.encode() );
    auto data = fromEp.encode();
    uint32_t now = static_cast<std::uint32_t>( std::time( nullptr ) );
    uint32_t expire_in_1_minute = now + 60;
    encoder.add( expire_in_1_minute );
    encoder.end_list();

    rlp::Bytes bytes = encoder.move_bytes();
    bytes.insert( bytes.begin(), PacketType() );
    return std::vector<uint8_t>( bytes.begin(), bytes.end() );
}

} // namespace discv4
