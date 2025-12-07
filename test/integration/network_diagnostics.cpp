// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

// Network diagnostics utility for troubleshooting P2P connectivity issues
// Provides detailed network information useful for debugging CI/local failures

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <boost/asio.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

namespace asio = boost::asio;

namespace rlp::integration::diagnostics {

// Print local network interfaces
void printNetworkInterfaces() noexcept
{
    std::cout << "\n=== Local Network Interfaces ===\n";

#ifdef _WIN32
    // Windows implementation using GetAdaptersAddresses
    ULONG bufferSize = 15000;
    std::vector<uint8_t> buffer( bufferSize );
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>( buffer.data() );

    ULONG result = GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX,
        nullptr,
        pAddresses,
        &bufferSize
    );

    if ( result == NO_ERROR )
    {
        for ( auto pCurrAddresses = pAddresses; pCurrAddresses; pCurrAddresses = pCurrAddresses->Next )
        {
            std::wcout << L"Interface: " << pCurrAddresses->FriendlyName << L"\n";
            std::cout << "  Status: " << ( pCurrAddresses->OperStatus == IfOperStatusUp ? "UP" : "DOWN" ) << "\n";

            for ( auto pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next )
            {
                char ip[INET6_ADDRSTRLEN];
                if ( pUnicast->Address.lpSockaddr->sa_family == AF_INET )
                {
                    auto addr = reinterpret_cast<sockaddr_in*>( pUnicast->Address.lpSockaddr );
                    inet_ntop( AF_INET, &addr->sin_addr, ip, sizeof( ip ) );
                    std::cout << "  IPv4: " << ip << "\n";
                }
                else if ( pUnicast->Address.lpSockaddr->sa_family == AF_INET6 )
                {
                    auto addr = reinterpret_cast<sockaddr_in6*>( pUnicast->Address.lpSockaddr );
                    inet_ntop( AF_INET6, &addr->sin6_addr, ip, sizeof( ip ) );
                    std::cout << "  IPv6: " << ip << "\n";
                }
            }
            std::cout << "\n";
        }
    }
    else
    {
        std::cerr << "GetAdaptersAddresses failed with error: " << result << "\n";
    }
#else
    // Unix/Linux implementation using getifaddrs
    struct ifaddrs* ifaddr = nullptr;
    if ( getifaddrs( &ifaddr ) == -1 )
    {
        std::cerr << "getifaddrs failed\n";
        return;
    }

    for ( auto ifa = ifaddr; ifa; ifa = ifa->ifa_next )
    {
        if ( !ifa->ifa_addr ) continue;

        if ( ifa->ifa_addr->sa_family == AF_INET )
        {
            char ip[INET_ADDRSTRLEN];
            auto addr = reinterpret_cast<sockaddr_in*>( ifa->ifa_addr );
            inet_ntop( AF_INET, &addr->sin_addr, ip, sizeof( ip ) );

            std::cout << "Interface: " << ifa->ifa_name << "\n";
            std::cout << "  IPv4: " << ip << "\n";
            std::cout << "  Flags: " << ( ifa->ifa_flags & IFF_UP ? "UP " : "DOWN " )
                      << ( ifa->ifa_flags & IFF_RUNNING ? "RUNNING " : "" )
                      << ( ifa->ifa_flags & IFF_LOOPBACK ? "LOOPBACK" : "" ) << "\n\n";
        }
        else if ( ifa->ifa_addr->sa_family == AF_INET6 )
        {
            char ip[INET6_ADDRSTRLEN];
            auto addr = reinterpret_cast<sockaddr_in6*>( ifa->ifa_addr );
            inet_ntop( AF_INET6, &addr->sin6_addr, ip, sizeof( ip ) );

            std::cout << "Interface: " << ifa->ifa_name << "\n";
            std::cout << "  IPv6: " << ip << "\n\n";
        }
    }

    freeifaddrs( ifaddr );
#endif
}

// Test DNS resolution
void testDnsResolution( const std::vector<std::string>& hostnames ) noexcept
{
    std::cout << "\n=== DNS Resolution Test ===\n";

    try
    {
        asio::io_context io_context;
        asio::ip::tcp::resolver resolver( io_context );

        for ( const auto& hostname : hostnames )
        {
            std::cout << "\nResolving: " << hostname << "\n";

            try
            {
                boost::system::error_code ec;
                auto endpoints = resolver.resolve( hostname, "30303", ec );

                if ( ec )
                {
                    std::cout << "  [FAIL] Resolution failed: " << ec.message() << "\n";
                    continue;
                }

                std::cout << "  [OK] Resolved to:\n";
                for ( const auto& endpoint : endpoints )
                {
                    std::cout << "    " << endpoint.endpoint().address().to_string()
                              << ":" << endpoint.endpoint().port() << "\n";
                }
            }
            catch ( const std::exception& e )
            {
                std::cout << "  [FAIL] Exception: " << e.what() << "\n";
            }
        }
    }
    catch ( const std::exception& e )
    {
        std::cerr << "DNS test exception: " << e.what() << "\n";
    }
}

