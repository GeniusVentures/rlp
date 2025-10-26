// discv4_ping.h
#ifndef DISCV4_PING_H
#define DISCV4_PING_H

#include <PeerDiscovery/Discv4Packet.hpp>

namespace discv4 {

class Discv4Ping : public Discv4Packet {
public:
    struct Endpoint {
        rlp::ByteView ip_bv;
        uint8_t ip_bytes[4];
        uint16_t udp_port = 0;
        uint16_t tcp_port = 0;

        Endpoint() = default;
        Endpoint(const std::string& ip_str, uint16_t udp, uint16_t tcp) {
            inet_pton(AF_INET, ip_str.c_str(), ip_bytes);
            ip_bv = rlp::ByteView(ip_bytes, sizeof(ip_bytes));
            udp_port = udp;
            tcp_port = tcp;
        }

        rlp::Bytes encode() {
            rlp::RlpEncoder encoder;
            encoder.begin_list();
            encoder.add(ip_bv);
            encoder.add(udp_port);
            encoder.add(tcp_port);
            encoder.end_list();
            rlp::Bytes endpoint_msg = encoder.move_bytes();

            return endpoint_msg;
        }
    };

private:
    Endpoint from_ep;
    Endpoint to_ep;
    uint32_t expires;

public:
        Discv4Ping(const std::string& from_ip, uint16_t f_udp, uint16_t f_tcp,
                             const std::string& to_ip,   uint16_t t_udp, uint16_t t_tcp)
                : Discv4Packet(0x01, 0x04, "Ping"),
                    from_ep(from_ip, f_udp, f_tcp), to_ep(to_ip, t_udp, t_tcp) {
        expires = static_cast<uint32_t>(std::time(nullptr)) + 60;
    }

    // Implement base methods
        std::vector<uint8_t> rlp_payload() override;

    // Getters
    const Endpoint& from_endpoint() const { return from_ep; }
    const Endpoint& to_endpoint() const { return to_ep; }
    uint32_t expiration() const { return expires; }
};

} // namespace discv4

#endif // DISCV4_PING_H
