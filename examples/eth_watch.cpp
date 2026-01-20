// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <array>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <eth/messages.hpp>
#include <rlpx/crypto/ecdh.hpp>
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

std::optional<std::string> get_string(const boost::json::object& obj, std::string_view key) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->value().is_string()) {
        return std::nullopt;
    }
    return std::string(it->value().as_string());
}

std::optional<uint64_t> get_uint(const boost::json::object& obj, std::string_view key) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return std::nullopt;
    }
    if (it->value().is_uint64()) {
        return it->value().as_uint64();
    }
    if (it->value().is_int64()) {
        const auto value = it->value().as_int64();
        if (value < 0) {
            return std::nullopt;
        }
        return static_cast<uint64_t>(value);
    }
    return std::nullopt;
}

std::optional<Config> load_config(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }

    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    boost::system::error_code ec;
    auto value = boost::json::parse(content, ec);
    if (ec || !value.is_object()) {
        return std::nullopt;
    }

    const auto& obj = value.as_object();
    Config cfg;

    auto host = get_string(obj, "host");
    auto peer = get_string(obj, "peer_pubkey_hex");
    auto port = get_uint(obj, "port");
    auto offset = get_uint(obj, "eth_offset");

    if (!host || !peer || !port) {
        return std::nullopt;
    }

    if (*port > 0xFFFFU) {
        return std::nullopt;
    }

    cfg.host = *host;
    cfg.peer_pubkey_hex = *peer;
    cfg.port = static_cast<uint16_t>(*port);

    if (offset.has_value()) {
        if (*offset > 0xFFU) {
            return std::nullopt;
        }
        cfg.eth_offset = static_cast<uint8_t>(*offset);
    }

    return cfg;
}

void print_usage(const char* exe) {
    std::cout << "Usage:\n"
              << "  " << exe << " <host> <port> <peer_pubkey_hex> [eth_offset]\n"
              << "  " << exe << " --config <path_to_json>\n"
              << "  " << exe << " --chain <mainnet|sepolia>\n";
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
        std::cout << "Failed to connect: " << static_cast<int>(session_result.error()) << "\n";
        co_return;
    }

    auto session = std::move(session_result.value());

    session->set_hello_handler([](const rlpx::protocol::HelloMessage& msg) {
        std::cout << "HELLO from peer: " << msg.client_id << "\n";
    });

    session->set_disconnect_handler([](const rlpx::protocol::DisconnectMessage& msg) {
        std::cout << "Disconnected: reason=" << static_cast<int>(msg.reason) << "\n";
    });

    session->set_ping_handler([session_ptr = session.get()](const rlpx::protocol::PingMessage&) {
        const rlpx::protocol::PongMessage pong;
        auto encoded = pong.encode();
        if (!encoded) {
            return;
        }
        const auto post_result = session_ptr->post_message(rlpx::framing::Message{
            .id = rlpx::kPongMessageId,
            .payload = std::move(encoded.value())
        });
        if (!post_result) {
            return;
        }
    });

    session->set_generic_handler([eth_offset](const rlpx::protocol::Message& msg) {
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

    std::cout << "Connected. Waiting for messages...\n";
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
        if (std::string_view(argv[1]) == "--config") {
            if (argc < 3) {
                print_usage(argv[0]);
                return 1;
            }
            config = load_config(argv[2]);
            if (!config) {
                std::cout << "Failed to load config file.\n";
                return 1;
            }
        } else if (std::string_view(argv[1]) == "--chain") {
            if (argc < 3) {
                print_usage(argv[0]);
                return 1;
            }
            const std::string config_path = std::string("examples/config/") + argv[2] + ".json";
            config = load_config(config_path);
            if (!config) {
                std::cout << "Failed to load chain config: " << config_path << "\n";
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
