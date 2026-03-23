/// @file discv4_client_test.cpp
/// @brief Unit tests for discv4_client lifetime and receive-loop behaviour.
///
/// These tests guard against the bug where the discv4_client shared_ptr goes
/// out of scope before io.run(), destroying the socket and silently killing
/// all UDP traffic.

#include <discv4/discv4_client.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <array>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
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
