// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

/// @file enr_client_test.cpp
/// @brief Unit tests for discv4_client::request_enr() send and reply-matching paths.
///
/// Uses the same UdpListener + io.run_for() pattern as discv4_client_test.cpp.
/// No sleep_for — completion is detected via atomic flags and condition polling.

#include <gtest/gtest.h>
#include "discv4/discv4_client.hpp"
#include "discv4/discv4_enr_response.hpp"
#include "discv4/discv4_constants.hpp"
#include <rlp/rlp_encoder.hpp>

#include <arpa/inet.h>
#include <atomic>
#include <array>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

using boost::asio::ip::udp;

// ---------------------------------------------------------------------------
// Minimal UDP listener (same pattern as discv4_client_test.cpp)
// ---------------------------------------------------------------------------

class UdpListener
{
public:
    explicit UdpListener( uint16_t port )
    {
        sockfd_ = ::socket( AF_INET, SOCK_DGRAM, 0 );
        EXPECT_GE( sockfd_, 0 );

        struct timeval tv{};
        tv.tv_usec = 100000; // 100 ms poll
        ::setsockopt( sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) );

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons( port );
        EXPECT_EQ( ::bind( sockfd_, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) ), 0 );

        sockaddr_in bound{};
        socklen_t   len = sizeof( bound );
        ::getsockname( sockfd_, reinterpret_cast<sockaddr*>( &bound ), &len );
        port_ = ntohs( bound.sin_port );
    }

    ~UdpListener() { stop(); }

    void start()
    {
        running_ = true;
        thread_  = std::thread( [this] {
            std::array<uint8_t, 2048U> buf{};
            while ( running_ )
            {
                ssize_t n = ::recv( sockfd_, buf.data(), buf.size(), 0 );
                if ( n > 0 ) { received_ = true; }
            }
        } );
    }

    void stop()
    {
        running_ = false;
        if ( sockfd_ >= 0 ) { ::close( sockfd_ ); sockfd_ = -1; }
        if ( thread_.joinable() ) { thread_.join(); }
    }

    bool     received() const { return received_.load(); }
    uint16_t port()     const { return port_; }

private:
    int               sockfd_{-1};
    uint16_t          port_{0U};
    std::atomic<bool> received_{false};
    std::atomic<bool> running_{false};
    std::thread       thread_;
};

// ---------------------------------------------------------------------------
// Helper: build a synthetic ENRResponse wire packet to send back to the client
// ---------------------------------------------------------------------------

/// @brief Build a minimal ENR record RLP: RLP([sig_64, seq])
std::vector<uint8_t> make_record_rlp( uint64_t seq )
{
    rlp::RlpEncoder enc;
    (void)enc.BeginList();
    const std::array<uint8_t, 64U> sig{};
    (void)enc.add( rlp::ByteView( sig.data(), sig.size() ) );
    (void)enc.add( seq );
    (void)enc.EndList();
    auto res = enc.MoveBytes();
    return std::vector<uint8_t>( res.value().begin(), res.value().end() );
}

/// @brief Build a full ENRResponse wire packet with the given reply_tok.
std::vector<uint8_t> make_enr_response_wire(
    const std::array<uint8_t, discv4::kWireHashSize>& reply_tok )
{
    const auto record = make_record_rlp( 1U );

    rlp::RlpEncoder enc;
    (void)enc.BeginList();
    (void)enc.add( rlp::ByteView( reply_tok.data(), reply_tok.size() ) );
    (void)enc.AddRaw( rlp::ByteView( record.data(), record.size() ) );
    (void)enc.EndList();

    auto rlp_res = enc.MoveBytes();

    std::vector<uint8_t> wire;
    wire.resize( discv4::kWireHashSize, 0U );
    wire.resize( discv4::kWireHashSize + discv4::kWireSigSize, 0U );
    wire.push_back( discv4::kPacketTypeEnrResponse );
    wire.insert( wire.end(), rlp_res.value().begin(), rlp_res.value().end() );
    return wire;
}

// ---------------------------------------------------------------------------
// Config helper (same as discv4_client_test.cpp)
// ---------------------------------------------------------------------------

