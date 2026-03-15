// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0
//
// discv5_crawl — live discv5 peer discovery example.
//
// Usage:
//   discv5_crawl [options]
//
// Options:
//   --chain <name>         Chain to discover on.  Supported names:
//                            ethereum (default), sepolia, holesky,
//                            polygon, amoy, bsc, bsc-testnet,
//                            base, base-sepolia
//   --bootnode-enr <uri>   Add an extra bootstrap ENR ("enr:…") or
//                          enode ("enode://…") URI.  May be repeated.
//   --port <udp-port>      Local UDP bind port.  Default: 9000.
//   --timeout <secs>       Stop after this many seconds.  Default: 60.
//   --log-level <level>    spdlog level (trace/debug/info/warn/error).
//                          Default: info.
//
// The binary starts a discv5_client, seeds it from the selected chain's
// bootnode registry plus any explicit --bootnode-enr flags, and runs until
// the timeout expires.  It reports the final CrawlerStats to stdout.
//
// This is an opt-in live test — it requires network access and is NOT
// wired into the CTest suite.

#include <boost/asio/spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/redirect_error.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <discv5/discv5_client.hpp>
#include <discv5/discv5_bootnodes.hpp>
#include <discv5/discv5_constants.hpp>
#include <base/logger.hpp>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

/// @brief Map of CLI chain name strings → ChainId enum values.
///        Defined once here (M011 — no if/else string chains).
static const std::unordered_map<std::string, discv5::ChainId> kChainNameMap =
{
    { "ethereum",    discv5::ChainId::kEthereumMainnet },
    { "mainnet",     discv5::ChainId::kEthereumMainnet },
    { "sepolia",     discv5::ChainId::kEthereumSepolia },
    { "holesky",     discv5::ChainId::kEthereumHolesky },
    { "polygon",     discv5::ChainId::kPolygonMainnet  },
    { "amoy",        discv5::ChainId::kPolygonAmoy     },
    { "bsc",         discv5::ChainId::kBscMainnet      },
    { "bsc-testnet", discv5::ChainId::kBscTestnet      },
    { "base",        discv5::ChainId::kBaseMainnet     },
    { "base-sepolia",discv5::ChainId::kBaseSepolia     },
};

// ---------------------------------------------------------------------------
// CLI parsing
// ---------------------------------------------------------------------------

struct CliArgs
{
    discv5::ChainId       chain            = discv5::ChainId::kEthereumMainnet;
    std::vector<std::string> extra_enrs{};
    uint16_t              bind_port        = discv5::kDefaultUdpPort;
    uint32_t              timeout_sec      = 60U;
    std::string           log_level        = "info";
};

void print_usage(const char* argv0)
{
    std::cerr
        << "Usage: " << argv0 << " [options]\n"
        << "\nOptions:\n"
        << "  --chain <name>         ethereum|sepolia|holesky|polygon|amoy|bsc|bsc-testnet|base|base-sepolia\n"
        << "  --bootnode-enr <uri>   Add extra ENR or enode URI (may repeat)\n"
        << "  --port <udp-port>      Local UDP bind port (default: " << discv5::kDefaultUdpPort << ")\n"
        << "  --timeout <secs>       Stop after N seconds (default: 60)\n"
        << "  --log-level <level>    trace|debug|info|warn|error (default: info)\n"
        << "  --help                 Show this message\n";
}

