/// @file discv4_client_test.cpp
/// @brief Unit tests for discv4_client lifetime and receive-loop behaviour.
///
/// These tests guard against the bug where the discv4_client shared_ptr goes
/// out of scope before io.run(), destroying the socket and silently killing
/// all UDP traffic.

#include <discv4/discv4_client.hpp>
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <atomic>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

using boost::asio::ip::udp;

/// @brief A minimal UDP listener that records the first datagram it receives.
///        Uses a plain blocking socket in a background thread — no shared_ptrs
///        that could race with the test's io_context.
class UdpListener {
public:
    explicit UdpListener(uint16_t port)
    {
        // Bind a UDP socket synchronously so port() is valid immediately.
        sockfd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        EXPECT_GE(sockfd_, 0) << "socket() failed";

        struct timeval tv{};
        tv.tv_sec  = 0;
        tv.tv_usec = 100000;  // 100 ms receive timeout so the thread can poll running_
        ::setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);
        EXPECT_EQ(::bind(sockfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0)
            << "bind() failed on port " << port;

        sockaddr_in bound{};
        socklen_t   len = sizeof(bound);
        ::getsockname(sockfd_, reinterpret_cast<sockaddr*>(&bound), &len);
        port_ = ntohs(bound.sin_port);
    }

    ~UdpListener() { stop(); }

    /// @brief Start listening in a background thread.
    void start()
    {
        running_ = true;
        thread_  = std::thread([this] {
            std::array<uint8_t, 2048> buf{};
            while (running_) {
                ssize_t n = ::recv(sockfd_, buf.data(), buf.size(), 0);
                if (n > 0) { received_ = true; }
            }
        });
    }

    /// @brief Stop the listener.
    void stop()
    {
        running_ = false;
        if (sockfd_ >= 0) { ::close(sockfd_); sockfd_ = -1; }
        if (thread_.joinable()) { thread_.join(); }
    }

    /// @brief True if at least one datagram was received.
    bool received() const { return received_.load(); }

    uint16_t port() const { return port_; }

private:
    int                 sockfd_{-1};
    uint16_t            port_{0};
    std::atomic<bool>   received_{false};
    std::atomic<bool>   running_{false};
    std::thread         thread_;
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

    boost::asio::co_spawn(io,
        [dv4, port = listener.port(), &dummy_id]() -> boost::asio::awaitable<void> {
            [[maybe_unused]] auto r = co_await dv4->ping("127.0.0.1", port, dummy_id);
        },
        boost::asio::detached);

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
    boost::asio::co_spawn(io,
        [dv4, p1 = listener1.port(), &dummy_id]() -> boost::asio::awaitable<void> {
            [[maybe_unused]] auto r = co_await dv4->ping("127.0.0.1", p1, dummy_id);
        },
        boost::asio::detached);

    boost::asio::co_spawn(io,
        [dv4, p2 = listener2.port(), &dummy_id]() -> boost::asio::awaitable<void> {
            [[maybe_unused]] auto r = co_await dv4->ping("127.0.0.1", p2, dummy_id);
        },
        boost::asio::detached);

    io.run_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(listener1.received())
        << "PING must reach first discovered peer when dv4 is in outer scope";
    EXPECT_TRUE(listener2.received())
        << "PING must reach second discovered peer when dv4 is in outer scope";
}
