// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <eth/messages.hpp>
#include <algorithm>

namespace {

template <typename Array>
Array make_filled(uint8_t seed) {
    Array value{};
    for (size_t i = 0; i < value.size(); ++i) {
        value[i] = static_cast<uint8_t>(seed + i);
    }
    return value;
}

} // namespace

TEST(EthMessagesTest, StatusRoundtrip) {
    eth::StatusMessage original;
    original.protocol_version = 66;
    original.network_id = 11155111;
    original.total_difficulty = intx::uint256(123456);
    original.best_hash = make_filled<eth::Hash256>(0x10);
    original.genesis_hash = make_filled<eth::Hash256>(0x20);
    original.fork_id.fork_hash = make_filled<std::array<uint8_t, 4>>(0x01);
    original.fork_id.next_fork = 987654;

    auto encoded = eth::protocol::encode_status(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_status(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    EXPECT_EQ(result.protocol_version, original.protocol_version);
    EXPECT_EQ(result.network_id, original.network_id);
    EXPECT_EQ(result.total_difficulty, original.total_difficulty);
    EXPECT_EQ(result.best_hash, original.best_hash);
    EXPECT_EQ(result.genesis_hash, original.genesis_hash);
    EXPECT_EQ(result.fork_id.fork_hash, original.fork_id.fork_hash);
    EXPECT_EQ(result.fork_id.next_fork, original.fork_id.next_fork);
}

TEST(EthMessagesTest, NewBlockHashesRoundtrip) {
    eth::NewBlockHashesMessage original;
    original.entries.push_back({make_filled<eth::Hash256>(0x01), 123});
    original.entries.push_back({make_filled<eth::Hash256>(0x02), 456});

    auto encoded = eth::protocol::encode_new_block_hashes(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_new_block_hashes(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_EQ(result.entries.size(), original.entries.size());
    EXPECT_EQ(result.entries[0].hash, original.entries[0].hash);
    EXPECT_EQ(result.entries[0].number, original.entries[0].number);
    EXPECT_EQ(result.entries[1].hash, original.entries[1].hash);
    EXPECT_EQ(result.entries[1].number, original.entries[1].number);
}

TEST(EthMessagesTest, NewPooledTransactionHashesRoundtrip) {
    eth::NewPooledTransactionHashesMessage original;
    original.hashes.push_back(make_filled<eth::Hash256>(0xA0));
    original.hashes.push_back(make_filled<eth::Hash256>(0xB0));

    auto encoded = eth::protocol::encode_new_pooled_tx_hashes(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_new_pooled_tx_hashes(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_EQ(result.hashes.size(), original.hashes.size());
    EXPECT_EQ(result.hashes[0], original.hashes[0]);
    EXPECT_EQ(result.hashes[1], original.hashes[1]);
}

TEST(EthMessagesTest, GetBlockHeadersRoundtripByHash) {
    eth::GetBlockHeadersMessage original;
    original.start_hash = make_filled<eth::Hash256>(0x11);
    original.max_headers = 128;
    original.skip = 2;
    original.reverse = true;

    auto encoded = eth::protocol::encode_get_block_headers(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_get_block_headers(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_TRUE(result.start_hash.has_value());
    EXPECT_EQ(result.start_hash.value(), original.start_hash.value());
    EXPECT_FALSE(result.start_number.has_value());
    EXPECT_EQ(result.max_headers, original.max_headers);
    EXPECT_EQ(result.skip, original.skip);
    EXPECT_EQ(result.reverse, original.reverse);
}

TEST(EthMessagesTest, GetBlockHeadersRoundtripByNumber) {
    eth::GetBlockHeadersMessage original;
    original.start_number = 900;
    original.max_headers = 64;
    original.skip = 0;
    original.reverse = false;

    auto encoded = eth::protocol::encode_get_block_headers(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_get_block_headers(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_TRUE(result.start_number.has_value());
    EXPECT_EQ(result.start_number.value(), original.start_number.value());
    EXPECT_FALSE(result.start_hash.has_value());
    EXPECT_EQ(result.max_headers, original.max_headers);
    EXPECT_EQ(result.skip, original.skip);
    EXPECT_EQ(result.reverse, original.reverse);
}

