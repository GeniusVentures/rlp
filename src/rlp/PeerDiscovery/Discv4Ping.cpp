// discv4_ping.cpp

#include <rlp/PeerDiscovery/Discv4Ping.hpp>

namespace discv4 {

std::vector<uint8_t> Discv4Ping::rlp_payload() {
    rlp::RlpEncoder encoder;
    encoder.begin_list();
    encoder.add(version());
    encoder.addRaw(from_ep.encode());
    encoder.addRaw(to_ep.encode());
    auto data = from_ep.encode();
    uint32_t now = static_cast<std::uint32_t>(std::time(nullptr));
    uint32_t expire_in_1_minute = now + 60;
    encoder.add(expire_in_1_minute);
    encoder.end_list();

    rlp::Bytes bytes = encoder.move_bytes();
    bytes.insert(bytes.begin(), packet_type());
    return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

} // namespace discv4
