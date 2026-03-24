/// @file discv4_client_test.cpp
/// @brief Unit tests for discv4_client lifetime and receive-loop behaviour.
///
/// These tests guard against the bug where the discv4_client shared_ptr goes
/// out of scope before io.run(), destroying the socket and silently killing
/// all UDP traffic.

#include <discv4/discv4_client.hpp>
#include <discv4/discv4_constants.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <array>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <rlp/rlp_encoder.hpp>
#include <thread>

namespace {

using boost::asio::ip::udp;

/// @brief A minimal UDP listener that records the first datagram it receives.
///        Uses a dedicated Boost.Asio socket so the test stays cross-platform.
class UdpListener {
public:
    explicit UdpListener(uint16_t port)
        : socket_(io_)
    {
        boost::system::error_code ec;
        socket_.open(udp::v4(), ec);
        EXPECT_FALSE(ec) << "open() failed";

        socket_.bind(udp::endpoint(udp::v4(), port), ec);
        EXPECT_FALSE(ec) << "bind() failed on port " << port;

        port_ = socket_.local_endpoint(ec).port();
        EXPECT_FALSE(ec) << "local_endpoint() failed";
    }

    ~UdpListener() { stop(); }

    /// @brief Start listening in a background thread.
    void start()
    {
        socket_.async_receive_from(
            boost::asio::buffer(buffer_),
            sender_endpoint_,
            [this](const boost::system::error_code& ec, std::size_t bytes_received)
            {
                if (!ec && bytes_received > 0U) {
                    received_ = true;
                }
            });

        thread_ = std::thread([this] {
            io_.run();
        });
    }

    /// @brief Stop the listener.
    void stop()
    {
        boost::system::error_code ec;
        socket_.cancel(ec);
        socket_.close(ec);
        io_.stop();
        if (thread_.joinable()) { thread_.join(); }
    }

    /// @brief True if at least one datagram was received.
    bool received() const { return received_.load(); }

    uint16_t port() const { return port_; }

private:
    boost::asio::io_context io_;
    udp::socket socket_;
    udp::endpoint sender_endpoint_{};
    std::array<uint8_t, 2048> buffer_{};
    uint16_t port_{0};
    std::atomic<bool> received_{false};
    std::thread thread_;
};

/// @brief Build a minimal discv4Config with a generated keypair.
discv4::discv4Config make_cfg(boost::asio::io_context& /*io*/)
{
    discv4::discv4Config cfg;
    cfg.bind_port  = 0;  // OS-assigned ephemeral port
    cfg.tcp_port   = 0;
    cfg.bind_ip    = "127.0.0.1";
    // Use a fixed test private key (valid secp256k1 scalar).
    cfg.private_key = {
        0xe6, 0xb1, 0x81, 0x2f, 0x04, 0xe3, 0x45, 0x19,
        0x00, 0x43, 0x4f, 0x5a, 0xbd, 0x33, 0x03, 0xb5,
        0x3d, 0x28, 0x4b, 0xd4, 0x2f, 0x42, 0x5c, 0x07,
        0x61, 0x0a, 0x82, 0xc4, 0x2b, 0x8d, 0x29, 0x77
    };
    // Corresponding public key is not needed for the send-only tests below.
    // Short timeout ensures coroutines complete within test run_for() windows.
    cfg.ping_timeout = std::chrono::milliseconds(100);
    return cfg;
}

/// @brief Build a minimal PONG wire packet addressed back to the provided endpoint.
std::vector<uint8_t> make_pong_wire(
    const std::array<uint8_t, discv4::kWireHashSize>& ping_hash,
    const udp::endpoint&                               recipient )
{
    rlp::RlpEncoder encoder;
    EXPECT_TRUE( encoder.BeginList().has_value() );
    EXPECT_TRUE( encoder.BeginList().has_value() );

    const auto recipient_ip = recipient.address().to_v4().to_bytes();
    EXPECT_TRUE( encoder.add( rlp::ByteView( recipient_ip.data(), recipient_ip.size() ) ).has_value() );
    EXPECT_TRUE( encoder.add( recipient.port() ).has_value() );
    EXPECT_TRUE( encoder.add( recipient.port() ).has_value() );
    EXPECT_TRUE( encoder.EndList().has_value() );
    EXPECT_TRUE( encoder.add( rlp::ByteView( ping_hash.data(), ping_hash.size() ) ).has_value() );

    const uint32_t expiration = static_cast<uint32_t>( std::time( nullptr ) ) + 60U;
    EXPECT_TRUE( encoder.add( expiration ).has_value() );
    EXPECT_TRUE( encoder.EndList().has_value() );

    auto pong_bytes = encoder.MoveBytes();
    EXPECT_TRUE( pong_bytes.has_value() );

    std::vector<uint8_t> wire;
    wire.reserve( discv4::kWireHeaderSize + pong_bytes.value().size() );
    wire.insert( wire.end(), discv4::kWireHashSize, 0U );
    wire.insert( wire.end(), discv4::kWireSigSize, 0U );
    wire.push_back( discv4::kPacketTypePong );
    wire.insert( wire.end(), pong_bytes.value().begin(), pong_bytes.value().end() );
    return wire;
}

} // namespace

