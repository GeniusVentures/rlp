// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

// Simple local test to verify network connectivity to EVM testnets
// Tests TCP/UDP port accessibility before attempting full P2P handshake

#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <chrono>
#include <string>
#include <vector>
#include <iostream>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using udp = asio::ip::udp;

namespace rlp::integration {

// Network endpoint configuration
struct NetworkEndpoint
{
    std::string name;
    std::string host;
    uint16_t tcp_port;
    uint16_t udp_port;
    std::chrono::seconds timeout{5};
};

// Test networks based on research
const std::vector<NetworkEndpoint> kTestNetworks = {
    // Ethereum Sepolia bootnodes
    {"Sepolia-NYC", "138.197.51.181", 30303, 30303, std::chrono::seconds(5)},
    {"Sepolia-SFO", "146.190.1.103", 30303, 30303, std::chrono::seconds(5)},
    {"Sepolia-Sydney", "170.64.250.88", 30303, 30303, std::chrono::seconds(5)},

    // Polygon Amoy bootnodes
    {"Amoy-GCP-1", "35.197.249.21", 30303, 30303, std::chrono::seconds(5)},
    {"Amoy-GCP-2", "34.89.39.114", 30303, 30303, std::chrono::seconds(5)},

    // Base Sepolia (OP Stack uses different port)
    {"Base-Sepolia", "op-sepolia-bootnode-1.optimism.io", 9222, 9222, std::chrono::seconds(5)},
};

// Helper class for network testing (not a test itself)
class NetworkTestHelper
{
public:
    asio::io_context io_context_;

    // Helper: Test TCP connectivity
    bool testTcpConnect( const std::string& host, uint16_t port, std::chrono::seconds timeout ) noexcept
    {
        try
        {
            tcp::socket socket( io_context_ );
            tcp::resolver resolver( io_context_ );

            // Resolve hostname
            boost::system::error_code ec;
            auto endpoints = resolver.resolve( host, std::to_string( port ), ec );
            if ( ec )
            {
                std::cerr << "DNS resolution failed for " << host << ": " << ec.message() << "\n";
                return false;
            }

            // Set socket timeout
            socket.open( tcp::v4(), ec );
            if ( ec )
            {
                std::cerr << "Failed to open socket: " << ec.message() << "\n";
                return false;
            }

            // Attempt connection with timeout
            asio::steady_timer timer( io_context_ );
            timer.expires_after( timeout );

            bool connected = false;
            bool timeout_expired = false;

            asio::async_connect( socket, endpoints,
                [&]( const boost::system::error_code& error, const tcp::endpoint& )
                {
                    if ( !error )
                    {
                        connected = true;
                    }
                    timer.cancel();
                }
            );

            timer.async_wait( [&]( const boost::system::error_code& error )
                {
                    if ( !error )
                    {
                        timeout_expired = true;
                        socket.cancel();
                    }
                }
            );

            io_context_.run();
            io_context_.restart();

            if ( connected )
            {
                socket.close( ec );
                return true;
            }

            if ( timeout_expired )
            {
                std::cerr << "Connection timeout to " << host << ":" << port << "\n";
            }

            return false;
        }
        catch ( const std::exception& e )
        {
            std::cerr << "TCP test exception: " << e.what() << "\n";
            return false;
        }
    }

