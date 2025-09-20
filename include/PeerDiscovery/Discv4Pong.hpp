// discv4_pong.h
#ifndef DISCV4_PONG_H
#define DISCV4_PONG_H

#include <vector>
#include <array>
#include <stdint.h>

#include <common.hpp>
#include <boost/outcome/try.hpp>
#include <rlp_decoder.hpp>

namespace discv4 {

struct Discv4Pong {
    // to_endpoint: [ip, udp_port, tcp_port]
    struct Endpoint 
    {
        std::array<uint8_t, 4> ip;  // IPv4 address
        uint16_t udp_port = 0;
        uint16_t tcp_port = 0;
    } to_endpoint;
    
    std::array<uint8_t, 32> ping_hash;  // Hash of the original PING packet
    uint32_t expiration = 0;            // Unix timestamp
    uint64_t ers_erq = 0;               // Optional ENR sequence number of the sender

    static rlp::Result<Discv4Pong> parse(rlp::ByteView raw);

    static rlp::DecodingResult parse_endpoint(rlp::RlpDecoder& decoder, Discv4Pong::Endpoint& endpoint);
};

} // namespace discv4

#endif // DISCV4_PONG_H