// ---------------------------------------------------------------------------

/// @brief A live discv4_client whose shared_ptr is kept alive across io.run()
///        can send packets that reach a local UDP listener.
TEST(DiscoveryClientLifetimeTest, ClientAlive_PacketReachesListener)
{
    UdpListener listener(30450);
    listener.start();

    boost::asio::io_context io;
    auto cfg = make_cfg(io);
    // Keep client alive in outer scope — this is the CORRECT pattern.
    auto dv4 = std::make_shared<discv4::discv4_client>(io, cfg);
    ASSERT_TRUE(dv4->start()) << "discv4_client::start() failed";

    discv4::NodeId dummy_id{};  // zeroed — ping() doesn't validate the node_id

    boost::asio::spawn(io,
        [dv4, port = listener.port(), dummy_id](boost::asio::yield_context yield) {
            [[maybe_unused]] auto r = dv4->ping("127.0.0.1", port, dummy_id, yield);
        });

    io.run_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(listener.received())
        << "discv4_client must send a PING when kept alive across io.run()";
}

/// @brief Verifies the CORRECT eth_watch.cpp pattern:
///        dv4 declared in outer scope (not inside an if-block) stays alive
///        across multiple simulated discovery callbacks and can send a PING
///        to each discovered peer.
TEST(DiscoveryClientLifetimeTest, ClientOuterScope_MultiPingReachesListeners)
{
    UdpListener listener1(30452);
    UdpListener listener2(30453);
    listener1.start();
    listener2.start();

    boost::asio::io_context io;
    discv4::NodeId dummy_id{};

    // CORRECT pattern: dv4 declared in outer scope.
    std::shared_ptr<discv4::discv4_client> dv4;
    {
        auto cfg = make_cfg(io);
        dv4 = std::make_shared<discv4::discv4_client>(io, cfg);
        ASSERT_TRUE(dv4->start()) << "discv4_client::start() failed";
    }
    // dv4 still alive here — inner scope only held the config.

    // Simulate two discovery callbacks firing (two different peers found).
    boost::asio::spawn(io,
        [dv4, p1 = listener1.port(), dummy_id](boost::asio::yield_context yield) {
            [[maybe_unused]] auto r = dv4->ping("127.0.0.1", p1, dummy_id, yield);
        });

    boost::asio::spawn(io,
        [dv4, p2 = listener2.port(), dummy_id](boost::asio::yield_context yield) {
            [[maybe_unused]] auto r = dv4->ping("127.0.0.1", p2, dummy_id, yield);
        });

    io.run_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(listener1.received())
        << "PING must reach first discovered peer when dv4 is in outer scope";
    EXPECT_TRUE(listener2.received())
        << "PING must reach second discovered peer when dv4 is in outer scope";
}

// ---------------------------------------------------------------------------
// RecursiveBondingTest
//
// These tests verify the mechanisms used by the recursive Kademlia bonding
// code in handle_pong() and handle_neighbours():
//   - handle_pong  spawns find_node() for newly-seen peers
//   - handle_neighbours spawns ping() for newly-discovered peers
//
// Because bonded_set_ is private and triggering handle_pong requires a fully-
// signed wire packet, we test the underlying send primitives directly:
// verifying that find_node() and ping() actually deliver UDP datagrams gives
// confidence that the coroutines spawned by those handlers will work correctly
// when real signed packets arrive.
// ---------------------------------------------------------------------------