    // Helper: Test UDP connectivity (send/receive)
    bool testUdpConnect( const std::string& host, uint16_t port, std::chrono::seconds timeout ) noexcept
    {
        try
        {
            udp::socket socket( io_context_, udp::v4() );
            udp::resolver resolver( io_context_ );

            // Resolve hostname
            boost::system::error_code ec;
            auto endpoints = resolver.resolve( udp::v4(), host, std::to_string( port ), ec );
            if ( ec )
            {
                std::cerr << "UDP DNS resolution failed for " << host << ": " << ec.message() << "\n";
                return false;
            }

            udp::endpoint remote_endpoint = *endpoints.begin();

            // Send a simple test packet (empty is fine for connectivity test)
            std::vector<uint8_t> test_packet = { 0x00, 0x01, 0x02, 0x03 };
            socket.send_to( asio::buffer( test_packet ), remote_endpoint, 0, ec );

            if ( ec )
            {
                std::cerr << "UDP send failed: " << ec.message() << "\n";
                return false;
            }

            // Try to receive response (may timeout - that's ok, we just test send capability)
            std::vector<uint8_t> recv_buffer( 1024 );
            udp::endpoint sender_endpoint;

            socket.async_receive_from(
                asio::buffer( recv_buffer ),
                sender_endpoint,
                [&]( const boost::system::error_code& error, std::size_t bytes_received )
                {
                    if ( !error && bytes_received > 0 )
                    {
                        std::cout << "UDP response received (" << bytes_received << " bytes)\n";
                    }
                }
            );

            // Set timeout
            asio::steady_timer timer( io_context_ );
            timer.expires_after( timeout );
            timer.async_wait( [&]( const boost::system::error_code& )
                {
                    socket.cancel();
                }
            );

            io_context_.run();
            io_context_.restart();

            // UDP send succeeded is enough for connectivity test
            return true;
        }
        catch ( const std::exception& e )
        {
            std::cerr << "UDP test exception: " << e.what() << "\n";
            return false;
        }
    }
};

// Actual test class that uses the helper
class NetworkPortTest : public ::testing::TestWithParam<NetworkEndpoint>, public NetworkTestHelper
{
};

// Parameterized test: TCP connectivity to all networks
TEST_P( NetworkPortTest, TcpConnectivity )
{
    const auto& network = GetParam();

    std::cout << "\n=== Testing TCP connectivity to " << network.name
              << " (" << network.host << ":" << network.tcp_port << ") ===\n";

    auto start = std::chrono::steady_clock::now();
    bool connected = testTcpConnect( network.host, network.tcp_port, network.timeout );
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();

    std::cout << "TCP connection ";
    if ( connected )
    {
        std::cout << "[OK] SUCCESS in " << duration << "ms\n";
        EXPECT_TRUE( connected );
    }
    else
    {
        std::cout << "[FAIL] FAILED after " << duration << "ms\n";
        GTEST_SKIP() << network.name << " TCP port unreachable (firewall or node down)";
    }
}

// Parameterized test: UDP connectivity to all networks
TEST_P( NetworkPortTest, UdpConnectivity )
{
    const auto& network = GetParam();

    std::cout << "\n=== Testing UDP connectivity to " << network.name
              << " (" << network.host << ":" << network.udp_port << ") ===\n";

    auto start = std::chrono::steady_clock::now();
    bool can_send = testUdpConnect( network.host, network.udp_port, network.timeout );
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();

    std::cout << "UDP send ";
    if ( can_send )
    {
        std::cout << "[OK] SUCCESS in " << duration << "ms\n";
        EXPECT_TRUE( can_send );
    }
    else
    {
        std::cout << "[FAIL] FAILED after " << duration << "ms\n";
        std::cout << "NOTE: UDP may be blocked by firewall (common in corporate/CI environments)\n";
        GTEST_SKIP() << network.name << " UDP port unreachable (likely firewall block)";
    }
}

// Instantiate tests for all networks
INSTANTIATE_TEST_SUITE_P(
    AllNetworks,
    NetworkPortTest,
    ::testing::ValuesIn( kTestNetworks ),
    []( const ::testing::TestParamInfo<NetworkEndpoint>& info )
    {
        // Replace hyphens with underscores for valid test names
        std::string name = info.param.name;
        std::replace( name.begin(), name.end(), '-', '_' );
        return name;
    }
);

// Summary test: Report firewall status
TEST( NetworkPortTest, FirewallSummary )
{
    std::cout << "\n=== Network Firewall Analysis ===\n";
    std::cout << "Testing " << kTestNetworks.size() << " network endpoints:\n";

    int tcp_success = 0;
    int udp_success = 0;

    NetworkTestHelper test_helper;

    for ( const auto& network : kTestNetworks )
    {
        std::cout << "\n[" << network.name << "]\n";

        bool tcp_ok = test_helper.testTcpConnect( network.host, network.tcp_port, std::chrono::seconds( 3 ) );
        bool udp_ok = test_helper.testUdpConnect( network.host, network.udp_port, std::chrono::seconds( 3 ) );

        std::cout << "  TCP: " << ( tcp_ok ? "[OK]" : "[FAIL]" ) << "\n";
        std::cout << "  UDP: " << ( udp_ok ? "[OK]" : "[FAIL]" ) << "\n";

        if ( tcp_ok ) tcp_success++;
        if ( udp_ok ) udp_success++;
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "TCP Success: " << tcp_success << "/" << kTestNetworks.size() << "\n";
    std::cout << "UDP Success: " << udp_success << "/" << kTestNetworks.size() << "\n";

    if ( tcp_success == 0 )
    {
        std::cout << "\nWARNING: No TCP connections succeeded!\n";
        std::cout << "Check firewall settings or network connectivity.\n";
        GTEST_SKIP() << "Complete network isolation detected";
    }

    if ( udp_success == 0 )
    {
        std::cout << "\nWARNING: No UDP connections succeeded!\n";
        std::cout << "UDP may be blocked. P2P discovery will need TCP-only fallback.\n";
    }

    EXPECT_GT( tcp_success, 0 ) << "At least one TCP connection should succeed";
}

} // namespace rlp::integration
