// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <iomanip>
#include <boost/asio/spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/redirect_error.hpp>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <eth/messages.hpp>
#include <eth/eth_watch_service.hpp>
#include <eth/eth_watch_cli.hpp>
#include <discv4/bootnodes.hpp>
#include <discv4/bootnodes_test.hpp>
#include <discv4/dial_history.hpp>
#include <discv4/dial_scheduler.hpp>
#include <discv4/discv4_client.hpp>
#include <rlpx/crypto/ecdh.hpp>
#include <rlpx/rlpx_error.hpp>
#include <rlpx/rlpx_session.hpp>
#include <base/rlp-logger.hpp>

namespace {

enum class DiscoveryMode {
    kDiscv4,
    kDiscv5,
};

struct Config {
    std::string host;
    uint16_t port = 0;
    std::string peer_pubkey_hex;
    uint8_t eth_offset = 0x10;
    std::vector<eth::cli::WatchSpec> watch_specs;
    // ETH Status fields — must match the target chain
    uint64_t network_id = 1;
    eth::Hash256 genesis_hash{};
    eth::ForkId  fork_id{};   ///< EIP-2124 fork identifier; set per chain
    // Discovery — set when --chain is used; empty when explicit host/port/pubkey given
    std::vector<std::string> bootnode_enodes;
    DiscoveryMode discovery_mode = DiscoveryMode::kDiscv4;
};

std::optional<uint8_t> hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<uint8_t>(10 + (c - 'a'));
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<uint8_t>(10 + (c - 'A'));
    }
    return std::nullopt;
}

template <size_t N>
bool parse_hex_array(std::string_view hex, std::array<uint8_t, N>& out) {
    if (hex.size() != N * 2) {
        return false;
    }
    for (size_t i = 0; i < N; ++i) {
        const size_t index = i * 2;
        auto hi = hex_to_nibble(hex.at(index));
        auto lo = hex_to_nibble(hex.at(index + 1));
        if (!hi || !lo) {
            return false;
        }
        out.at(i) = static_cast<uint8_t>(((*hi) << 4) | *lo);
    }
    return true;
}