// Test outbound connectivity to a host
void testOutboundConnection( const std::string& host, uint16_t port ) noexcept
{
    std::cout << "\n=== Outbound Connection Test ===\n";
    std::cout << "Target: " << host << ":" << port << "\n";

    try
    {
        asio::io_context io_context;
        asio::ip::tcp::socket socket( io_context );
        asio::ip::tcp::resolver resolver( io_context );

        boost::system::error_code ec;
        auto endpoints = resolver.resolve( host, std::to_string( port ), ec );

        if ( ec )
        {
            std::cout << "[FAIL] DNS resolution failed: " << ec.message() << "\n";
            return;
        }

        auto start = std::chrono::steady_clock::now();
        asio::connect( socket, endpoints, ec );
        auto end = std::chrono::steady_clock::now();

        if ( ec )
        {
            std::cout << "[FAIL] Connection failed: " << ec.message() << "\n";
            std::cout << "  Possible causes:\n";
            std::cout << "    - Firewall blocking outbound connections\n";
            std::cout << "    - Remote host unreachable\n";
            std::cout << "    - Network configuration issue\n";
        }
        else
        {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
            std::cout << "[OK] Connected successfully in " << duration << "ms\n";

            auto local_endpoint = socket.local_endpoint( ec );
            if ( !ec )
            {
                std::cout << "  Local endpoint: " << local_endpoint.address().to_string()
                          << ":" << local_endpoint.port() << "\n";
            }

            auto remote_endpoint = socket.remote_endpoint( ec );
            if ( !ec )
            {
                std::cout << "  Remote endpoint: " << remote_endpoint.address().to_string()
                          << ":" << remote_endpoint.port() << "\n";
            }

            socket.close();
        }
    }
    catch ( const std::exception& e )
    {
        std::cout << "[FAIL] Exception: " << e.what() << "\n";
    }
}

// Check system firewall hints (platform-specific)
void checkFirewallHints() noexcept
{
    std::cout << "\n=== Firewall Detection Hints ===\n";

#ifdef _WIN32
    std::cout << "Platform: Windows\n";
    std::cout << "To check Windows Firewall:\n";
    std::cout << "  netsh advfirewall show allprofiles state\n";
    std::cout << "To temporarily disable for testing:\n";
    std::cout << "  netsh advfirewall set allprofiles state off\n";
#else
    std::cout << "Platform: Unix/Linux\n";
    std::cout << "Check firewall status:\n";
    std::cout << "  sudo iptables -L -n -v  (iptables)\n";
    std::cout << "  sudo ufw status         (ufw)\n";
    std::cout << "  sudo firewall-cmd --list-all (firewalld)\n";
#endif

    std::cout << "\nCommon firewall indicators:\n";
    std::cout << "  - Connection timeout errors\n";
    std::cout << "  - 'Connection refused' (port closed vs. filtered)\n";
    std::cout << "  - UDP packets silently dropped\n";
}

// Main diagnostic runner
void runFullDiagnostics() noexcept
{
    std::cout << "\n╔════════════════════════════════════════╗\n";
    std::cout << "║  Network Diagnostics Tool - P2P Tests  ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";

    // 1. Network interfaces
    printNetworkInterfaces();

    // 2. DNS resolution
    testDnsResolution(
        {
            "138.197.51.181",            // Sepolia IP (should resolve to itself)
            "35.197.249.21",             // Amoy IP
            "op-sepolia-bootnode-1.optimism.io" // Base Sepolia hostname
        }
    );

    // 3. Outbound connectivity
    testOutboundConnection( "138.197.51.181", 30303 ); // Sepolia

    // 4. Firewall hints
    checkFirewallHints();

    std::cout << "\n=== Diagnostics Complete ===\n";
    std::cout << "Review the output above to identify connectivity issues.\n";
}

} // namespace rlp::integration::diagnostics

// Standalone executable entry point
int main( int argc, char* argv[] )
{
    std::cout << "RLP Network Diagnostics Tool\n";
    std::cout << "Usage: " << argv[0] << " [--full]\n\n";

    if ( argc > 1 && std::string( argv[1] ) == "--full" )
    {
        rlp::integration::diagnostics::runFullDiagnostics();
    }
    else
    {
        // Quick test mode
        std::cout << "Quick connectivity test mode\n";
        rlp::integration::diagnostics::testOutboundConnection( "138.197.51.181", 30303 );
    }

    return 0;
}
