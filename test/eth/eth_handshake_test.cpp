// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

// eth_handshake_test.cpp
//
// Unit tests for the ETH sub-protocol Status handshake, mirroring
// go-ethereum's eth/protocols/eth/handshake_test.go (TestHandshake69 /
// testHandshake) as closely as the C++ library allows.
//
// go-ethereum reference (eth/protocols/eth/handshake.go):
//   const handshakeTimeout = 5 * time.Second
//
// go-ethereum reference error types checked in testHandshake():
//   errNoStatusMsg              – first ETH message was not StatusMsg
//   errProtocolVersionMismatch  – ProtocolVersion != negotiated version
//   errNetworkIDMismatch        – NetworkID mismatch
//   errGenesisMismatch          – Genesis hash mismatch
//   errForkIDRejected           – ForkID filter rejected (not yet impl)
//   errInvalidBlockRange        – EarliestBlock > LatestBlock
//
// Our validate_status() returns StatusValidationError; the timeout constant
// is kStatusHandshakeTimeout.

#include <gtest/gtest.h>
#include <eth/messages.hpp>
#include <chrono>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

constexpr uint64_t kSepoliaNetworkID = 11155111;
constexpr uint8_t  kProtoVersion     = 68;

// Sepolia genesis hash (go-ethereum params/config.go SepoliaGenesisHash)
static const eth::Hash256 kSepoliaGenesis = []() {
    eth::Hash256 h{};
    // 0x25a5cc106eea7138acab33231d7160d69cb777ee0c2c553fcddf5138993e6dd9
    const uint8_t raw[32] = {
        0x25, 0xa5, 0xcc, 0x10, 0x6e, 0xea, 0x71, 0x38,
        0xac, 0xab, 0x33, 0x23, 0x1d, 0x71, 0x60, 0xd6,
        0x9c, 0xb7, 0x77, 0xee, 0x0c, 0x2c, 0x55, 0x3f,
        0xcd, 0xdf, 0x51, 0x38, 0x99, 0x3e, 0x6d, 0xd9,
    };
    std::copy(raw, raw + 32, h.begin());
    return h;
}();

/// Build a StatusMessage that passes all validation checks.
eth::StatusMessage make_valid_status() {
    eth::StatusMessage msg;
    msg.protocol_version  = kProtoVersion;
    msg.network_id        = kSepoliaNetworkID;
    msg.genesis_hash      = kSepoliaGenesis;
    msg.fork_id           = {};
    msg.earliest_block    = 0;
    msg.latest_block      = 8'000'000;
    msg.latest_block_hash = kSepoliaGenesis; // any non-zero hash is fine for validation
    return msg;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// EthHandshakeConstantsTest
// ---------------------------------------------------------------------------

/// go-ethereum: const handshakeTimeout = 5 * time.Second
/// Our library:  eth::protocol::kStatusHandshakeTimeout
TEST(EthHandshakeConstantsTest, HandshakeTimeout_Is5Seconds)
{
    EXPECT_EQ(eth::protocol::kStatusHandshakeTimeout, std::chrono::seconds(5))
        << "kStatusHandshakeTimeout must match go-ethereum's handshakeTimeout (5s)";
}

// ---------------------------------------------------------------------------
// EthHandshakeValidationTest — mirrors go-ethereum testHandshake() table
// ---------------------------------------------------------------------------

/// go-ethereum: { code: StatusMsg, valid Status } → no error (happy path)
TEST(EthHandshakeValidationTest, ValidStatus_Passes)
{
    const auto msg    = make_valid_status();
    const auto result = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    EXPECT_TRUE(result.has_value())
        << "A fully valid ETH Status must pass validation without error";
}

/// go-ethereum: { code: StatusMsg, data: StatusPacket{10, ...} }
///              → errProtocolVersionMismatch
TEST(EthHandshakeValidationTest, WrongProtocolVersion_Fails)
{
    auto msg             = make_valid_status();
    msg.protocol_version = 10; // ancient/unsupported version
    const auto result    = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), eth::StatusValidationError::kProtocolVersionMismatch);
}

/// go-ethereum: { code: StatusMsg, data: StatusPacket{proto, 999, ...} }
///              → errNetworkIDMismatch
TEST(EthHandshakeValidationTest, WrongNetworkID_Fails)
{
    auto msg          = make_valid_status();
    msg.network_id    = 999; // unknown chain
    const auto result = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), eth::StatusValidationError::kNetworkIDMismatch);
}

