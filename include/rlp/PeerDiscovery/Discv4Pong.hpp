// discv4_pong.h
#ifndef RLP_PEERDISCOVERY_DISCV4_PONG_HPP
#define RLP_PEERDISCOVERY_DISCV4_PONG_HPP

#include <vector>
#include <array>
#include <stdint.h>

#include <rlp/common.hpp>
#include <boost/outcome/try.hpp>
#include <rlp/rlp_decoder.hpp>

namespace discv4 {

struct Discv4Pong
{
    // to_endpoint: [ip, udp_port, tcp_port]
    struct Endpoint 
    {
        std::array<uint8_t, 4> ip;  // IPv4 address
        uint16_t udpPort = 0;
        uint16_t tcpPort = 0;
    } toEndpoint;
    
    std::array<uint8_t, 32> pingHash;  // Hash of the original PING packet
    uint32_t expiration = 0;            // Unix timestamp
    uint64_t ersErq = 0;               // Optional ENR sequence number of the sender

    static rlp::Result<Discv4Pong> Parse( rlp::ByteView raw );

    static rlp::DecodingResult ParseEndpoint( rlp::RlpDecoder& decoder, Discv4Pong::Endpoint& endpoint );
};

} // namespace discv4

#endif // RLP_PEERDISCOVERY_DISCV4_PONG_HPP
