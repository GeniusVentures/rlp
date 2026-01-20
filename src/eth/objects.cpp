// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <eth/objects.hpp>
#include <rlp/rlp_decoder.hpp>
#include <rlp/rlp_encoder.hpp>

namespace eth::codec {

namespace {

EncodeResult finalize_encoding(rlp::RlpEncoder& encoder) {
    auto result = encoder.GetBytes();
    if (!result) {
        return result.error();
    }
    return ByteBuffer(result.value()->begin(), result.value()->end());
}

rlp::Result<void> decode_status_or_state_root(rlp::RlpDecoder& decoder, Receipt& receipt) {
    auto header = decoder.PeekHeader();
    if (!header) {
        return header.error();
    }

    if (header.value().list) {
        return rlp::DecodingError::kUnexpectedList;
    }

    if (header.value().payload_size_bytes == Hash256{}.size()) {
        Hash256 root{};
        auto read_result = decoder.read(root);
        if (!read_result) {
            return read_result.error();
        }
        receipt.state_root = root;
        receipt.status.reset();
        return rlp::outcome::success();
    }

    rlp::Bytes status_bytes;
    auto status_result = decoder.read(status_bytes);
    if (!status_result) {
        return status_result.error();
    }

    if (status_bytes.empty()) {
        receipt.status = false;
        receipt.state_root.reset();
        return rlp::outcome::success();
    }

    if (status_bytes.size() == 1) {
        receipt.status = (status_bytes[0] != 0);
        receipt.state_root.reset();
        return rlp::outcome::success();
    }

    return rlp::DecodingError::kUnexpectedLength;
}

rlp::Result<LogEntry> decode_log_entry_from_decoder(rlp::RlpDecoder& decoder) {
    auto list_size = decoder.ReadListHeaderBytes();
    if (!list_size) {
        return list_size.error();
    }

    const size_t payload_size = list_size.value();
    const size_t start_remaining = decoder.Remaining().size();
    const size_t target_remaining = start_remaining - payload_size;

    LogEntry entry;
    if (!decoder.read(entry.address)) {
        return rlp::DecodingError::kUnexpectedLength;
    }

    auto topics_list_size = decoder.ReadListHeaderBytes();
    if (!topics_list_size) {
        return topics_list_size.error();
    }

    const size_t topics_payload = topics_list_size.value();
    const size_t topics_start = decoder.Remaining().size();
    const size_t topics_target = topics_start - topics_payload;

    while (decoder.Remaining().size() > topics_target) {
        Hash256 topic{};
        if (!decoder.read(topic)) {
            return rlp::DecodingError::kUnexpectedLength;
        }
        entry.topics.push_back(topic);
    }

    if (decoder.Remaining().size() != topics_target) {
        return rlp::DecodingError::kListLengthMismatch;
    }

    rlp::Bytes data_bytes;
    if (!decoder.read(data_bytes)) {
        return rlp::DecodingError::kUnexpectedString;
    }
    entry.data.assign(data_bytes.begin(), data_bytes.end());

    if (decoder.Remaining().size() != target_remaining) {
        return rlp::DecodingError::kListLengthMismatch;
    }

    return entry;
}

} // namespace

EncodeResult encode_log_entry(const LogEntry& entry) noexcept {
    rlp::RlpEncoder encoder;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
    if (!encoder.add(rlp::ByteView(entry.address.data(), entry.address.size()))) return rlp::EncodingError::kPayloadTooLarge;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
    for (const auto& topic : entry.topics) {
        if (!encoder.add(rlp::ByteView(topic.data(), topic.size()))) return rlp::EncodingError::kPayloadTooLarge;
    }
    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    if (!encoder.add(rlp::ByteView(entry.data.data(), entry.data.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    return finalize_encoding(encoder);
}

DecodeResult<LogEntry> decode_log_entry(rlp::ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(rlp_data);
    return decode_log_entry_from_decoder(decoder);
}

EncodeResult encode_receipt(const Receipt& receipt) noexcept {
    rlp::RlpEncoder encoder;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;

    if (receipt.state_root.has_value()) {
        if (!encoder.add(rlp::ByteView(receipt.state_root->data(), receipt.state_root->size()))) return rlp::EncodingError::kPayloadTooLarge;
    } else if (receipt.status.has_value()) {
        if (!encoder.add(static_cast<uint8_t>(receipt.status.value() ? 1 : 0))) return rlp::EncodingError::kPayloadTooLarge;
    } else {
        return rlp::EncodingError::kEmptyInput;
    }

    if (!encoder.add(receipt.cumulative_gas_used)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(receipt.bloom.data(), receipt.bloom.size()))) return rlp::EncodingError::kPayloadTooLarge;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
    for (const auto& log : receipt.logs) {
        if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
        if (!encoder.add(rlp::ByteView(log.address.data(), log.address.size()))) return rlp::EncodingError::kPayloadTooLarge;

        if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
        for (const auto& topic : log.topics) {
            if (!encoder.add(rlp::ByteView(topic.data(), topic.size()))) return rlp::EncodingError::kPayloadTooLarge;
        }
        if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

        if (!encoder.add(rlp::ByteView(log.data.data(), log.data.size()))) return rlp::EncodingError::kPayloadTooLarge;
        if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;
    }
    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    return finalize_encoding(encoder);
}

DecodeResult<Receipt> decode_receipt(rlp::ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(rlp_data);

    auto list_size = decoder.ReadListHeaderBytes();
    if (!list_size) {
        return list_size.error();
    }

    Receipt receipt;
    auto status_result = decode_status_or_state_root(decoder, receipt);
    if (!status_result) {
        return status_result.error();
    }

    if (!decoder.read(receipt.cumulative_gas_used)) {
        return rlp::DecodingError::kUnexpectedString;
    }

    if (!decoder.read(receipt.bloom)) {
        return rlp::DecodingError::kUnexpectedLength;
    }

    auto logs_list_size = decoder.ReadListHeaderBytes();
    if (!logs_list_size) {
        return logs_list_size.error();
    }

    const size_t logs_payload = logs_list_size.value();
    const size_t start_remaining = decoder.Remaining().size();
    const size_t target_remaining = start_remaining - logs_payload;

    while (decoder.Remaining().size() > target_remaining) {
        auto log_result = decode_log_entry_from_decoder(decoder);
        if (!log_result) {
            return log_result.error();
        }
        receipt.logs.push_back(std::move(log_result.value()));
    }

    if (decoder.Remaining().size() != target_remaining) {
        return rlp::DecodingError::kListLengthMismatch;
    }

    return receipt;
}

EncodeResult encode_block_header(const BlockHeader& header) noexcept {
    rlp::RlpEncoder encoder;

    if (!encoder.BeginList()) return rlp::EncodingError::kUnclosedList;
    if (!encoder.add(rlp::ByteView(header.parent_hash.data(), header.parent_hash.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.ommers_hash.data(), header.ommers_hash.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.beneficiary.data(), header.beneficiary.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.state_root.data(), header.state_root.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.transactions_root.data(), header.transactions_root.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.receipts_root.data(), header.receipts_root.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.logs_bloom.data(), header.logs_bloom.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(header.difficulty)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(header.number)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(header.gas_limit)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(header.gas_used)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(header.timestamp)) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.extra_data.data(), header.extra_data.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.mix_hash.data(), header.mix_hash.size()))) return rlp::EncodingError::kPayloadTooLarge;
    if (!encoder.add(rlp::ByteView(header.nonce.data(), header.nonce.size()))) return rlp::EncodingError::kPayloadTooLarge;

    if (header.base_fee_per_gas.has_value()) {
        if (!encoder.add(header.base_fee_per_gas.value())) return rlp::EncodingError::kPayloadTooLarge;
    }

    if (!encoder.EndList()) return rlp::EncodingError::kUnclosedList;

    return finalize_encoding(encoder);
}

DecodeResult<BlockHeader> decode_block_header(rlp::ByteView rlp_data) noexcept {
    rlp::RlpDecoder decoder(rlp_data);

    auto list_size = decoder.ReadListHeaderBytes();
    if (!list_size) {
        return list_size.error();
    }

    BlockHeader header;

    if (!decoder.read(header.parent_hash)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(header.ommers_hash)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(header.beneficiary)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(header.state_root)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(header.transactions_root)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(header.receipts_root)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(header.logs_bloom)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(header.difficulty)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(header.number)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(header.gas_limit)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(header.gas_used)) return rlp::DecodingError::kUnexpectedString;
    if (!decoder.read(header.timestamp)) return rlp::DecodingError::kUnexpectedString;

    rlp::Bytes extra_bytes;
    if (!decoder.read(extra_bytes)) return rlp::DecodingError::kUnexpectedString;
    header.extra_data.assign(extra_bytes.begin(), extra_bytes.end());

    if (!decoder.read(header.mix_hash)) return rlp::DecodingError::kUnexpectedLength;
    if (!decoder.read(header.nonce)) return rlp::DecodingError::kUnexpectedLength;

    if (!decoder.IsFinished()) {
        intx::uint256 base_fee{};
        if (!decoder.read(base_fee)) return rlp::DecodingError::kUnexpectedString;
        header.base_fee_per_gas = base_fee;
    }

    return header;
}

} // namespace eth::codec