std::optional<CliArgs> parse_args(int argc, char** argv)
{
    CliArgs args;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view flag(argv[i]);

        if (flag == "--help" || flag == "-h")
        {
            print_usage(argv[0]);
            return std::nullopt;
        }

        auto require_next = [&](std::string_view name) -> const char*
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: " << name << " requires an argument\n";
                print_usage(argv[0]);
                return nullptr;
            }
            return argv[++i];
        };

        if (flag == "--chain")
        {
            const char* val = require_next("--chain");
            if (!val) { return std::nullopt; }
            const auto it = kChainNameMap.find(std::string(val));
            if (it == kChainNameMap.end())
            {
                std::cerr << "Error: unknown chain '" << val << "'\n";
                print_usage(argv[0]);
                return std::nullopt;
            }
            args.chain = it->second;
        }
        else if (flag == "--bootnode-enr")
        {
            const char* val = require_next("--bootnode-enr");
            if (!val) { return std::nullopt; }
            args.extra_enrs.emplace_back(val);
        }
        else if (flag == "--port")
        {
            const char* val = require_next("--port");
            if (!val) { return std::nullopt; }
            args.bind_port = static_cast<uint16_t>(std::stoul(val));
        }
        else if (flag == "--timeout")
        {
            const char* val = require_next("--timeout");
            if (!val) { return std::nullopt; }
            args.timeout_sec = static_cast<uint32_t>(std::stoul(val));
        }
        else if (flag == "--log-level")
        {
            const char* val = require_next("--log-level");
            if (!val) { return std::nullopt; }
            args.log_level = val;
        }
        else
        {
            std::cerr << "Error: unknown option '" << flag << "'\n";
            print_usage(argv[0]);
            return std::nullopt;
        }
    }

    return args;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    const auto args_opt = parse_args(argc, argv);
    if (!args_opt.has_value())
    {
        return EXIT_FAILURE;
    }
    const CliArgs& args = args_opt.value();

    // -----------------------------------------------------------------------
    // Configure logging.
    // -----------------------------------------------------------------------
    spdlog::set_level(spdlog::level::from_str(args.log_level));
    auto logger = rlp::base::createLogger("discv5_crawl");

    // -----------------------------------------------------------------------
    // Build bootnode seed list.
    // -----------------------------------------------------------------------
    discv5::discv5Config cfg;
    cfg.bind_port          = args.bind_port;
    cfg.query_interval_sec = 10U;   // Probe every 10 s for the live demo

    // Load seeds from the chain registry.
    auto chain_source = discv5::ChainBootnodeRegistry::for_chain(args.chain);
    if (chain_source)
    {
        const auto seeds = chain_source->fetch();
        cfg.bootstrap_enrs.insert(cfg.bootstrap_enrs.end(), seeds.begin(), seeds.end());
        logger->info("Chain: {}  seed count: {}",
                     discv5::ChainBootnodeRegistry::chain_name(args.chain),
                     seeds.size());
    }

    // Append any manually specified bootstrap URIs.
    for (const auto& uri : args.extra_enrs)
    {
        cfg.bootstrap_enrs.push_back(uri);
        logger->info("Extra bootnode: {}", uri.substr(0U, 60U));
    }

    if (cfg.bootstrap_enrs.empty())
    {
        logger->error("No bootstrap nodes available.  Use --bootnode-enr or --chain.");
        return EXIT_FAILURE;
    }

    // -----------------------------------------------------------------------
    // Build and start the client.
    // -----------------------------------------------------------------------
    boost::asio::io_context io;

    discv5::discv5_client client(io, cfg);

    // Track peers as they are discovered.
    std::atomic<size_t> total_discovered{0U};

    client.set_peer_discovered_callback(
        [&logger, &total_discovered](const discovery::ValidatedPeer& peer)
        {
            ++total_discovered;
            logger->info("Discovered peer {}  {}:{}  eth_fork={}",
                         total_discovered.load(),
                         peer.ip,
                         peer.tcp_port,
                         peer.eth_fork_id.has_value() ? "yes" : "no");
        });

    client.set_error_callback(
        [&logger](const std::string& msg)
        {
            logger->warn("Crawler error: {}", msg);
        });

    {
        const auto start_result = client.start();
        if (!start_result.has_value())
        {
            logger->error("Failed to start discv5 client: {}",
                          discv5::to_string(start_result.error()));
            return EXIT_FAILURE;
        }
    }

    logger->info("discv5_crawl started on UDP port {}.  Running for {} s …",
                 args.bind_port, args.timeout_sec);

    // -----------------------------------------------------------------------
    // Run the io_context with a timeout and Ctrl-C handler.
    // -----------------------------------------------------------------------
    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait(
        [&io](const boost::system::error_code& /*ec*/, int /*sig*/)
        {
            io.stop();
        });

    boost::asio::spawn(io,
        [&io, &client, timeout_sec = args.timeout_sec](boost::asio::yield_context yield)
        {
            boost::asio::steady_timer timer(io);
            timer.expires_after(std::chrono::seconds(timeout_sec));
            boost::system::error_code ec;
            timer.async_wait(boost::asio::redirect_error(yield, ec));
            client.stop();
            io.stop();
        });

    io.run();

    // -----------------------------------------------------------------------
    // Print final stats.
    // -----------------------------------------------------------------------
    const discv5::CrawlerStats stats = client.stats();

    std::cout << "\n=== discv5_crawl results ===\n"
              << "  discovered  : " << stats.discovered   << "\n"
              << "  queued      : " << stats.queued        << "\n"
              << "  measured    : " << stats.measured      << "\n"
              << "  failed      : " << stats.failed        << "\n"
              << "  duplicates  : " << stats.duplicates    << "\n"
              << "  wrong_chain : " << stats.wrong_chain   << "\n"
              << "  no_eth_entry: " << stats.no_eth_entry  << "\n"
              << "  invalid_enr : " << stats.invalid_enr   << "\n"
              << "===========================\n";

    return EXIT_SUCCESS;
}