/// @brief find_node() delivers a UDP datagram to the target listener.
TEST(RecursiveBondingTest, FindNodeSentToPeer_PacketReachesListener)
{
    UdpListener listener(30460);
    listener.start();

    boost::asio::io_context io;
    auto cfg = make_cfg(io);
    auto dv4 = std::make_shared<discv4::discv4_client>(io, cfg);
    ASSERT_TRUE(dv4->start()) << "discv4_client::start() failed";

    discv4::NodeId targetId{};  // zeroed target — acceptable for send test

    boost::asio::spawn(io,
        [dv4, port = listener.port(), targetId](boost::asio::yield_context yield) {
            [[maybe_unused]] auto r =
                dv4->find_node("127.0.0.1", port, targetId, yield);
        });

    io.run_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(listener.received())
        << "find_node() must deliver a UDP datagram to the target endpoint";
}

/// @brief ping() to a new peer delivers a datagram — mirrors the spawn issued
///        by handle_neighbours for each undiscovered peer.
TEST(RecursiveBondingTest, PingToNewPeer_PacketReachesListener)
{
    UdpListener listener(30461);
    listener.start();

    boost::asio::io_context io;
    auto cfg = make_cfg(io);
    auto dv4 = std::make_shared<discv4::discv4_client>(io, cfg);
    ASSERT_TRUE(dv4->start()) << "discv4_client::start() failed";

    discv4::NodeId dummyId{};

    boost::asio::spawn(io,
        [dv4, port = listener.port(), dummyId](boost::asio::yield_context yield) {
            [[maybe_unused]] auto r =
                dv4->ping("127.0.0.1", port, dummyId, yield);
        });

    io.run_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(listener.received())
        << "ping() must deliver a UDP datagram when called for a new peer";
}

/// @brief Spawning find_node() to multiple distinct peers all deliver packets.
///        This mirrors what handle_pong does when each of N new peers replies.
TEST(RecursiveBondingTest, FindNodeSentToMultiplePeers_AllReceivePackets)
{
    UdpListener listener1(30462);
    UdpListener listener2(30463);
    UdpListener listener3(30464);
    listener1.start();
    listener2.start();
    listener3.start();

    boost::asio::io_context io;
    auto cfg = make_cfg(io);
    auto dv4 = std::make_shared<discv4::discv4_client>(io, cfg);
    ASSERT_TRUE(dv4->start()) << "discv4_client::start() failed";

    discv4::NodeId targetId{};

    boost::asio::spawn(io,
        [dv4, p = listener1.port(), targetId](boost::asio::yield_context yield) {
            [[maybe_unused]] auto r =
                dv4->find_node("127.0.0.1", p, targetId, yield);
        });

    boost::asio::spawn(io,
        [dv4, p = listener2.port(), targetId](boost::asio::yield_context yield) {
            [[maybe_unused]] auto r =
                dv4->find_node("127.0.0.1", p, targetId, yield);
        });

    boost::asio::spawn(io,
        [dv4, p = listener3.port(), targetId](boost::asio::yield_context yield) {
            [[maybe_unused]] auto r =
                dv4->find_node("127.0.0.1", p, targetId, yield);
        });

    io.run_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(listener1.received())
        << "find_node() must reach first peer";
    EXPECT_TRUE(listener2.received())
        << "find_node() must reach second peer";
    EXPECT_TRUE(listener3.received())
        << "find_node() must reach third peer";
}