discv4::discv4Config make_cfg()
{
    discv4::discv4Config cfg;
    cfg.bind_port    = 0U;
    cfg.tcp_port     = 0U;
    cfg.bind_ip      = "127.0.0.1";
    cfg.private_key  = {
        0xe6, 0xb1, 0x81, 0x2f, 0x04, 0xe3, 0x45, 0x19,
        0x00, 0x43, 0x4f, 0x5a, 0xbd, 0x33, 0x03, 0xb5,
        0x3d, 0x28, 0x4b, 0xd4, 0x2f, 0x42, 0x5c, 0x07,
        0x61, 0x0a, 0x82, 0xc4, 0x2b, 0x8d, 0x29, 0x77
    };
    cfg.ping_timeout = std::chrono::milliseconds( 200U );
    return cfg;
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/// @brief request_enr() delivers a UDP datagram to the target endpoint.
TEST( EnrClientTest, RequestEnrSendsPacketToTarget )
{
    UdpListener listener( 30470U );
    listener.start();

    boost::asio::io_context io;
    auto dv4 = std::make_shared<discv4::discv4_client>( io, make_cfg() );
    ASSERT_TRUE( dv4->start() );

    boost::asio::spawn( io,
        [dv4, port = listener.port()]( boost::asio::yield_context yield )
        {
            // request_enr() will time out (no real peer), but the packet must be sent.
            [[maybe_unused]] auto r = dv4->request_enr( "127.0.0.1", port, yield );
        } );

    io.run_for( std::chrono::milliseconds( 500U ) );

    EXPECT_TRUE( listener.received() )
        << "request_enr() must send an ENRRequest datagram to the target";
}

/// @brief request_enr() times out when no ENRResponse arrives.
TEST( EnrClientTest, RequestEnrTimesOutWithNoReply )
{
    UdpListener listener( 30471U );
    listener.start();

    boost::asio::io_context io;
    auto dv4 = std::make_shared<discv4::discv4_client>( io, make_cfg() );
    ASSERT_TRUE( dv4->start() );

    std::atomic<bool> completed{ false };
    std::atomic<bool> got_error{ false };

    boost::asio::spawn( io,
        [dv4, port = listener.port(), &completed, &got_error](
            boost::asio::yield_context yield )
        {
            auto r = dv4->request_enr( "127.0.0.1", port, yield );
            got_error  = !r.has_value();
            completed  = true;
        } );

    io.run_for( std::chrono::milliseconds( 600U ) );

    EXPECT_TRUE( completed ) << "request_enr() coroutine must complete within the timeout window";
    EXPECT_TRUE( got_error  ) << "request_enr() must return an error when no ENRResponse arrives";
}

/// @brief Full loopback: a peer-side thread receives the ENRRequest, extracts
///        the packet hash (ReplyTok), and sends back a valid ENRResponse.
///        request_enr() must succeed with the parsed record.
TEST( EnrClientTest, RequestEnrCompletesOnValidResponse )
{
    // Pick a fixed port for the simulated peer.
    constexpr uint16_t kPeerPort = 30472U;

    // --- Peer-side socket ---
    int peer_fd = ::socket( AF_INET, SOCK_DGRAM, 0 );
    ASSERT_GE( peer_fd, 0 );
    {
        struct timeval tv{};
        tv.tv_sec = 1;
        ::setsockopt( peer_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) );

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons( kPeerPort );
        int r = ::bind( peer_fd, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) );
        if ( r != 0 )
        {
            ::close( peer_fd );
            GTEST_SKIP() << "Port " << kPeerPort << " unavailable — skipping loopback test";
        }
    }

    boost::asio::io_context io;
    auto dv4 = std::make_shared<discv4::discv4_client>( io, make_cfg() );
    ASSERT_TRUE( dv4->start() );
    const uint16_t client_port = dv4->bound_port();

    std::atomic<bool> enr_success{ false };

    // Spawn request_enr() coroutine.
    boost::asio::spawn( io,
        [dv4, &enr_success]( boost::asio::yield_context yield )
        {
            auto r       = dv4->request_enr( "127.0.0.1", kPeerPort, yield );
            enr_success  = r.has_value();
        } );

    // Peer thread: receive ENRRequest, extract hash, send back ENRResponse.
    std::thread peer_thread( [peer_fd, client_port]()
    {
        std::array<uint8_t, 2048U> buf{};
        sockaddr_in sender_addr{};
        socklen_t   sender_len = sizeof( sender_addr );

        ssize_t n = ::recvfrom( peer_fd, buf.data(), buf.size(), 0,
                                reinterpret_cast<sockaddr*>( &sender_addr ), &sender_len );
        if ( n < static_cast<ssize_t>( discv4::kWireHashSize ) )
        {
            ::close( peer_fd );
            return;
        }

        // The outer hash is the first kWireHashSize bytes of the wire packet.
        std::array<uint8_t, discv4::kWireHashSize> reply_tok{};
        std::copy( buf.begin(), buf.begin() + discv4::kWireHashSize, reply_tok.begin() );

        // Build ENRResponse wire packet addressed back to the client.
        const auto response_wire = make_enr_response_wire( reply_tok );

        sender_addr.sin_port = htons( client_port );
        ::sendto( peer_fd, response_wire.data(), response_wire.size(), 0,
                  reinterpret_cast<sockaddr*>( &sender_addr ), sizeof( sender_addr ) );
        ::close( peer_fd );
    } );

    io.run_for( std::chrono::milliseconds( 800U ) );
    peer_thread.join();

    EXPECT_TRUE( enr_success )
        << "request_enr() must succeed when a matching ENRResponse is received";
}

