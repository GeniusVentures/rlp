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

TEST(EthMessagesTest, GetBlockHeadersRoundtripEth66Envelope) {
    eth::GetBlockHeadersMessage original;
    original.request_id = 42;
    original.start_hash = make_filled<eth::Hash256>(0x55);
    original.max_headers = 16;
    original.skip = 1;
    original.reverse = false;

    auto encoded = eth::protocol::encode_get_block_headers(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_get_block_headers(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_TRUE(result.request_id.has_value());
    EXPECT_EQ(result.request_id.value(), original.request_id.value());
    ASSERT_TRUE(result.start_hash.has_value());
    EXPECT_EQ(result.start_hash.value(), original.start_hash.value());
    EXPECT_EQ(result.max_headers, original.max_headers);
    EXPECT_EQ(result.skip, original.skip);
    EXPECT_EQ(result.reverse, original.reverse);
}

TEST(EthMessagesTest, BlockHeadersRoundtripEth66Envelope) {
    eth::BlockHeadersMessage original;
    original.request_id = 88;

    eth::codec::BlockHeader header;
    header.parent_hash = make_filled<eth::Hash256>(0x01);
    header.ommers_hash = make_filled<eth::Hash256>(0x02);
    header.beneficiary = make_filled<eth::Address>(0x03);
    header.state_root = make_filled<eth::Hash256>(0x04);
    header.transactions_root = make_filled<eth::Hash256>(0x05);
    header.receipts_root = make_filled<eth::Hash256>(0x06);
    header.logs_bloom = make_filled<eth::Bloom>(0x07);
    header.difficulty = intx::uint256(123);
    header.number = 900;
    header.gas_limit = 30000000;
    header.gas_used = 21000;
    header.timestamp = 1700000000;
    header.extra_data = {0xab, 0xcd};
    header.mix_hash = make_filled<eth::Hash256>(0x08);
    header.nonce = make_filled<std::array<uint8_t, 8>>(0x09);
    header.base_fee_per_gas = intx::uint256(100);
    original.headers.push_back(header);

    auto encoded = eth::protocol::encode_block_headers(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_block_headers(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_TRUE(result.request_id.has_value());
    EXPECT_EQ(result.request_id.value(), original.request_id.value());
    ASSERT_EQ(result.headers.size(), 1u);
    EXPECT_EQ(result.headers[0].parent_hash, header.parent_hash);
    EXPECT_EQ(result.headers[0].number, header.number);
}

TEST(EthMessagesTest, GetReceiptsRoundtripEth66Envelope) {
    eth::GetReceiptsMessage original;
    original.request_id = 1001;
    original.block_hashes.push_back(make_filled<eth::Hash256>(0x21));
    original.block_hashes.push_back(make_filled<eth::Hash256>(0x31));

    auto encoded = eth::protocol::encode_get_receipts(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_get_receipts(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_TRUE(result.request_id.has_value());
    EXPECT_EQ(result.request_id.value(), original.request_id.value());
    EXPECT_EQ(result.block_hashes, original.block_hashes);
}

TEST(EthMessagesTest, ReceiptsRoundtripEth66Envelope) {
    eth::ReceiptsMessage original;
    original.request_id = 1002;

    eth::codec::Receipt receipt;
    receipt.status = true;
    receipt.cumulative_gas_used = intx::uint256(21000);
    receipt.bloom = make_filled<eth::Bloom>(0x40);

    eth::codec::LogEntry log;
    log.address = make_filled<eth::Address>(0x50);
    log.topics.push_back(make_filled<eth::Hash256>(0x60));
    log.data = {0xaa, 0xbb};
    receipt.logs.push_back(log);

    original.receipts.push_back({receipt});

    auto encoded = eth::protocol::encode_receipts(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_receipts(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_TRUE(result.request_id.has_value());
    EXPECT_EQ(result.request_id.value(), original.request_id.value());
    ASSERT_EQ(result.receipts.size(), 1u);
    ASSERT_EQ(result.receipts[0].size(), 1u);
    ASSERT_TRUE(result.receipts[0][0].status.has_value());
    EXPECT_TRUE(result.receipts[0][0].status.value());
    EXPECT_EQ(result.receipts[0][0].logs.size(), 1u);
    EXPECT_EQ(result.receipts[0][0].logs[0].address, log.address);
}

TEST(EthMessagesTest, GetPooledTransactionsRoundtripEth66Envelope) {
    eth::GetPooledTransactionsMessage original;
    original.request_id = 2001;
    original.transaction_hashes.push_back(make_filled<eth::Hash256>(0x71));
    original.transaction_hashes.push_back(make_filled<eth::Hash256>(0x81));

    auto encoded = eth::protocol::encode_get_pooled_transactions(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_get_pooled_transactions(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_TRUE(result.request_id.has_value());
    EXPECT_EQ(result.request_id.value(), original.request_id.value());
    EXPECT_EQ(result.transaction_hashes, original.transaction_hashes);
}

TEST(EthMessagesTest, PooledTransactionsRoundtripEth66Envelope) {
    rlp::RlpEncoder tx1_encoder;
    ASSERT_TRUE(tx1_encoder.BeginList().has_value());
    ASSERT_TRUE(tx1_encoder.add(static_cast<uint8_t>(0x01)).has_value());
    ASSERT_TRUE(tx1_encoder.add(static_cast<uint8_t>(0x02)).has_value());
    ASSERT_TRUE(tx1_encoder.EndList().has_value());
    auto tx1_bytes = tx1_encoder.GetBytes();
    ASSERT_TRUE(tx1_bytes.has_value());

    rlp::RlpEncoder tx2_encoder;
    ASSERT_TRUE(tx2_encoder.BeginList().has_value());
    ASSERT_TRUE(tx2_encoder.add(static_cast<uint8_t>(0x03)).has_value());
    ASSERT_TRUE(tx2_encoder.EndList().has_value());
    auto tx2_bytes = tx2_encoder.GetBytes();
    ASSERT_TRUE(tx2_bytes.has_value());

    eth::PooledTransactionsMessage original;
    original.request_id = 2002;
    original.encoded_transactions.emplace_back(tx1_bytes.value()->begin(), tx1_bytes.value()->end());
    original.encoded_transactions.emplace_back(tx2_bytes.value()->begin(), tx2_bytes.value()->end());

    auto encoded = eth::protocol::encode_pooled_transactions(original);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = eth::protocol::decode_pooled_transactions(rlp::ByteView(encoded.value().data(), encoded.value().size()));
    ASSERT_TRUE(decoded.has_value());

    const auto& result = decoded.value();
    ASSERT_TRUE(result.request_id.has_value());
    EXPECT_EQ(result.request_id.value(), original.request_id.value());
    EXPECT_EQ(result.encoded_transactions, original.encoded_transactions);
}
