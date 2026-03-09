// discv4_ping.h
#ifndef RLP_PEERDISCOVERY_discv4_PING_HPP
#define RLP_PEERDISCOVERY_discv4_PING_HPP

#include "discv4/discv4_packet.hpp"
#include "discv4/discv4_constants.hpp"
#include "rlp/rlp_encoder.hpp"

namespace discv4 {

class discv4_ping : public discv4_packet
{
public:
    struct Endpoint
    {
        rlp::ByteView ipBv;
        uint8_t ipBytes[4];
        uint16_t udpPort = 0;
        uint16_t tcpPort = 0;

        Endpoint() = default;
        Endpoint( const std::string& ipStr, uint16_t udp, uint16_t tcp )
        {
            inet_pton( AF_INET, ipStr.c_str(), ipBytes );
            ipBv = rlp::ByteView( ipBytes, sizeof( ipBytes ) );
            udpPort = udp;
            tcpPort = tcp;
        }

        rlp::EncodingResult<rlp::Bytes> encode()
        {
            rlp::RlpEncoder encoder;
            if (auto res = encoder.BeginList(); !res) {
                return res.error();
            }
            if (auto res = encoder.add( ipBv ); !res) {
                return res.error();
            }
            if (auto res = encoder.add( udpPort ); !res) {
                return res.error();
            }
            if (auto res = encoder.add( tcpPort ); !res) {
                return res.error();
            }
            if (auto res = encoder.EndList(); !res) {
                return res.error();
            }
            auto bytes_result = encoder.MoveBytes();
            if (!bytes_result) {
                return bytes_result.error();
            }
            return std::move(bytes_result.value());
        }
    };

private:
    Endpoint fromEp;
    Endpoint toEp;
    uint32_t expires;

public:
    discv4_ping( const std::string& fromIp, uint16_t fUdp, uint16_t fTcp,
                const std::string& toIp, uint16_t tUdp, uint16_t tTcp )
        : discv4_packet( kPacketTypePing, kProtocolVersion, "Ping" ),
          fromEp( fromIp, fUdp, fTcp ), toEp( toIp, tUdp, tTcp )
    {
        expires = static_cast<uint32_t>( std::time( nullptr ) ) + kPacketExpirySeconds;
    }

    // Implement base methods
    std::vector<uint8_t> RlpPayload() override;

    // Getters
    const Endpoint& FromEndpoint() const { return fromEp; }
    const Endpoint& ToEndpoint() const { return toEp; }
    uint32_t Expiration() const { return expires; }
};

} // namespace discv4

#endif // RLP_PEERDISCOVERY_discv4_PING_HPP