std::optional<uint16_t> parse_uint16(std::string_view value) {
    uint16_t out = 0;
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
    if (ec != std::errc{} || ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return out;
}

std::optional<uint8_t> parse_uint8(std::string_view value) {
    unsigned int out = 0;
    auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
    if (ec != std::errc{} || ptr != value.data() + value.size() || out > 0xFFU) {
        return std::nullopt;
    }
    return static_cast<uint8_t>(out);
}


std::optional<Config> parse_enode(std::string_view enode) {
    constexpr std::string_view kPrefix = "enode://";
    if (enode.size() < kPrefix.size() || enode.substr(0, kPrefix.size()) != kPrefix) {
        return std::nullopt;
    }

    const auto without_prefix = enode.substr(kPrefix.size());
    const auto at_pos = without_prefix.find('@');
    if (at_pos == std::string_view::npos) {
        return std::nullopt;
    }

    const auto pubkey_hex = without_prefix.substr(0, at_pos);
    if (pubkey_hex.size() != rlpx::kPublicKeySize * 2) {
        return std::nullopt;
    }

    const auto address_part = without_prefix.substr(at_pos + 1);
    const auto query_pos = address_part.find('?');
    const auto host_port = address_part.substr(0, query_pos);
    const auto colon_pos = host_port.rfind(':');
    if (colon_pos == std::string_view::npos) {
        return std::nullopt;
    }

    const auto host_view = host_port.substr(0, colon_pos);
    const auto port_view = host_port.substr(colon_pos + 1);
    auto port_value = parse_uint16(port_view);
    if (!port_value) {
        return std::nullopt;
    }

    Config cfg;
    cfg.host = std::string(host_view);
    cfg.port = *port_value;
    cfg.peer_pubkey_hex = std::string(pubkey_hex);
    return cfg;
}

// ---------------------------------------------------------------------------
// Chain registry — all per-chain constants in one place.
// Adding a new chain requires only one new entry in the map inside
// load_chain_config below.
// ---------------------------------------------------------------------------

/// @brief Decode a 64-char hex literal into a Hash256.
static eth::Hash256 hash_from_hex(const char* hex)
{
    eth::Hash256 out{};
    for (size_t i = 0; i < 32; ++i)
    {
        const auto hi = hex_to_nibble(hex[(i * 2)]).value_or(0);
        const auto lo = hex_to_nibble(hex[(i * 2) + 1]).value_or(0);
        out.at(i) = static_cast<uint8_t>((hi << 4) | lo);
    }
    return out;
}

struct ChainEntry
{
    const std::vector<std::string>* bootnodes;
    uint64_t                        network_id;
    const char*                     genesis_hex;
    eth::ForkId                     fork_id{};  ///< EIP-2124; computed from genesis + past forks
};

/// @brief Look up chain config by name
std::optional<Config> load_chain_config(std::string_view chain_name)
{
    // Fork-ids are pre-computed via EIP-2124 for each chain as of early 2025.
    // Sepolia: MergeNetsplit@1735371, Shanghai@1677557088, Cancun@1706655072, Prague@1741159776
    static const eth::ForkId kSepoliaForkId{ { 0xed, 0x88, 0xb5, 0xfd }, 0 };

    static const std::unordered_map<std::string, ChainEntry> kChains = {
        { "mainnet",      ChainEntry{ &ETHEREUM_MAINNET_BOOTNODES, 1,        "d4e56740f876aef8c010b86a40d5f56745a118d0906a34e69aec8c0db1cb8fa3" } },
        { "sepolia",      ChainEntry{ &ETHEREUM_SEPOLIA_BOOTNODES, 11155111, "25a5cc106eea7138acab33231d7160d69cb777ee0c2c553fcddf5138993e6dd9", kSepoliaForkId } },
        { "holesky",      ChainEntry{ &ETHEREUM_HOLESKY_BOOTNODES, 17000,    "b5f7f912443c940f21fd611f12828d75b534364ed9e95ca4e307729a4661bde4" } },
        { "polygon",      ChainEntry{ &POLYGON_MAINNET_BOOTNODES,  137,      "a9c28ce2141b56c474f1dc504bee9b01eb1bd7d1a507580d5519d4437a97de1b" } },
        { "polygon-amoy", ChainEntry{ &POLYGON_AMOY_BOOTNODES,     80002,    "0000000000000000000000000000000000000000000000000000000000000000" } },
        { "bsc",          ChainEntry{ &BSC_MAINNET_BOOTNODES,      56,       "0d21840abff46b96c84b2ac9e10e4f5cdaeb5693cb665db62a2f3b02d2d57b5b" } },
        { "bsc-testnet",  ChainEntry{ &BSC_TESTNET_STATICNODES,    97,       "6d3c66c5357ec91d5c43af47e234a939b22557cbb552dc45bebbceeed90fbe10" } },
        { "base",         ChainEntry{ &BASE_MAINNET_BOOTNODES,     8453,     "f712aa9241cc24369b143cf6dce85f0902a9731e70d66818a3a5845b296c73dd" } },
        { "base-sepolia", ChainEntry{ &BASE_SEPOLIA_BOOTNODES,     84532,    "0dcc9e089e30b90ddfc55be9a37dd15bc551aeee999d2e2b51414c54eaf934e4" } },
    };

    const auto it = kChains.find(std::string(chain_name));
    if (it == kChains.end())
    {
        return std::nullopt;
    }

    const auto& entry = it->second;
    if (entry.bootnodes->empty())
    {
        static auto log = rlp::base::createLogger("eth_watch");
        SPDLOG_LOGGER_ERROR(log, "No bootnodes configured for chain: {}", chain_name);
        return std::nullopt;
    }

    Config cfg;
    cfg.network_id   = entry.network_id;
    cfg.genesis_hash = hash_from_hex(entry.genesis_hex);
    cfg.fork_id      = entry.fork_id;
    // Store all bootnodes for discv4 — host/port/pubkey filled in after discovery
    for (const auto& bn : *entry.bootnodes)
    {
        cfg.bootnode_enodes.push_back(bn);
    }
    return cfg;
}


void print_usage(const char* exe) {
    std::cout << "Usage:\n"
              << "  " << exe << " <host> <port> <peer_pubkey_hex> [eth_offset]\n"
              << "  " << exe << " --chain <chain_name>\n"
              << "  " << exe << " --chain <chain_name> --discovery-mode <discv4|discv5>\n"
              << "\nOptional watch flags (repeatable, must follow connection args):\n"
              << "  --watch-contract <0x20byteHex>   Contract address to filter (omit for any)\n"
              << "  --watch-event    <signature>      Event signature, e.g. Transfer(address,address,uint256)\n"
              << "  Each --watch-event pairs with the preceding --watch-contract (or any contract if none).\n"
              << "\nExamples:\n"
              << "  " << exe << " --chain sepolia --watch-event Transfer(address,address,uint256)\n"
              << "  " << exe << " --chain mainnet --watch-contract 0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48 --watch-event Transfer(address,address,uint256)\n"
              << "\nAvailable chains:\n"
              << "  Ethereum: mainnet, sepolia, holesky\n"
              << "  Polygon:  polygon, polygon-amoy\n"
              << "  BSC:      bsc, bsc-testnet\n"
              << "  Base:     base, base-sepolia\n";
}

/// @brief Attempts an RLPx connection to a peer and runs the ETH watch loop.
///        Calls @p on_done on every exit path so the DialScheduler can recycle
///        the dial slot.  Calls @p on_connected once the session is established
///        so the scheduler can track it for async stop().
void run_watch(std::string host,
               uint16_t port,
               rlpx::PublicKey peer_pubkey,
               uint8_t eth_offset,
               uint64_t network_id,
               eth::Hash256 genesis_hash,
               eth::ForkId fork_id,
               std::vector<eth::cli::WatchSpec> watch_specs,
               std::function<void()> on_done,
               std::function<void(std::shared_ptr<rlpx::RlpxSession>)> on_connected,
               boost::asio::yield_context yield)
{
    static auto log = rlp::base::createLogger("eth_watch");

    auto keypair_result = rlpx::crypto::Ecdh::generate_ephemeral_keypair();
    if (!keypair_result) {
        SPDLOG_LOGGER_ERROR(log, "run_watch: failed to generate local keypair");
        on_done();
        return;
    }

    const auto& keypair = keypair_result.value();

    const rlpx::SessionConnectParams params{
        host,
        port,
        keypair.public_key,
        keypair.private_key,
        peer_pubkey,
        "rlp-eth-watch",
        0
    };

    SPDLOG_LOGGER_DEBUG(log, "run_watch: connecting to {}:{}", host, port);
    auto session_result = rlpx::RlpxSession::connect(params, yield);
    if (!session_result) {
        auto err = session_result.error();
        SPDLOG_LOGGER_DEBUG(log, "run_watch: failed to connect to {}:{} (error {}: {})",
                            host, port, static_cast<int>(err), rlpx::to_string(err));
        on_done();
        return;
    }

    auto session = std::move(session_result.value());

    SPDLOG_LOGGER_DEBUG(log, "run_watch: HELLO from peer: {}", session->peer_info().client_id);
    {
        eth::StatusMessage69 status69;
        status69.protocol_version = 69;
        status69.network_id = network_id;
        status69.genesis_hash = genesis_hash;
        status69.fork_id = fork_id;
        status69.earliest_block = 0;
        status69.latest_block = 0;
        status69.latest_block_hash = genesis_hash;
        eth::StatusMessage status = status69;
        auto encoded = eth::protocol::encode_status(status);
        if (encoded) {
            rlpx::framing::Message status_msg{};
            status_msg.id = static_cast<uint8_t>(eth_offset + eth::protocol::kStatusMessageId);
            status_msg.payload = std::move(encoded.value());
            const auto post_result = session->post_message(std::move(status_msg));
            if (!post_result) {
                SPDLOG_LOGGER_ERROR(log, "run_watch: failed to post ETH Status message");
            } else {
                SPDLOG_LOGGER_DEBUG(log, "run_watch: ETH Status posted (network_id={})", network_id);
            }
        } else {
            SPDLOG_LOGGER_ERROR(log, "run_watch: failed to encode ETH Status message");
        }
    }

    // -------------------------------------------------------------------------
    // EthWatchService — register watches from CLI args (or default to Transfer)
    // -------------------------------------------------------------------------
    auto watch_svc = std::make_shared<eth::EthWatchService>();

    if (watch_specs.empty())
    {
        // Default: watch all Transfer events on any contract
        watch_specs.push_back(eth::cli::WatchSpec{"", "Transfer(address,address,uint256)"});
    }

    for (const auto& spec : watch_specs)
    {
        eth::codec::Address contract{};
        if (!spec.contract_hex.empty())
        {
            auto addr = eth::cli::parse_address(spec.contract_hex);
            if (!addr)
            {
                SPDLOG_LOGGER_ERROR(log, "Invalid contract address: {}", spec.contract_hex);
                on_done();
                return;
            }
            contract = *addr;
        }

        const auto abi_params = eth::cli::infer_params(spec.event_signature);
        const std::string sig_copy = spec.event_signature;

        watch_svc->watch_event(
            contract,
            spec.event_signature,
            abi_params,
            [sig_copy, abi_params](const eth::MatchedEvent& ev, const std::vector<eth::abi::AbiValue>& vals)
            {
                static auto ev_log = rlp::base::createLogger("eth_watch");
                auto bytes_to_hex = [](const auto& arr) {
                    std::string s;
                    s.reserve(arr.size() * 2);
                    for (const auto b : arr) {
                        const char hex[] = "0123456789abcdef";
                        s += hex[(static_cast<uint8_t>(b) >> 4) & 0xf];
                        s += hex[ static_cast<uint8_t>(b)       & 0xf];
                    }
                    return s;
                };

                std::string header = sig_copy + " at block " + std::to_string(ev.block_number);
                if (ev.tx_hash != eth::codec::Hash256{})
                {
                    header += "  tx: 0x" + bytes_to_hex(ev.tx_hash);
                }
                SPDLOG_LOGGER_INFO(ev_log, "{}", header);

                for (size_t i = 0; i < vals.size(); ++i)
                {
                    const std::string label = (i < abi_params.size() && !abi_params[i].name.empty())
                        ? abi_params[i].name
                        : std::to_string(i);
                    std::string value;
                    if (const auto* addr = std::get_if<eth::codec::Address>(&vals[i]))
                    {
                        value = "0x" + bytes_to_hex(*addr);
                    }
                    else if (const auto* u256 = std::get_if<intx::uint256>(&vals[i]))
                    {
                        value = intx::to_string(*u256);
                    }
                    else if (const auto* b32 = std::get_if<eth::codec::Hash256>(&vals[i]))
                    {
                        value = "0x" + bytes_to_hex(*b32);
                    }
                    else if (const auto* bval = std::get_if<bool>(&vals[i]))
                    {
                        value = (*bval ? "true" : "false");
                    }
                    SPDLOG_LOGGER_INFO(ev_log, "  [{}] {}", label, value);
                }
            });

        if (!spec.contract_hex.empty())
        {
            SPDLOG_LOGGER_INFO(log, "Watching: {} on contract {}", spec.event_signature, spec.contract_hex);
        }
        else
        {
            SPDLOG_LOGGER_INFO(log, "Watching: {}", spec.event_signature);
        }
    }

    watch_svc->set_send_callback([session, eth_offset](uint8_t eth_msg_id,
                                                        std::vector<uint8_t> payload)
    {
        rlpx::framing::Message out_msg{};
        out_msg.id = static_cast<uint8_t>(eth_offset + eth_msg_id);
        out_msg.payload = std::move(payload);
        const auto post_result = session->post_message(std::move(out_msg));
        if (!post_result)
        {
            static auto cb_log = rlp::base::createLogger("eth_watch");
            SPDLOG_LOGGER_WARN(cb_log, "send_callback: failed to post eth_msg_id=0x{:02x}", eth_msg_id);
        }
    });

    // Create timers and handshake state.
    // status_timeout: fires in kStatusHandshakeTimeout if peer never sends ETH Status.
    //   Mirrors go-ethereum's waitForHandshake() with handshakeTimeout = 5s.
    // lifetime: cancelled by the disconnect handler to tear down the session.
    auto executor = yield.get_executor();
    auto status_received  = std::make_shared<std::atomic<bool>>(false);
    auto status_timeout   = std::make_shared<boost::asio::steady_timer>(executor);
    auto lifetime         = std::make_shared<boost::asio::steady_timer>(executor);
    status_timeout->expires_after(eth::protocol::kStatusHandshakeTimeout);
    lifetime->expires_after(std::chrono::hours(24 * 365));

    session->set_disconnect_handler([session, lifetime, status_timeout](const rlpx::protocol::DisconnectMessage& msg) {
        static auto disc_log = rlp::base::createLogger("eth_watch");
        (void)session;
        SPDLOG_LOGGER_DEBUG(disc_log, "run_watch: Disconnected reason={}", static_cast<int>(msg.reason));
        lifetime->cancel();
        status_timeout->cancel();
    });

    session->set_ping_handler([session](const rlpx::protocol::PingMessage&) {
        const rlpx::protocol::PongMessage pong;
        auto encoded = pong.encode();
        if (!encoded) { return; }
        rlpx::framing::Message pong_msg{};
        pong_msg.id = rlpx::kPongMessageId;
        pong_msg.payload = std::move(encoded.value());
        const auto post_result = session->post_message(std::move(pong_msg));
        if (!post_result) { return; }
    });

    session->set_generic_handler([session, eth_offset, network_id, genesis_hash, watch_svc,
                                   status_received, status_timeout, on_connected](const rlpx::protocol::Message& msg) {
        (void)session;
        static auto gh_log = rlp::base::createLogger("eth_watch");
        if (msg.id < eth_offset) {
            SPDLOG_LOGGER_DEBUG(gh_log, "generic_handler: unknown p2p msg id=0x{:02x}", msg.id);
            return;
        }

        const auto eth_id = static_cast<uint8_t>(msg.id - eth_offset);
        const rlp::ByteView payload(msg.payload.data(), msg.payload.size());

        if (eth_id == eth::protocol::kStatusMessageId) {
            auto decoded = eth::protocol::decode_status(payload);
            if (!decoded) {
                SPDLOG_LOGGER_WARN(gh_log, "generic_handler: ETH Status decode failed, payload_size={}, error={}",
                                   msg.payload.size(), static_cast<int>(decoded.error()));
                status_timeout->cancel(); // validation failed — stop waiting
                (void)session->disconnect(rlpx::DisconnectReason::kSubprotocolError);
                return;
            }
            const auto& status = decoded.value();
            const auto common = eth::get_common_fields(status);
            auto valid = eth::protocol::validate_status(status, network_id, genesis_hash);
            if (!valid) {
                using E = eth::StatusValidationError;
                switch (valid.error()) {
                case E::kProtocolVersionMismatch:
                    SPDLOG_LOGGER_WARN(gh_log, "ETH Status: protocol version not supported (peer={})",
                                       common.protocol_version);
                    break;
                case E::kNetworkIDMismatch:
                    SPDLOG_LOGGER_WARN(gh_log, "ETH Status: network_id mismatch (peer={}, ours={})",
                                       common.network_id, network_id);
                    break;
                case E::kGenesisMismatch:
                    SPDLOG_LOGGER_WARN(gh_log, "ETH Status: genesis mismatch");
                    break;
                case E::kInvalidBlockRange:
                    {
                        const auto* msg69 = std::get_if<eth::StatusMessage69>(&status);
                        const uint64_t earliest = msg69 ? msg69->earliest_block : 0;
                        const uint64_t latest = msg69 ? msg69->latest_block : 0;
                        SPDLOG_LOGGER_WARN(gh_log, "ETH Status: invalid block range (earliest={} > latest={})",
                                           earliest, latest);
                    }
                    break;
                }
                status_timeout->cancel(); // validation failed — stop waiting
                (void)session->disconnect(rlpx::DisconnectReason::kSubprotocolError);
                return;
            }
            // Handshake successful — signal the awaiting coroutine.
            const uint64_t latest_block = std::visit([](const auto& m) -> uint64_t
            {
                if constexpr (std::is_same_v<std::decay_t<decltype(m)>, eth::StatusMessage69>)
                {
                    return m.latest_block;
                }
                return 0;
            }, status);
            SPDLOG_LOGGER_INFO(gh_log, "ETH Status: network_id={} protocol={} latest_block={}",
                               common.network_id,
                               static_cast<int>(common.protocol_version),
                               latest_block);
            status_received->store(true);
            status_timeout->cancel(); // wake the co_await below
            SPDLOG_LOGGER_INFO(gh_log, "Connected. Watching for events...");
            on_connected(session);
            return;
        }

        // Received a non-Status ETH message before the handshake completed.
        // Per go-ethereum (readStatusMsg): the first ETH message MUST be Status.
        if (!status_received->load()) {
            SPDLOG_LOGGER_WARN(gh_log, "generic_handler: non-Status ETH message (id=0x{:02x}) received before handshake",
                               eth_id);
            status_timeout->cancel();
            (void)session->disconnect(rlpx::DisconnectReason::kSubprotocolError);
            return;
        }

        if (eth_id == eth::protocol::kNewBlockHashesMessageId) {
            auto decoded = eth::protocol::decode_new_block_hashes(payload);
            if (decoded) {
                SPDLOG_LOGGER_DEBUG(gh_log, "generic_handler: NewBlockHashes count={}", decoded.value().entries.size());
            } else {
                SPDLOG_LOGGER_WARN(gh_log, "generic_handler: NewBlockHashes decode failed");
            }
            // Fall through to process_message so the service requests receipts
        }

        SPDLOG_LOGGER_DEBUG(gh_log, "generic_handler: ETH msg id=0x{:02x} payload_size={}", eth_id, msg.payload.size());

        // Dispatch to EthWatchService for NewBlockHashes, NewBlock, and Receipts
        watch_svc->process_message(eth_id, payload);
    });

    // ── ETH Status handshake wait (mirrors go-ethereum's waitForHandshake) ────
    // Await the status_timeout timer.  The generic_handler cancels it (with
    // operation_aborted) as soon as it receives a valid peer Status, or on any
    // validation/decode failure.  If it fires naturally the peer is silent
    // (e.g. a Polygon bor node connecting on the Ethereum P2P network).
    {
        boost::system::error_code hs_ec;
        status_timeout->async_wait(
            boost::asio::redirect_error(yield, hs_ec));

        if (!status_received->load()) {
            if (hs_ec != boost::asio::error::operation_aborted) {
                // Timer expired naturally — peer never sent ETH Status.
                SPDLOG_LOGGER_WARN(log, "run_watch: ETH Status handshake timeout ({}:{}) — "
                                   "peer is likely on a different chain", host, port);
                (void)session->disconnect(rlpx::DisconnectReason::kTimeout);
            }
            // else: validation failure — session->disconnect() already called in handler.
            on_done();
            return;
        }
    }
    // status_received == true: handshake complete, now watch until disconnected.

    boost::system::error_code ec;
    lifetime->async_wait(boost::asio::redirect_error(yield, ec));
    on_done();
}

} // namespace