/// Polygon mainnet peer connecting on Sepolia P2P (real-world bor node case).
/// network_id=137 should be rejected as kNetworkIDMismatch.
TEST(EthHandshakeValidationTest, PolygonPeerOnSepolia_NetworkIDMismatch)
{
    auto msg          = make_valid_status();
    msg.network_id    = 137; // Polygon mainnet
    const auto result = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), eth::StatusValidationError::kNetworkIDMismatch);
}

/// go-ethereum: { code: StatusMsg, data: StatusPacket{proto, 1, common.Hash{3}, ...} }
///              → errGenesisMismatch
TEST(EthHandshakeValidationTest, WrongGenesis_Fails)
{
    auto msg             = make_valid_status();
    msg.genesis_hash[0] ^= 0xFF; // flip first byte → wrong genesis
    const auto result    = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), eth::StatusValidationError::kGenesisMismatch);
}

/// go-ethereum: { code: StatusMsg, data: StatusPacket{..., earliest+1, latest, ...} }
///              → errInvalidBlockRange (via BlockRangeUpdatePacket.Validate())
TEST(EthHandshakeValidationTest, InvalidBlockRange_EarliestAfterLatest_Fails)
{
    auto msg              = make_valid_status();
    msg.earliest_block    = 500;
    msg.latest_block      = 499; // earliest > latest
    const auto result     = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), eth::StatusValidationError::kInvalidBlockRange);
}

/// go-ethereum: both earliest=0 and latest=0 are valid (fresh node, no blocks yet).
TEST(EthHandshakeValidationTest, ZeroBlockRange_Passes)
{
    auto msg           = make_valid_status();
    msg.earliest_block = 0;
    msg.latest_block   = 0;
    const auto result  = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    EXPECT_TRUE(result.has_value())
        << "Zero block range (fresh node) must be accepted";
}

/// Validation checks network_id before genesis — NetworkID mismatch takes priority.
TEST(EthHandshakeValidationTest, NetworkIDCheckedBeforeGenesis)
{
    auto msg             = make_valid_status();
    msg.network_id       = 1;   // mainnet instead of Sepolia
    msg.genesis_hash[0] ^= 0xFF; // also wrong genesis
    const auto result    = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    ASSERT_FALSE(result.has_value());
    // NetworkID mismatch must be reported (not genesis mismatch) because
    // validate_status checks network_id first, matching go-ethereum's readStatus().
    EXPECT_EQ(result.error(), eth::StatusValidationError::kNetworkIDMismatch);
}

// ---------------------------------------------------------------------------
// EthHandshakeEncodeDecodeTest — round-trip encode→decode→validate
// Tests the full wire-format path, not just validate_status in isolation.
// ---------------------------------------------------------------------------

/// A Status that encodes and decodes cleanly must then pass validation.
TEST(EthHandshakeEncodeDecodeTest, RoundTrip_ValidStatus)
{
    const auto original = make_valid_status();

    const auto encoded = eth::protocol::encode_status(original);
    ASSERT_TRUE(encoded.has_value()) << "encode_status must succeed for a valid message";

    const rlp::ByteView wire{encoded.value().data(), encoded.value().size()};
    const auto decoded = eth::protocol::decode_status(wire);
    ASSERT_TRUE(decoded.has_value()) << "decode_status must succeed on its own encoded output";

    const auto& msg   = decoded.value();
    const auto result = eth::protocol::validate_status(
        msg, kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    EXPECT_TRUE(result.has_value())
        << "Round-tripped Status must pass validation";
}

/// A Status with mismatched network_id must still fail after round-trip.
TEST(EthHandshakeEncodeDecodeTest, RoundTrip_WrongNetworkID_StillFails)
{
    auto original      = make_valid_status();
    original.network_id = 1; // mainnet

    const auto encoded = eth::protocol::encode_status(original);
    ASSERT_TRUE(encoded.has_value());

    const rlp::ByteView wire{encoded.value().data(), encoded.value().size()};
    const auto decoded = eth::protocol::decode_status(wire);
    ASSERT_TRUE(decoded.has_value());

    const auto result = eth::protocol::validate_status(
        decoded.value(), kProtoVersion, kSepoliaNetworkID, kSepoliaGenesis);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), eth::StatusValidationError::kNetworkIDMismatch);
}
