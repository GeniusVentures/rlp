// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0
//
// examples/discovery/test_discovery.cpp
//
// Functional test for discv4 peer discovery + RLPx ETH Status handshake
// against live Sepolia bootnodes.  Uses DialScheduler to maintain concurrent
// outbound dials and verifies that at least MIN_CONNECTIONS peers complete the
// ETH/68+69 Status handshake on the correct chain (network_id=11155111).
//
// Checks (GTest-style output):
//   1. At least one bootnode bond completes (PING→PONG)
//   2. At least MIN_PEERS neighbour peers discovered
//   3. At least MIN_CONNECTIONS peers complete the Sepolia ETH Status handshake
//
// Exit code 0 = all checks pass, 1 = any check failed.
//
// Usage:
//   ./test_discovery [--log-level debug] [--timeout 60] [--connections 3]

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

#include <discv4/bootnodes_test.hpp>
#include <discv4/dial_scheduler.hpp>
#include <discv4/discv4_client.hpp>
#include <eth/eth_types.hpp>
#include <eth/messages.hpp>
#include <rlpx/crypto/ecdh.hpp>
#include <rlpx/framing/message_stream.hpp>
#include <rlpx/protocol/messages.hpp>
#include <rlpx/rlpx_error.hpp>
#include <rlpx/rlpx_session.hpp>
#include <base/logger.hpp>

#include <spdlog/spdlog.h>

#include "../chain_config.hpp"

// ── Sepolia chain constants ───────────────────────────────────────────────────

static constexpr uint64_t kSepoliaNetworkId = 11155111;
static constexpr uint8_t  kEthOffset        = 0x10;

static eth::Hash256 sepolia_genesis()
{
    // 25a5cc106eea7138acab33231d7160d69cb777ee0c2c553fcddf5138993e6dd9
    eth::Hash256 h{};
    const char* hex = "25a5cc106eea7138acab33231d7160d69cb777ee0c2c553fcddf5138993e6dd9";
    for (size_t i = 0; i < 32; ++i)
    {
        auto nibble = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + c - 'a');
            return 0;
        };
        h[i] = static_cast<uint8_t>((nibble(hex[i*2]) << 4) | nibble(hex[i*2+1]));
    }
    return h;
}

// Sepolia post-BPO2 fallback hash — used only when chains.json is not found.
// Update chains.json instead of this constant when the fork advances.
static const std::array<uint8_t, 4U> kSepoliaForkHashFallback{ 0x26, 0x89, 0x56, 0xb6 };

// ── Test framework ────────────────────────────────────────────────────────────

namespace {

struct TestSuite
{
    int run = 0, passed = 0, failed = 0;
    std::string current;

    void start(const std::string& name)
    {
        current = name;
        ++run;
        std::cout << "[ RUN      ] " << name << "\n";
    }
    void pass(const std::string& detail = "")
    {
        ++passed;
        std::cout << "[       OK ] " << current << "\n";
        if (!detail.empty()) std::cout << "            " << detail << "\n";
    }
    void fail(const std::string& detail = "")
    {
        ++failed;
        std::cout << "[  FAILED  ] " << current << "\n";
        if (!detail.empty()) std::cout << "            " << detail << "\n";
    }
    void header(int n)
    {
        std::cout << "\n[==========] DiscoveryTest (" << n << " checks)\n\n";
    }
    void footer()
    {
        std::cout << "\n[==========] " << run << " check(s)\n";
        std::cout << "[  PASSED  ] " << passed << "\n";
        if (failed) std::cout << "[  FAILED  ] " << failed << "\n";
        std::cout << "\n";
    }
};

} // namespace

// ── Dial-attempt statistics ───────────────────────────────────────────────────

