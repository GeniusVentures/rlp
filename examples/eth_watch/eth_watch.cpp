// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
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

#include <eth/messages.hpp>
#include <eth/eth_watch_service.hpp>
#include <eth/eth_watch_cli.hpp>
#include <discv4/bootnodes.hpp>
#include <discv4/bootnodes_test.hpp>
#include <discv4/discv4_client.hpp>
#include <rlpx/crypto/ecdh.hpp>
#include <rlpx/rlpx_error.hpp>
#include <rlpx/rlpx_session.hpp>
#include <base/logger.hpp>

namespace {

struct Config {
    std::string host;
    uint16_t port = 0;
    std::string peer_pubkey_hex;
    uint8_t eth_offset = 0x10;
    std::vector<eth::cli::WatchSpec> watch_specs;
    // ETH Status fields — must match the target chain
    uint64_t network_id = 1;
    eth::Hash256 genesis_hash{};
    // Discovery — set when --chain is used; empty when explicit host/port/pubkey given
    std::vector<std::string> bootnode_enodes;
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
    if (!enode.starts_with(kPrefix)) {
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
};

/// @brief Look up chain config by name
std::optional<Config> load_chain_config(std::string_view chain_name)
{
    static const std::unordered_map<std::string, ChainEntry> kChains = {
        { "mainnet",      ChainEntry{ &ETHEREUM_MAINNET_BOOTNODES, 1,        "d4e56740f876aef8c010b86a40d5f56745a118d0906a34e69aec8c0db1cb8fa3" } },
        { "sepolia",      ChainEntry{ &ETHEREUM_SEPOLIA_BOOTNODES, 11155111, "25a5cc106eea7138acab33231d7160d69cb777ee0c2c553fcddf5138993e6dd9" } },
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
        std::cout << "No bootnodes configured for chain: " << chain_name << "\n";
        return std::nullopt;
    }

    Config cfg;
    cfg.network_id   = entry.network_id;
    cfg.genesis_hash = hash_from_hex(entry.genesis_hex);
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

boost::asio::awaitable<void> run_watch(std::string host,
                                       uint16_t port,
                                       rlpx::PublicKey peer_pubkey,
                                       uint8_t eth_offset,
                                       uint64_t network_id,
                                       eth::Hash256 genesis_hash,
                                       std::vector<eth::cli::WatchSpec> watch_specs)
{
    auto keypair_result = rlpx::crypto::Ecdh::generate_ephemeral_keypair();
    if (!keypair_result) {
        std::cout << "Failed to generate local keypair.\n";
        co_return;
    }

    const auto& keypair = keypair_result.value();

    const rlpx::SessionConnectParams params{
        .remote_host = host,
        .remote_port = port,
        .local_public_key = keypair.public_key,
        .local_private_key = keypair.private_key,
        .peer_public_key = peer_pubkey,
        .client_id = "rlp-eth-watch",
        .listen_port = 0
    };

    auto session_result = co_await rlpx::RlpxSession::connect(params);
    if (!session_result) {
        auto err = session_result.error();
        std::cout << "Failed to connect to " << host << ":" << port << "\n"
                  << "Error code: " << static_cast<int>(err) << "\n"
                  << "Error: " << rlpx::to_string(err) << "\n"
                  << "\nNote: Error 12 (kConnectionFailed) typically means:\n"
                  << "  - Bootstrap node is offline or unreachable\n"
                  << "  - Wrong port number (try 30303 for Ethereum, 30311 for BSC)\n"
                  << "  - Public key doesn't match the bootstrap node\n";
        co_return;
    }

    auto session_unique = std::move(session_result.value());
    auto session = std::shared_ptr<rlpx::RlpxSession>(std::move(session_unique));

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
                std::cout << "Invalid contract address: " << spec.contract_hex << "\n";
                co_return;
            }
            contract = *addr;
        }

        const auto abi_params = eth::cli::infer_params(spec.event_signature);
        const std::string sig_copy = spec.event_signature;

