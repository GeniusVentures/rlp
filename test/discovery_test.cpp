#include <gtest/gtest.h>
#include <PeerDiscovery/discovery.hpp>
#include <PeerDiscovery/packet_factory.hpp>
#include <PeerDiscovery/Discv4Pong.hpp>
#include <vector>
#include <array>

using namespace rlp;
using namespace discv4;


TEST(PeerDiscovery, RunTest) {
    asio::io_context io;

    auto callback = [](const std::vector<uint8_t>& data, const udp::endpoint& endpoint) {
        rlp::ByteView raw_packet_data(data.data(), data.size());
        std::cout<<"Received PONG: "<<rlp::hexToString(raw_packet_data)<<"\n\n";
        auto parse_result = Discv4Pong::parse(raw_packet_data);
        
        if (!parse_result) 
        {
            // Handle parsing error
            const char* error_str = rlp::decoding_error_to_string(parse_result.error());
            printf("Failed to parse PONG packet: %s\n", error_str);
            return;
        }
        
        const auto& pong = parse_result.value();
        
        // Process the parsed PONG packet
        printf("Received PONG packet:\n");
        printf("  Endpoint IP: %d.%d.%d.%d\n", 
            pong.to_endpoint.ip[0], pong.to_endpoint.ip[1], 
            pong.to_endpoint.ip[2], pong.to_endpoint.ip[3]);
        printf("  UDP Port: %u\n", pong.to_endpoint.udp_port);
        printf("  TCP Port: %u\n", pong.to_endpoint.tcp_port);
        printf("  Ping Hash: ");
        for (const auto& byte : pong.ping_hash) 
        {
            printf("%02x", byte);
        }
        printf("\n");
        printf("  Expiration: %u\n", pong.expiration);
        
        // Validate expiration timestamp
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        if (pong.expiration < static_cast<uint64_t>(now)) 
        {
            printf("Warning: PONG packet has expired\n");
        }
    };

    // Send Ping and wait
    std::vector<uint8_t> priv_key = {
        0xe6, 0xb1, 0x81, 0x2f, 0x04, 0xe3, 0x45, 0x19,
        0x00, 0x43, 0x4f, 0x5a, 0xbd, 0x33, 0x03, 0xb5,
        0x3d, 0x28, 0x4b, 0xd4, 0x2f, 0x42, 0x5c, 0x07,
        0x61, 0x0a, 0x82, 0xc4, 0x2b, 0x8d, 0x29, 0x77
    };

    discv4::PacketFactory::send_ping_and_wait(
        io,
        "10.0.2.15", 30303, 30303,
        "146.190.13.128", 30303, 30303,
        priv_key,
        callback
    );
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}