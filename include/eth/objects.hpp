// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <rlp/intx.hpp>
#include <rlp/rlp_decoder.hpp>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_ethereum.hpp>

namespace eth::codec {

using Hash256 = rlp::Hash256;
using Address = rlp::Address;
using Bloom = rlp::Bloom;
using ByteBuffer = std::vector<uint8_t>;

struct LogEntry {
    Address address{};
    std::vector<Hash256> topics;
    ByteBuffer data;
};

struct Receipt {
    std::optional<Hash256> state_root;
    std::optional<bool> status;
    intx::uint256 cumulative_gas_used{};
    Bloom bloom{};
    std::vector<LogEntry> logs;
};

struct BlockHeader {
    Hash256 parent_hash{};
    Hash256 ommers_hash{};
    Address beneficiary{};
    Hash256 state_root{};
    Hash256 transactions_root{};
    Hash256 receipts_root{};
    Bloom logs_bloom{};
    intx::uint256 difficulty{};
    uint64_t number = 0;
    uint64_t gas_limit = 0;
    uint64_t gas_used = 0;
    uint64_t timestamp = 0;
    ByteBuffer extra_data;
    Hash256 mix_hash{};
    std::array<uint8_t, 8> nonce{};
    std::optional<intx::uint256> base_fee_per_gas;
};

using EncodeResult = rlp::EncodingResult<ByteBuffer>;

template <typename T>
using DecodeResult = rlp::Result<T>;

[[nodiscard]] EncodeResult encode_log_entry(const LogEntry& entry) noexcept;
[[nodiscard]] DecodeResult<LogEntry> decode_log_entry(rlp::ByteView rlp_data) noexcept;

[[nodiscard]] EncodeResult encode_receipt(const Receipt& receipt) noexcept;
[[nodiscard]] DecodeResult<Receipt> decode_receipt(rlp::ByteView rlp_data) noexcept;

[[nodiscard]] EncodeResult encode_block_header(const BlockHeader& header) noexcept;
[[nodiscard]] DecodeResult<BlockHeader> decode_block_header(rlp::ByteView rlp_data) noexcept;

} // namespace eth::codec