struct DialStats
{
    std::atomic<int> dialed{0};                     ///< total dial attempts started
    std::atomic<int> connect_failed{0};             ///< TCP / auth / pre-HELLO Disconnect
    std::atomic<int> wrong_chain{0};                ///< Status received but wrong network_id
    std::atomic<int> status_timeout{0};             ///< no Status within timeout (not TooManyPeers)
    std::atomic<int> too_many_peers{0};             ///< TooManyPeers before chain confirmed
    std::atomic<int> too_many_peers_right_chain{0}; ///< TooManyPeers after chain confirmed
    std::atomic<int> connected{0};                  ///< right chain, Status validated
};

// Does not set up EthWatchService — just validates the chain and returns.

static void dial_connect_only(
    discv4::ValidatedPeer                                           vp,
    std::function<void()>                                          on_done,
    std::function<void(std::shared_ptr<rlpx::RlpxSession>)>       on_connected,
    boost::asio::yield_context                                     yield,
    std::shared_ptr<DialStats>                                     stats,
    eth::ForkId                                                    fork_id)
{
    static auto log = rlp::base::createLogger("test_discovery");
    ++stats->dialed;

    auto keypair_result = rlpx::crypto::Ecdh::generate_ephemeral_keypair();
    if (!keypair_result)
    {
        ++stats->connect_failed;
        on_done();
        return;
    }
    const auto& keypair = keypair_result.value();

    const rlpx::SessionConnectParams params{
        .remote_host       = vp.peer.ip,
        .remote_port       = vp.peer.tcp_port,
        .local_public_key  = keypair.public_key,
        .local_private_key = keypair.private_key,
        .peer_public_key   = vp.pubkey,
        .client_id         = "rlp-test-discovery",
        .listen_port       = 0
    };

    auto session_result = rlpx::RlpxSession::connect(params, yield);
    if (!session_result)
    {
        ++stats->connect_failed;
        on_done();
        return;
    }
    auto session = std::move(session_result.value());

    // Send ETH Status (69)
    {
        const eth::Hash256 genesis = sepolia_genesis();
        eth::StatusMessage69 status69{
            .protocol_version  = 69,
            .network_id        = kSepoliaNetworkId,
            .genesis_hash      = genesis,
            .fork_id           = fork_id,
            .earliest_block    = 0,
            .latest_block      = 0,
            .latest_block_hash = genesis,
        };
        eth::StatusMessage status = status69;
        auto encoded = eth::protocol::encode_status(status);
        if (encoded)
        {
            (void)session->post_message(rlpx::framing::Message{
                .id      = static_cast<uint8_t>(kEthOffset + eth::protocol::kStatusMessageId),
                .payload = std::move(encoded.value())
            });
        }
    }

    auto executor           = yield.get_executor();
    auto status_received    = std::make_shared<std::atomic<bool>>(false);
    auto status_timeout     = std::make_shared<boost::asio::steady_timer>(executor);
    auto lifetime           = std::make_shared<boost::asio::steady_timer>(executor);
    auto disconnect_reason  = std::make_shared<std::atomic<int>>(
                                  static_cast<int>(rlpx::DisconnectReason::kRequested));
    status_timeout->expires_after(eth::protocol::kStatusHandshakeTimeout);
    lifetime->expires_after(std::chrono::seconds(10)); // stay connected briefly after handshake

    session->set_disconnect_handler(
        [lifetime, status_timeout, disconnect_reason]
        (const rlpx::protocol::DisconnectMessage& msg)
        {
            disconnect_reason->store(static_cast<int>(msg.reason));
            lifetime->cancel();
            status_timeout->cancel();
        });

    session->set_ping_handler([session](const rlpx::protocol::PingMessage&) {
        const rlpx::protocol::PongMessage pong;
        auto encoded = pong.encode();
        if (!encoded) { return; }
        (void)session->post_message(rlpx::framing::Message{
            .id = rlpx::kPongMessageId, .payload = std::move(encoded.value())
        });
    });

    const eth::Hash256 genesis = sepolia_genesis();
    session->set_generic_handler([session, status_received, status_timeout,
                                   on_connected, genesis, stats](const rlpx::protocol::Message& msg)
    {
        static auto gh_log = rlp::base::createLogger("test_discovery");
        if (msg.id < kEthOffset) { return; }
        const auto eth_id = static_cast<uint8_t>(msg.id - kEthOffset);
        if (eth_id != eth::protocol::kStatusMessageId) { return; }

        const rlp::ByteView payload(msg.payload.data(), msg.payload.size());
        auto decoded = eth::protocol::decode_status(payload);
        if (!decoded)
        {
            status_timeout->cancel();
            (void)session->disconnect(rlpx::DisconnectReason::kSubprotocolError);
            return;
        }
        auto valid = eth::protocol::validate_status(decoded.value(), kSepoliaNetworkId, genesis);
        if (!valid)
        {
            SPDLOG_LOGGER_DEBUG(gh_log, "ETH Status validation failed: {}",
                                static_cast<int>(valid.error()));
            ++stats->wrong_chain;
            status_timeout->cancel();
            (void)session->disconnect(rlpx::DisconnectReason::kSubprotocolError);
            return;
        }
        ++stats->connected;
        status_received->store(true);
        status_timeout->cancel();
        on_connected(session);
    });

    boost::system::error_code hs_ec;
    status_timeout->async_wait(boost::asio::redirect_error(yield, hs_ec));

    if (!status_received->load())
    {
        if (hs_ec)  // timer was cancelled — peer disconnected us before Status
        {
            const auto reason = static_cast<rlpx::DisconnectReason>(disconnect_reason->load());
            if (reason == rlpx::DisconnectReason::kTooManyPeers)
            {
                ++stats->too_many_peers;
            }
            else
            {
                ++stats->connect_failed;
            }
        }
        else  // timer fired naturally — no Status received within timeout
        {
            ++stats->status_timeout;
        }
        (void)session->disconnect(rlpx::DisconnectReason::kTimeout);
        on_done();
        return;
    }

    // Stay briefly connected so on_connected can be counted
    boost::system::error_code lt_ec;
    lifetime->async_wait(boost::asio::redirect_error(yield, lt_ec));
    on_done();
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    int timeout_secs    = 180;
    int min_connections = 3;
    int min_peers       = 3;
    int max_dials       = 16;  // target dialed peers (go-ethereum: MaxPeers/dialRatio = 50/3 ≈ 16)
                               // active concurrent attempts = min(target*2, 50) per go-ethereum's freeDialSlots()

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg(argv[i]);
        if (arg == "--log-level" && i + 1 < argc)
        {
            std::string_view lvl(argv[++i]);
            if (lvl == "debug")    spdlog::set_level(spdlog::level::debug);
            else if (lvl == "info") spdlog::set_level(spdlog::level::info);
            else if (lvl == "warn") spdlog::set_level(spdlog::level::warn);
            else if (lvl == "off")  spdlog::set_level(spdlog::level::off);
        }
        else if (arg == "--timeout" && i + 1 < argc)   { timeout_secs    = std::atoi(argv[++i]); }
        else if (arg == "--connections" && i + 1 < argc){ min_connections = std::atoi(argv[++i]); }
        else if (arg == "--peers" && i + 1 < argc)      { min_peers       = std::atoi(argv[++i]); }
        else if (arg == "--dials" && i + 1 < argc)      { max_dials       = std::atoi(argv[++i]); }
    }

    // ── Fork hash — loaded from chains.json, fallback to compiled-in value ──────
    const auto loaded_hash = load_fork_hash( "sepolia", argv[0] );
    if ( !loaded_hash )
    {
        std::cout << "[  WARN    ] chains.json not found or missing 'sepolia' key — "
                     "using compiled-in fallback hash.\n";
    }
    const eth::ForkId sepolia_fork_id{
        loaded_hash.value_or( kSepoliaForkHashFallback ),
        0
    };

    TestSuite suite;
    suite.header(3);

    boost::asio::io_context io;

    // Shared result counters (written only from the single io_context thread)
    std::atomic<int> peers_count{0};
    auto stats = std::make_shared<DialStats>();

    // ── discv4 setup ─────────────────────────────────────────────────────────
    auto keypair_result = rlpx::crypto::Ecdh::generate_ephemeral_keypair();
    if (!keypair_result)
    {
        std::cout << "Failed to generate keypair\n";
        return 1;
    }
    const auto& keypair = keypair_result.value();

    discv4::discv4Config dv4_cfg;
    dv4_cfg.bind_port = 0;
    std::copy(keypair.private_key.begin(), keypair.private_key.end(), dv4_cfg.private_key.begin());
    std::copy(keypair.public_key.begin(),  keypair.public_key.end(),  dv4_cfg.public_key.begin());

    auto dv4 = std::make_shared<discv4::discv4_client>(io, dv4_cfg);

    // ── Overall test timeout ─────────────────────────────────────────────────
    boost::asio::steady_timer deadline(io, std::chrono::seconds(timeout_secs));

    // ── DialScheduler ────────────────────────────────────────────────────────
    const int kMaxActiveDials = 50;
    auto pool = std::make_shared<discv4::WatcherPool>(kMaxActiveDials, max_dials * 2);

    auto sched_ref = std::make_shared<discv4::DialScheduler*>(nullptr);

    auto scheduler = std::make_shared<discv4::DialScheduler>(io, pool,
        [&io, &deadline, min_connections, sched_ref, stats, sepolia_fork_id]
        (discv4::ValidatedPeer                                      vp,
         std::function<void()>                                      on_done,
         std::function<void(std::shared_ptr<rlpx::RlpxSession>)>   on_connected,
         boost::asio::yield_context                                 yc) mutable
        {
            dial_connect_only(vp, std::move(on_done),
                [on_connected, &io, &deadline, min_connections, sched_ref]
                (std::shared_ptr<rlpx::RlpxSession> s) mutable
                {
                    on_connected(s);  // increments total_validated
                    if (*sched_ref && (*sched_ref)->total_validated >= min_connections)
                    {
                        deadline.cancel();
                        io.stop();
                    }
                },
                yc, stats, sepolia_fork_id);
        });
    *sched_ref = scheduler.get();

    // Pre-dial ENR chain filter: only enqueue peers whose ENR `eth` entry carries
    // the correct Sepolia fork hash.  Mirrors go-ethereum NewNodeFilter.
    // Peers with no eth_fork_id (ENR absent or no `eth` entry) are also dropped.
    scheduler->filter_fn = discv4::make_fork_id_filter( sepolia_fork_id.fork_hash );

    dv4->set_peer_discovered_callback(
        [scheduler, &peers_count](const discv4::DiscoveredPeer& peer)
        {
            discv4::ValidatedPeer vp;
            vp.peer = peer;
            std::copy(peer.node_id.begin(), peer.node_id.end(), vp.pubkey.begin());
            if (!rlpx::crypto::Ecdh::verify_public_key(vp.pubkey)) { return; }
            ++peers_count;
            scheduler->enqueue(std::move(vp));
        });

    dv4->set_error_callback([](const std::string&) {});

    deadline.async_wait([&](boost::system::error_code) {
        scheduler->stop();
        dv4->stop();
        io.stop();
    });

    // ── Signal handler ───────────────────────────────────────────────────────
    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](boost::system::error_code, int) {
        deadline.cancel();
        scheduler->stop();
        dv4->stop();
        io.stop();
    });

    // ── Seed discovery with Sepolia bootnodes ─────────────────────────────────
    auto parse_enode = [](const std::string& enode)
        -> std::optional<std::tuple<std::string, uint16_t, std::string>>
    {
        // enode://<pubkey>@<host>:<port>
        const std::string prefix = "enode://";
        if (enode.substr(0, prefix.size()) != prefix) { return std::nullopt; }
        const auto at = enode.find('@', prefix.size());
        if (at == std::string::npos) { return std::nullopt; }
        const auto colon = enode.rfind(':');
        if (colon == std::string::npos || colon < at) { return std::nullopt; }
        std::string pubkey = enode.substr(prefix.size(), at - prefix.size());
        std::string host   = enode.substr(at + 1, colon - at - 1);
        uint16_t port      = static_cast<uint16_t>(std::stoi(enode.substr(colon + 1)));
        return std::make_tuple(host, port, pubkey);
    };

    auto hex_to_nibble = [](char c) -> std::optional<uint8_t> {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + c - 'a');
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + c - 'A');
        return std::nullopt;
    };

    const auto start_result = dv4->start();
    if (!start_result)
    {
        std::cout << "Failed to start discv4\n";
        return 1;
    }

    for (const auto& enode : ETHEREUM_SEPOLIA_BOOTNODES)
    {
        auto parsed = parse_enode(enode);
        if (!parsed) { continue; }
        const auto& [host, port, pubkey_hex] = *parsed;
        if (pubkey_hex.size() != 128) { continue; }
        discv4::NodeId bn_id{};
        bool ok = true;
        for (size_t i = 0; i < 64 && ok; ++i)
        {
            auto hi = hex_to_nibble(pubkey_hex[i*2]);
            auto lo = hex_to_nibble(pubkey_hex[i*2+1]);
            if (!hi || !lo) { ok = false; break; }
            bn_id[i] = static_cast<uint8_t>((*hi << 4) | *lo);
        }
        if (!ok) { continue; }
        std::string  host_copy = host;
        uint16_t     port_copy = port;
        boost::asio::spawn(io,
            [dv4, host_copy, port_copy, bn_id](boost::asio::yield_context yc)
            {
                (void)dv4->find_node(host_copy, port_copy, bn_id, yc);
            });
    }

    io.run();

    // ── Dial breakdown ────────────────────────────────────────────────────────
    std::cout << "\n[  STATS   ] Dial breakdown:\n"
              << "              dialed:                       " << stats->dialed.load()                     << "\n"
              << "              connect failed:               " << stats->connect_failed.load()              << "\n"
              << "              wrong chain:                  " << stats->wrong_chain.load()                 << "\n"
              << "              too many peers:               " << stats->too_many_peers.load()              << "\n"
              << "              too many peers (right chain): " << stats->too_many_peers_right_chain.load()  << "\n"
              << "              status timeout:               " << stats->status_timeout.load()              << "\n"
              << "              connected (right chain):      " << stats->connected.load()                   << "\n";

    // ── Results ───────────────────────────────────────────────────────────────
    const int connections = scheduler->total_validated;

    suite.start("DiscoveryTest.BootnodeBondComplete");
    // bonds_count: we infer from the fact that peers were discovered (discv4 bonds internally)
    if (peers_count.load() > 0)
        suite.pass(std::to_string(peers_count.load()) + " neighbour peer(s) discovered");
    else
        suite.fail("No peers discovered — PING→PONG bond may have failed (firewall / UDP 30303?)");

    suite.start("DiscoveryTest.RecursiveDiscovery");
    if (peers_count.load() >= min_peers)
        suite.pass(std::to_string(peers_count.load()) + " peer(s) discovered (min=" + std::to_string(min_peers) + ")");
    else
        suite.fail("Only " + std::to_string(peers_count.load()) + "/" + std::to_string(min_peers) + " peers discovered");

    suite.start("DiscoveryTest.ActiveSepoliaConnections");
    if (connections >= min_connections)
        suite.pass(std::to_string(connections) + " active Sepolia ETH Status connection(s) confirmed");
    else
        suite.fail("Only " + std::to_string(connections) + "/" + std::to_string(min_connections)
                   + " Sepolia connection(s) — run with --log-level debug for details");

    suite.footer();
    // std::exit bypasses stack-variable destructors (including io_context), which avoids
    // boost::coroutines::detail::forced_unwind being thrown during io cleanup when
    // active coroutines are present at shutdown (TCP connect, etc.).
    std::cout.flush();
    std::exit(suite.failed > 0 ? 1 : 0);
}
