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

#include <eth/messages.hpp>
#include <discv4/bootnodes.hpp>
#include <discv4/bootnodes_test.hpp>
#include <rlpx/crypto/ecdh.hpp>
#include <rlpx/rlpx_error.hpp>
#include <rlpx/rlpx_session.hpp>

namespace {

struct Config {
    std::string host;
    uint16_t port = 0;
    std::string peer_pubkey_hex;
    uint8_t eth_offset = 0x10;
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

std::optional<Config> load_bootnode_for_chain(std::string_view chain) {
    // Ethereum Mainnet
    if (chain == "mainnet") {
        if (ETHEREUM_MAINNET_BOOTNODES.empty()) {
            return std::nullopt;
        }
        return parse_enode(ETHEREUM_MAINNET_BOOTNODES.front());
    }

    // Ethereum Sepolia Testnet
    if (chain == "sepolia") {
        if (ETHEREUM_SEPOLIA_BOOTNODES.empty()) {
            return std::nullopt;
        }
        return parse_enode(ETHEREUM_SEPOLIA_BOOTNODES.front());
    }

    // Ethereum Holesky Testnet
    if (chain == "holesky") {
        if (ETHEREUM_HOLESKY_BOOTNODES.empty()) {
            return std::nullopt;
        }
        return parse_enode(ETHEREUM_HOLESKY_BOOTNODES.front());
    }

    // Polygon PoS Mainnet
    if (chain == "polygon") {
        if (POLYGON_MAINNET_BOOTNODES.empty()) {
            return std::nullopt;
        }
        return parse_enode(POLYGON_MAINNET_BOOTNODES.front());
    }

    // Polygon Amoy Testnet
    if (chain == "polygon-amoy") {
        if (POLYGON_AMOY_BOOTNODES.empty()) {
            return std::nullopt;
        }
        return parse_enode(POLYGON_AMOY_BOOTNODES.front());
    }

    // BSC Mainnet (Note: uses port 30311)
    if (chain == "bsc" || chain == "bsc-mainnet") {
        if (BSC_MAINNET_BOOTNODES.empty()) {
            return std::nullopt;
        }
        return parse_enode(BSC_MAINNET_BOOTNODES.front());
    }

    // BSC Testnet (Note: uses port 30311)
    if (chain == "bsc-testnet") {
        if (BSC_TESTNET_STATICNODES.empty()) {
            return std::nullopt;
        }
        return parse_enode(BSC_TESTNET_STATICNODES.front());
    }

    // Base Mainnet
    if (chain == "base") {
        if (BASE_MAINNET_BOOTNODES.empty()) {
            std::cout << "Base mainnet bootnodes not yet configured.\n"
                      << "Base uses OP Stack discovery - see https://docs.base.org/base-chain/node-operators/run-a-base-node\n";
            return std::nullopt;
        }
        return parse_enode(BASE_MAINNET_BOOTNODES.front());
    }

    // Base Sepolia Testnet
    if (chain == "base-sepolia") {
        if (BASE_SEPOLIA_BOOTNODES.empty()) {
            std::cout << "Base Sepolia bootnodes not yet configured.\n"
                      << "Base uses OP Stack discovery - see https://docs.base.org/base-chain/node-operators/run-a-base-node\n";
            return std::nullopt;
        }
        return parse_enode(BASE_SEPOLIA_BOOTNODES.front());
    }

    return std::nullopt;
}

void print_usage(const char* exe) {
    std::cout << "Usage:\n"
              << "  " << exe << " <host> <port> <peer_pubkey_hex> [eth_offset]\n"
              << "  " << exe << " --chain <chain_name>\n"
              << "\nAvailable chains:\n"
              << "  Ethereum: mainnet, sepolia, holesky\n"
              << "  Polygon:  polygon, polygon-amoy\n"
              << "  BSC:      bsc (or bsc-mainnet), bsc-testnet\n"
              << "  Base:     base, base-sepolia\n";
}

boost::asio::awaitable<void> run_watch(std::string host,
                                       uint16_t port,
                                       rlpx::PublicKey peer_pubkey,
                                       uint8_t eth_offset) {
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

    session->set_hello_handler([session](const rlpx::protocol::HelloMessage& msg) {
        std::cout << "HELLO from peer: " << msg.client_id << "\n";

        // Send ETH Status message after HELLO handshake completes
        eth::StatusMessage status{
            .protocol_version = 68,  // Ethereum protocol version 68
            .network_id = 1,         // Ethereum mainnet
            .total_difficulty = intx::uint256(0),
            .best_hash = {},         // Empty block hash
            .genesis_hash = {},      // Empty genesis hash
            .fork_id = {}            // Default fork ID
        };

        auto encoded = eth::protocol::encode_status(status);
        if (encoded) {
            // ETH Status message ID is 0, so we send with message ID 0
            const auto post_result = session->post_message(rlpx::framing::Message{
                .id = 0x00,  // ETH Status is always message 0
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
        if (!encoded) {
            return;
        }
        const auto post_result = session->post_message(rlpx::framing::Message{
            .id = rlpx::kPongMessageId,
            .payload = std::move(encoded.value())
        });
        if (!post_result) {
            return;
        }
    });

    session->set_generic_handler([session, eth_offset](const rlpx::protocol::Message& msg) {
        (void)session;
        if (msg.id < eth_offset) {
            std::cout << "Unknown message id=" << static_cast<int>(msg.id) << "\n";
            return;
        }

        const auto eth_id = static_cast<uint8_t>(msg.id - eth_offset);
        if (eth_id == eth::protocol::kStatusMessageId) {
            auto decoded = eth::protocol::decode_status(rlp::ByteView(msg.payload.data(), msg.payload.size()));
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
            auto decoded = eth::protocol::decode_new_block_hashes(rlp::ByteView(msg.payload.data(), msg.payload.size()));
            if (decoded) {
                std::cout << "NewBlockHashes: " << decoded.value().entries.size() << " hashes\n";
            } else {
                std::cout << "Failed to decode NewBlockHashes\n";
            }
            return;
        }

        std::cout << "ETH message id=" << static_cast<int>(eth_id)
                  << " payload=" << msg.payload.size() << " bytes\n";
    });

    std::cout << "Connected. Waiting for messages...\n"
              << "\n⚠️  Note: Bootstrap nodes are for DISCOVERY ONLY (discv4 protocol)\n"
              << "    They will NOT send block data. To receive blocks:\n"
              << "    1. Use discv4 to discover real peer nodes, OR\n"
              << "    2. Connect directly to a full node (not a bootstrap node)\n"
              << "\n    See BOOTNODES_CONFIGURATION.md for more info.\n\n";

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

        std::optional<Config> config;
        if (std::string_view(argv[1]) == "--chain") {
            if (argc < 3) {
                print_usage(argv[0]);
                return 1;
            }
            const std::string chain_name = argv[2];
            config = load_bootnode_for_chain(chain_name);
            if (!config) {
                std::cout << "Failed to load bootnode for chain: " << chain_name << "\n";
                return 1;
            }
        } else if (argc >= 4) {
            const auto port_value = parse_uint16(argv[2]);
            if (!port_value) {
                std::cout << "Invalid port value.\n";
                return 1;
            }

            Config cfg;
            cfg.host = argv[1];
            cfg.port = *port_value;
            cfg.peer_pubkey_hex = argv[3];

            if (argc >= 5) {
                auto offset_value = parse_uint8(argv[4]);
                if (!offset_value) {
                    std::cout << "Invalid eth_offset value.\n";
                    return 1;
                }
                cfg.eth_offset = *offset_value;
            }
            config = cfg;
        } else {
            print_usage(argv[0]);
            return 1;
        }

        rlpx::PublicKey peer_pubkey{};
        if (!parse_hex_array(config->peer_pubkey_hex, peer_pubkey)) {
            std::cout << "Invalid peer public key hex (expected 128 hex chars).\n";
            return 1;
        }

        boost::asio::io_context io;
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            io.stop();
        });

        boost::asio::co_spawn(io,
                              run_watch(config->host, config->port, peer_pubkey, config->eth_offset),
                              boost::asio::detached);

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