        watch_svc->watch_event(
            contract,
            spec.event_signature,
            abi_params,
            [sig_copy](const eth::MatchedEvent& ev, const std::vector<eth::abi::AbiValue>& vals)
            {
                std::cout << sig_copy << " at block " << ev.block_number << "\n";
                for (size_t i = 0; i < vals.size(); ++i)
                {
                    std::cout << "  [" << i << "] ";
                    if (const auto* addr = std::get_if<eth::codec::Address>(&vals[i]))
                    {
                        std::cout << "address: 0x";
                        for (const auto b : *addr) { std::cout << std::hex << static_cast<int>(b); }
                        std::cout << std::dec;
                    }
                    else if (const auto* u256 = std::get_if<intx::uint256>(&vals[i]))
                    {
                        std::cout << "uint256: " << intx::to_string(*u256);
                    }
                    else if (const auto* b32 = std::get_if<eth::codec::Hash256>(&vals[i]))
                    {
                        std::cout << "bytes32: 0x";
                        for (const auto b : *b32) { std::cout << std::hex << static_cast<int>(b); }
                        std::cout << std::dec;
                    }
                    else if (const auto* bval = std::get_if<bool>(&vals[i]))
                    {
                        std::cout << "bool: " << (*bval ? "true" : "false");
                    }
                    std::cout << "\n";
                }
            });

        std::cout << "Watching: " << spec.event_signature;
        if (!spec.contract_hex.empty())
        {
            std::cout << " on contract " << spec.contract_hex;
        }
        std::cout << "\n";
    }

    watch_svc->set_send_callback([session, eth_offset](uint8_t eth_msg_id,
                                                        std::vector<uint8_t> payload)
    {
        const auto post_result = session->post_message(rlpx::framing::Message{
            .id      = static_cast<uint8_t>(eth_offset + eth_msg_id),
            .payload = std::move(payload)
        });
        if (!post_result)
        {
            std::cout << "Failed to send eth message id=" << static_cast<int>(eth_msg_id) << "\n";
        }
    });

    session->set_hello_handler([session, network_id, genesis_hash, eth_offset](const rlpx::protocol::HelloMessage& msg) {
        std::cout << "HELLO from peer: " << msg.client_id << "\n";

        eth::StatusMessage status{
            .protocol_version = 68,
            .network_id = network_id,
            .total_difficulty = intx::uint256(0),
            .best_hash = genesis_hash,
            .genesis_hash = genesis_hash,
            .fork_id = {}
        };

        auto encoded = eth::protocol::encode_status(status);
        if (encoded) {
            const auto post_result = session->post_message(rlpx::framing::Message{
                .id = static_cast<uint8_t>(eth_offset + eth::protocol::kStatusMessageId),
                .payload = std::move(encoded.value())
            });
            if (!post_result) {
                std::cout << "Failed to send ETH Status message\n";
            } else {
                std::cout << "Sent ETH Status message to peer\n";
            }
        } else {
            std::cout << "Failed to encode ETH Status message\n";
        }
    });

    session->set_disconnect_handler([session](const rlpx::protocol::DisconnectMessage& msg) {
        (void)session;
        std::cout << "Disconnected: reason=" << static_cast<int>(msg.reason) << "\n";
    });

    session->set_ping_handler([session](const rlpx::protocol::PingMessage&) {
        const rlpx::protocol::PongMessage pong;
        auto encoded = pong.encode();
        if (!encoded) { return; }
        const auto post_result = session->post_message(rlpx::framing::Message{
            .id = rlpx::kPongMessageId,
            .payload = std::move(encoded.value())
        });
        if (!post_result) { return; }
    });

