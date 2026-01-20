// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <eth/messages.hpp>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>

namespace eth::protocol {

namespace {

ByteBuffer to_byte_buffer(const rlp::Bytes& bytes) {
    return ByteBuffer(bytes.begin(), bytes.end());
}

} // namespace

EncodeResult encode_status(const StatusMessage& msg) noexcept {
    rlp::RlpEncoder encoder;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
    if (!encoder.add(msg.protocol_version)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(msg.network_id)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(msg.total_difficulty)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(msg.best_hash.data(), msg.best_hash.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(msg.genesis_hash.data(), msg.genesis_hash.size()))) return rlp::EncodingError::kPayloadTooLarge;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
    if (!encoder.add(rlp::ByteView(msg.fork_id.fork_hash.data(), msg.fork_id.fork_hash.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(msg.fork_id.next_fork)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    auto result = encoder.GetBytes();
    if (!result) return result.error();
    return to_byte_buffer(*result.value());
}

DecodeResult<StatusMessage> decode_status(rlp::ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(rlp_data);

    auto list_size = decoder.ReadListHeaderBytes();
    if (!list_size) return list_size.error();

    StatusMessage msg;

    if (!decoder.read(msg.protocol_version)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(msg.network_id)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(msg.total_difficulty)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(msg.best_hash)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(msg.genesis_hash)) return rlp::DecodingError::kUnexpectedLength;

    auto fork_list = decoder.ReadListHeaderBytes();
    if (!fork_list) return fork_list.error();
    if (!decoder.read(msg.fork_id.fork_hash)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(msg.fork_id.next_fork)) return rlp::DecodingError::kUnexpectedString;

    return msg;
}

EncodeResult encode_new_block_hashes(const NewBlockHashesMessage& msg) noexcept {
    rlp::RlpEncoder encoder;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
    for (const auto& entry : msg.entries) {
        if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
        if (!encoder.add(rlp::ByteView(entry.hash.data(), entry.hash.size()))) return rlp::EncodingError::kPayloadTooLarge;
        if (!encoder.add(entry.number)) return rlp::EncodingError::kPayloadTooLarge;
        if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;
    }
    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    auto result = encoder.GetBytes();
    if (!result) return result.error();
    return to_byte_buffer(*result.value());
}

DecodeResult<NewBlockHashesMessage> decode_new_block_hashes(rlp::ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(rlp_data);

    auto list_size = decoder.ReadListHeaderBytes();
    if (!list_size) return list_size.error();

    NewBlockHashesMessage msg;

    while (!decoder.IsFinished()) {
        auto entry_list = decoder.ReadListHeaderBytes();
        if (!entry_list) return entry_list.error();

        NewBlockHashEntry entry;
        if (!decoder.read(entry.hash)) return rlp::DecodingError::kUnexpectedLength;
        if (!decoder.read(entry.number)) return rlp::DecodingError::kUnexpectedString;

        msg.entries.push_back(entry);
    }

    return msg;
}

EncodeResult encode_new_pooled_tx_hashes(const NewPooledTransactionHashesMessage& msg) noexcept {
    rlp::RlpEncoder encoder;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
    for (const auto& hash : msg.hashes) {
        if (!encoder.add(rlp::ByteView(hash.data(), hash.size()))) return rlp::EncodingError::kPayloadTooLarge;
    }
    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    auto result = encoder.GetBytes();
    if (!result) return result.error();
    return to_byte_buffer(*result.value());
}

DecodeResult<NewPooledTransactionHashesMessage> decode_new_pooled_tx_hashes(rlp::ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(rlp_data);

    auto list_size = decoder.ReadListHeaderBytes();
    if (!list_size) return list_size.error();

    NewPooledTransactionHashesMessage msg;

    while (!decoder.IsFinished()) {
        Hash256 hash{};
        if (!decoder.read(hash)) return rlp::DecodingError::kUnexpectedLength;
        msg.hashes.push_back(hash);
    }

    return msg;
}

EncodeResult encode_get_block_headers(const GetBlockHeadersMessage& msg) noexcept {
    rlp::RlpEncoder encoder;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;

    if (msg.start_hash.has_value()) {
        if (!encoder.add(rlp::ByteView(msg.start_hash->data(), msg.start_hash->size()))) return rlp::EncodingError::kPayloadTooLarge;
    } else if (msg.start_number.has_value()) {
        if (!encoder.add(*msg.start_number)) return rlp::EncodingError::kPayloadTooLarge;
    } else {
        return rlp::EncodingError::kEmptyInput;
    }

    if (!encoder.add(msg.max_headers)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(msg.skip)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(msg.reverse)) return rlp::EncodingError::kPayloadTooLarge;

    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    auto result = encoder.GetBytes();
    if (!result) return result.error();
    return to_byte_buffer(*result.value());
}

DecodeResult<GetBlockHeadersMessage> decode_get_block_headers(rlp::ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(rlp_data);

    auto list_size = decoder.ReadListHeaderBytes();
    if (!list_size) return list_size.error();

    GetBlockHeadersMessage msg;

    auto header = decoder.PeekHeader();
    if (!header) return header.error();
    if (header.value().list) return rlp::DecodingError::kUnexpectedList;

    if (header.value().payload_size_bytes == Hash256{}.size()) {
        Hash256 hash{};
        if (!decoder.read(hash)) return rlp::DecodingError::kUnexpectedLength;
        msg.start_hash = hash;
    } else {
        uint64_t number = 0;
        if (!decoder.read(number)) return rlp::DecodingError::kUnexpectedString;
        msg.start_number = number;
    }

    if (!decoder.read(msg.max_headers)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(msg.skip)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(msg.reverse)) return rlp::DecodingError::kUnexpectedString;

    return msg;
}

} // namespace eth::protocol