// ── WatcherPool and DialScheduler are defined in include/discv4/dial_scheduler.hpp ──

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage(argv[0]);
            return 1;
        }

        // Parse --log-level first so it takes effect before any loggers are created
        for (int i = 1; i < argc - 1; ++i)
        {
            if (std::string_view(argv[i]) == "--log-level")
            {
                const std::string_view level_str(argv[i + 1]);
                spdlog::level::level_enum lvl = spdlog::level::info;
                if      (level_str == "trace")    { lvl = spdlog::level::trace; }
                else if (level_str == "debug")    { lvl = spdlog::level::debug; }
                else if (level_str == "info")     { lvl = spdlog::level::info; }
                else if (level_str == "warn")     { lvl = spdlog::level::warn; }
                else if (level_str == "error")    { lvl = spdlog::level::err; }
                else if (level_str == "critical") { lvl = spdlog::level::critical; }
                else if (level_str == "off")      { lvl = spdlog::level::off; }
                spdlog::set_level(lvl);
                spdlog::apply_all([lvl](std::shared_ptr<spdlog::logger> l) { l->set_level(lvl); });
                break;
            }
        }

        std::optional<Config> config;
        int next_arg = 1;

        if (std::string_view(argv[next_arg]) == "--chain") {
            if (argc < 3) {
                print_usage(argv[0]);
                return 1;
            }
            const std::string chain_name = argv[next_arg + 1];
            config = load_chain_config(chain_name);
            if (!config) {
                std::cout << "Unknown or unconfigured chain: " << chain_name << "\n"
                          << "Available: mainnet, sepolia, holesky, polygon, polygon-amoy, bsc, bsc-testnet, base, base-sepolia\n";
                return 1;
            }
            next_arg += 2;
        } else if (argc >= 4) {
            const auto port_value = parse_uint16(argv[next_arg + 1]);
            if (!port_value) {
                std::cout << "Invalid port value.\n";
                return 1;
            }

            Config cfg;
            cfg.host = argv[next_arg];
            cfg.port = *port_value;
            cfg.peer_pubkey_hex = argv[next_arg + 2];
            next_arg += 3;

            if (next_arg < argc && std::string_view(argv[next_arg]).find("--") == std::string_view::npos) {
                auto offset_value = parse_uint8(argv[next_arg]);
                if (!offset_value) {
                    std::cout << "Invalid eth_offset value.\n";
                    return 1;
                }
                cfg.eth_offset = *offset_value;
                ++next_arg;
            }
            config = cfg;
        } else {
            print_usage(argv[0]);
            return 1;
        }

        // Parse optional --watch-contract / --watch-event flags
        std::string pending_contract;
        while (next_arg < argc) {
            const std::string_view arg(argv[next_arg]);

            if (arg == "--watch-contract") {
                if (next_arg + 1 >= argc) {
                    std::cout << "--watch-contract requires an address argument.\n";
                    return 1;
                }
                pending_contract = argv[next_arg + 1];
                next_arg += 2;
            } else if (arg == "--watch-event") {
                if (next_arg + 1 >= argc) {
                    std::cout << "--watch-event requires a signature argument.\n";
                    return 1;
                }
                eth::cli::WatchSpec spec;
                spec.contract_hex    = pending_contract;
                spec.event_signature = argv[next_arg + 1];
                config->watch_specs.push_back(std::move(spec));
                pending_contract.clear();
                next_arg += 2;
            } else if (arg == "--log-level") {
                if (next_arg + 1 >= argc) {
                    std::cout << "--log-level requires a level argument (trace, debug, info, warn, error, critical, off).\n";
                    return 1;
                }
                const std::string_view level_str(argv[next_arg + 1]);
                if (level_str == "trace")         { spdlog::set_level(spdlog::level::trace); }
                else if (level_str == "debug")    { spdlog::set_level(spdlog::level::debug); }
                else if (level_str == "info")     { spdlog::set_level(spdlog::level::info); }
                else if (level_str == "warn")     { spdlog::set_level(spdlog::level::warn); }
                else if (level_str == "error")    { spdlog::set_level(spdlog::level::err); }
                else if (level_str == "critical") { spdlog::set_level(spdlog::level::critical); }
                else if (level_str == "off")      { spdlog::set_level(spdlog::level::off); }
                else
                {
                    std::cout << "Unknown log level: " << level_str << "\n";
                    return 1;
                }
                next_arg += 2;
            } else if (arg == "--discovery-mode") {
                if (next_arg + 1 >= argc) {
                    std::cout << "--discovery-mode requires a value (discv4|discv5).\n";
                    return 1;
                }
                const std::string_view mode(argv[next_arg + 1]);
                if (mode == "discv4") {
                    config->discovery_mode = DiscoveryMode::kDiscv4;
                } else if (mode == "discv5") {
                    config->discovery_mode = DiscoveryMode::kDiscv5;
                } else {
                    std::cout << "Unknown discovery mode: " << mode << "\n";
                    return 1;
                }
                next_arg += 2;
            } else {
                std::cout << "Unknown argument: " << arg << "\n";
                print_usage(argv[0]);
                return 1;
            }
        }

        boost::asio::io_context io;
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            io.stop();
        });

        // dv4 declared outside the if-block so it lives past io.run().
        // If --chain mode is not used, this stays null (no-op).
        std::shared_ptr<discv4::discv4_client> dv4;

        if (!config->bootnode_enodes.empty())
        {
            if (config->discovery_mode == DiscoveryMode::kDiscv5)
            {
                std::cout << "--discovery-mode discv5 is not wired in eth_watch yet; use discv4 for now.\n";
                return 1;
            }

            // --chain mode: use discv4 to find a real full node, then connect via RLPx
            auto keypair_result = rlpx::crypto::Ecdh::generate_ephemeral_keypair();
            if (!keypair_result)
            {
                std::cout << "Failed to generate keypair for discv4.\n";
                return 1;
            }
            const auto& keypair = keypair_result.value();

            discv4::discv4Config dv4_cfg;
            dv4_cfg.bind_port = 0; // OS-assigned ephemeral port
            std::copy(keypair.private_key.begin(), keypair.private_key.end(),
                      dv4_cfg.private_key.begin());
            std::copy(keypair.public_key.begin(), keypair.public_key.end(),
                      dv4_cfg.public_key.begin());

            dv4 = std::make_shared<discv4::discv4_client>(io, dv4_cfg);

            // Capture config values needed in the callback
            const uint64_t   network_id   = config->network_id;
            const auto       genesis_hash = config->genesis_hash;
            const auto       fork_id      = config->fork_id;
            const uint8_t    eth_offset   = config->eth_offset;
            const auto       watch_specs  = config->watch_specs;

            // Two-level resource caps — desktop defaults (10 per chain, 200 total).
            // Embedding apps pass platform-appropriate values:
            //   mobile: WatcherPool(12, 3)   desktop: WatcherPool(200, 10)
            auto pool = std::make_shared<discv4::WatcherPool>(200, 10);
            auto scheduler = std::make_shared<discv4::DialScheduler>(
                io, pool,
                [eth_offset, network_id, genesis_hash, fork_id, watch_specs]
                (discv4::ValidatedPeer                                            vp,
                 std::function<void()>                                            on_done,
                 std::function<void(std::shared_ptr<rlpx::RlpxSession>)>         on_connected,
                 boost::asio::yield_context                                       yc)
                {
                    run_watch(vp.peer.ip, vp.peer.tcp_port, vp.pubkey,
                              eth_offset, network_id, genesis_hash, fork_id, watch_specs,
                              std::move(on_done), std::move(on_connected), yc);
                });

            dv4->set_peer_discovered_callback(
                [scheduler](const discv4::DiscoveredPeer& peer)
                {
                    discv4::ValidatedPeer vp;
                    vp.peer = peer;
                    std::copy(peer.node_id.begin(), peer.node_id.end(), vp.pubkey.begin());
                    if (!rlpx::crypto::Ecdh::verify_public_key(vp.pubkey))
                    {
                        return;
                    }
                    scheduler->enqueue(std::move(vp));
                });

            dv4->set_error_callback([](const std::string& err) {
                std::cout << "discv4 error: " << err << "\n";
            });

            // Ping all bootnodes to seed discovery — wrap in void coroutine
            // because ping() returns Result<pong> which has a deleted default ctor
            for (const auto& enode : config->bootnode_enodes)
            {
                const auto bn = parse_enode(enode);
                if (!bn)
                {
                    continue;
                }
                discv4::NodeId bn_id{};
                if (!parse_hex_array(bn->peer_pubkey_hex, bn_id))
                {
                    continue;
                }
                boost::asio::spawn(io,
                    [dv4, host = bn->host, port = bn->port, bn_id](boost::asio::yield_context yc)
                    {
                        // find_node internally calls ensure_bond (ping→pong) then sends FIND_NODE
                        auto result = dv4->find_node(host, port, bn_id, yc);
                        (void)result;
                    });
            }

            const auto start_result = dv4->start();
            if (!start_result)
            {
                std::cout << "Failed to start discv4.\n";
                return 1;
            }
            std::cout << "Running discv4 peer discovery...\n";
        }
        else
        {
            // Explicit host/port/pubkey mode — connect directly
            rlpx::PublicKey peer_pubkey{};
            if (!parse_hex_array(config->peer_pubkey_hex, peer_pubkey))
            {
                std::cout << "Invalid peer public key hex (expected 128 hex chars).\n";
                return 1;
            }

            boost::asio::spawn(io,
                [host = config->host, port = config->port, peer_pubkey,
                 eth_offset = config->eth_offset,
                 network_id = config->network_id,
                 genesis_hash = config->genesis_hash,
                 fork_id = config->fork_id,
                 watch_specs = std::move(config->watch_specs)](boost::asio::yield_context yc)
                {
                    run_watch(host, port, peer_pubkey,
                              eth_offset, network_id, genesis_hash, fork_id,
                              watch_specs,
                              []() {},
                              [](std::shared_ptr<rlpx::RlpxSession>) {},
                              yc);
                });
        }

        io.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cout << "Unhandled exception: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cout << "Unhandled exception.\n";
        return 1;
    }
}