/// @brief ping() succeeds when the received PONG echoes the outbound PING wire hash.
TEST(RecursiveBondingTest, PingCompletesOnlyWhenPongTokenMatches)
{
    constexpr uint16_t kPeerPort = 30465U;

    boost::asio::io_context peer_io;
    udp::socket peer_socket( peer_io );
    {
        boost::system::error_code ec;
        peer_socket.open( udp::v4(), ec );
        ASSERT_FALSE( ec ) << "open() failed";

        peer_socket.bind( udp::endpoint( udp::v4(), kPeerPort ), ec );
        if ( ec )
        {
            GTEST_SKIP() << "Port " << kPeerPort << " unavailable — skipping loopback test";
        }
    }

    boost::asio::io_context io;
    auto cfg = make_cfg( io );
    auto dv4 = std::make_shared<discv4::discv4_client>( io, cfg );
    ASSERT_TRUE( dv4->start() ) << "discv4_client::start() failed";

    discv4::NodeId dummy_id{};
    std::atomic<bool> ping_completed{ false };
    std::atomic<bool> ping_succeeded{ false };

    boost::asio::spawn( io,
        [dv4, &ping_completed, &ping_succeeded, peer_port = kPeerPort, dummy_id]( boost::asio::yield_context yield )
        {
            auto result = dv4->ping( "127.0.0.1", peer_port, dummy_id, yield );
            ping_succeeded = result.has_value();
            ping_completed = true;
        } );

    std::thread peer_thread( [&peer_socket]()
    {
        std::array<uint8_t, 2048U> buf{};
        udp::endpoint sender_endpoint;
        boost::system::error_code ec;

        const std::size_t bytes_received = peer_socket.receive_from(
            boost::asio::buffer( buf ),
            sender_endpoint,
            0,
            ec );
        if ( ec || bytes_received < discv4::kWireHashSize )
        {
            return;
        }

        std::array<uint8_t, discv4::kWireHashSize> ping_hash{};
        std::copy( buf.begin(), buf.begin() + discv4::kWireHashSize, ping_hash.begin() );

        const auto response_wire = make_pong_wire( ping_hash, sender_endpoint );
        peer_socket.send_to( boost::asio::buffer( response_wire ), sender_endpoint, 0, ec );
    } );

    io.run_for( std::chrono::milliseconds( 300U ) );
    peer_thread.join();

    EXPECT_TRUE( ping_completed ) << "ping() must complete after receiving a matching PONG";
    EXPECT_TRUE( ping_succeeded ) << "ping() must succeed when PONG pingHash matches the outbound PING";
}

/// @brief ping() times out when a PONG arrives from the endpoint with the wrong pingHash.
TEST(RecursiveBondingTest, PingIgnoresPongWithWrongToken)
{
    constexpr uint16_t kPeerPort = 30466U;

    boost::asio::io_context peer_io;
    udp::socket peer_socket( peer_io );
    {
        boost::system::error_code ec;
        peer_socket.open( udp::v4(), ec );
        ASSERT_FALSE( ec ) << "open() failed";

        peer_socket.bind( udp::endpoint( udp::v4(), kPeerPort ), ec );
        if ( ec )
        {
            GTEST_SKIP() << "Port " << kPeerPort << " unavailable — skipping loopback test";
        }
    }

    boost::asio::io_context io;
    auto cfg = make_cfg( io );
    auto dv4 = std::make_shared<discv4::discv4_client>( io, cfg );
    ASSERT_TRUE( dv4->start() ) << "discv4_client::start() failed";

    discv4::NodeId dummy_id{};
    std::atomic<bool> ping_completed{ false };
    std::atomic<bool> ping_succeeded{ false };

    boost::asio::spawn( io,
        [dv4, &ping_completed, &ping_succeeded, peer_port = kPeerPort, dummy_id]( boost::asio::yield_context yield )
        {
            auto result = dv4->ping( "127.0.0.1", peer_port, dummy_id, yield );
            ping_succeeded = result.has_value();
            ping_completed = true;
        } );

    std::thread peer_thread( [&peer_socket]()
    {
        std::array<uint8_t, 2048U> buf{};
        udp::endpoint sender_endpoint;
        boost::system::error_code ec;

        const std::size_t bytes_received = peer_socket.receive_from(
            boost::asio::buffer( buf ),
            sender_endpoint,
            0,
            ec );
        if ( ec || bytes_received < discv4::kWireHashSize )
        {
            return;
        }

        std::array<uint8_t, discv4::kWireHashSize> wrong_hash{};
        wrong_hash.fill( 0xA5U );

        const auto response_wire = make_pong_wire( wrong_hash, sender_endpoint );
        peer_socket.send_to( boost::asio::buffer( response_wire ), sender_endpoint, 0, ec );
    } );

    io.run_for( std::chrono::milliseconds( 300U ) );
    peer_thread.join();

    EXPECT_TRUE( ping_completed ) << "ping() must complete after timing out on a mismatched PONG";
    EXPECT_FALSE( ping_succeeded ) << "ping() must ignore a PONG whose pingHash does not match the outbound PING";
}

