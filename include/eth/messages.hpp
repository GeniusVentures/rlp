// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "eth_types.hpp"
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <vector>

namespace eth::protocol {

inline constexpr uint8_t kStatusMessageId = 0x00;
inline constexpr uint8_t kNewBlockHashesMessageId = 0x01;
inline constexpr uint8_t kTransactionsMessageId = 0x02;
inline constexpr uint8_t kGetBlockHeadersMessageId = 0x03;
inline constexpr uint8_t kBlockHeadersMessageId = 0x04;
inline constexpr uint8_t kNewBlockMessageId = 0x07;
inline constexpr uint8_t kNewPooledTransactionHashesMessageId = 0x08;
inline constexpr uint8_t kGetPooledTransactionsMessageId = 0x09;
inline constexpr uint8_t kPooledTransactionsMessageId = 0x0a;
inline constexpr uint8_t kGetReceiptsMessageId = 0x0f;
inline constexpr uint8_t kReceiptsMessageId = 0x10;

using ByteBuffer = std::vector<uint8_t>;
using EncodeResult = rlp::EncodingResult<ByteBuffer>;

template <typename T>
using DecodeResult = rlp::Result<T>;

// STATUS
[[nodiscard]] EncodeResult encode_status(const StatusMessage& msg) noexcept;
[[nodiscard]] DecodeResult<StatusMessage> decode_status(rlp::ByteView rlp_data) noexcept;

// NEW_BLOCK_HASHES
[[nodiscard]] EncodeResult encode_new_block_hashes(const NewBlockHashesMessage& msg) noexcept;
[[nodiscard]] DecodeResult<NewBlockHashesMessage> decode_new_block_hashes(rlp::ByteView rlp_data) noexcept;

// NEW_POOLED_TRANSACTION_HASHES
[[nodiscard]] EncodeResult encode_new_pooled_tx_hashes(const NewPooledTransactionHashesMessage& msg) noexcept;
[[nodiscard]] DecodeResult<NewPooledTransactionHashesMessage> decode_new_pooled_tx_hashes(rlp::ByteView rlp_data) noexcept;

// GET_BLOCK_HEADERS
[[nodiscard]] EncodeResult encode_get_block_headers(const GetBlockHeadersMessage& msg) noexcept;
[[nodiscard]] DecodeResult<GetBlockHeadersMessage> decode_get_block_headers(rlp::ByteView rlp_data) noexcept;

} // namespace eth::protocol