    session->set_generic_handler([session, eth_offset, watch_svc](const rlpx::protocol::Message& msg) {
        (void)session;
        if (msg.id < eth_offset) {
            std::cout << "Unknown message id=" << static_cast<int>(msg.id) << "\n";
            return;
        }

        const auto eth_id = static_cast<uint8_t>(msg.id - eth_offset);
        const rlp::ByteView payload(msg.payload.data(), msg.payload.size());

        if (eth_id == eth::protocol::kStatusMessageId) {
            auto decoded = eth::protocol::decode_status(payload);
            if (decoded) {
                std::cout << "ETH STATUS: network_id=" << decoded.value().network_id
                          << " protocol=" << static_cast<int>(decoded.value().protocol_version)
                          << "\n";
            } else {
                std::cout << "Failed to decode ETH STATUS\n";
            }
            return;
        }

        if (eth_id == eth::protocol::kNewBlockHashesMessageId) {
            auto decoded = eth::protocol::decode_new_block_hashes(payload);
            if (decoded) {
                std::cout << "NewBlockHashes: " << decoded.value().entries.size() << " hashes\n";
            } else {
                std::cout << "Failed to decode NewBlockHashes\n";
            }
            // Fall through to process_message so the service requests receipts
        }

        // Dispatch to EthWatchService for NewBlockHashes, NewBlock, and Receipts
        watch_svc->process_message(eth_id, payload);

        std::cout << "ETH message id=" << static_cast<int>(eth_id)
                  << " payload=" << msg.payload.size() << " bytes\n";
    });

    std::cout << "Connected. Watching for events...\n"
              << "\n⚠️  Bootstrap nodes are for DISCOVERY ONLY — they will not send block data.\n\n";

    auto executor = co_await boost::asio::this_coro::executor;
    boost::asio::steady_timer timer(executor);
    timer.expires_after(std::chrono::hours(24 * 365));
    boost::system::error_code ec;
    co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    (void)session;
    co_return;
}

} // namespace

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

        if (!config->bootnode_enodes.empty())
        {
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

            auto dv4 = std::make_shared<discv4::discv4_client>(io, dv4_cfg);

            // Capture config values needed in the callback
            const uint64_t   network_id   = config->network_id;
            const auto       genesis_hash = config->genesis_hash;
            const uint8_t    eth_offset   = config->eth_offset;
            const auto       watch_specs  = config->watch_specs;
            auto connected = std::make_shared<std::atomic<bool>>(false);

            dv4->set_peer_discovered_callback(
                [&io, dv4, network_id, genesis_hash, eth_offset, watch_specs, connected]
                (const discv4::DiscoveredPeer& peer)
                {
                    // Validate public key before attempting connection
                    rlpx::PublicKey pubkey{};
                    std::copy(peer.node_id.begin(), peer.node_id.end(), pubkey.begin());
                    if (!rlpx::crypto::Ecdh::verify_public_key(pubkey))
                    {
                        return;  // Skip peers with invalid pubkeys
                    }

                    // Only attempt one successful connection at a time
                    if (connected->exchange(true))
                    {
                        return;
                    }

                    std::cout << "Discovered peer: " << peer.ip
                              << ":" << peer.tcp_port << " — connecting...\n";

                    dv4->stop();

                    boost::asio::co_spawn(io,
                        [&io, pubkey, peer, eth_offset, network_id, genesis_hash,
                         watch_specs, connected]() -> boost::asio::awaitable<void>
                        {
                            co_await run_watch(peer.ip, peer.tcp_port, pubkey,
                                               eth_offset, network_id, genesis_hash,
                                               watch_specs);
                            // Allow retry if this connection failed immediately
                            connected->store(false);
                        },
                        boost::asio::detached);
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
                boost::asio::co_spawn(io,
                    [dv4, host = bn->host, port = bn->port, bn_id]()
                        -> boost::asio::awaitable<void>
                    {
                        auto result = co_await dv4->ping(host, port, bn_id);
                        (void)result;
                    },
                    boost::asio::detached);
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

            boost::asio::co_spawn(io,
                run_watch(config->host, config->port, peer_pubkey,
                          config->eth_offset,
                          config->network_id,
                          config->genesis_hash,
                          std::move(config->watch_specs)),
                boost::asio::detached);
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

