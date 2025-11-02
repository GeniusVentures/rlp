// discv4_ping.h
#ifndef RLP_PEERDISCOVERY_DISCV4_PING_HPP
#define RLP_PEERDISCOVERY_DISCV4_PING_HPP

#include <rlp/PeerDiscovery/Discv4Packet.hpp>

namespace discv4 {

class Discv4Ping : public Discv4Packet
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

        rlp::Bytes encode()
        {
            rlp::RlpEncoder encoder;
            encoder.BeginList();
            encoder.add( ipBv );
            encoder.add( udpPort );
            encoder.add( tcpPort );
            encoder.EndList();
            auto bytes_result = encoder.MoveBytes();
            if (!bytes_result) {
                return rlp::Bytes(); // Return empty on error
            }
            return std::move(bytes_result.value());
        }
    };

private:
    Endpoint fromEp;
    Endpoint toEp;
    uint32_t expires;

public:
    Discv4Ping( const std::string& fromIp, uint16_t fUdp, uint16_t fTcp,
                const std::string& toIp, uint16_t tUdp, uint16_t tTcp )
        : Discv4Packet( 0x01, 0x04, "Ping" ),
          fromEp( fromIp, fUdp, fTcp ), toEp( toIp, tUdp, tTcp )
    {
        expires = static_cast<uint32_t>( std::time( nullptr ) ) + 60;
    }

    // Implement base methods
    std::vector<uint8_t> RlpPayload() override;

    // Getters
    const Endpoint& FromEndpoint() const { return fromEp; }
    const Endpoint& ToEndpoint() const { return toEp; }
    uint32_t Expiration() const { return expires; }
};

} // namespace discv4

#endif // RLP_PEERDISCOVERY_DISCV4_PING_HPP
