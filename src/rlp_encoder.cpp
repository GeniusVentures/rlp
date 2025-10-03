#include <rlp_encoder.hpp> // Direct include
#include <endian.hpp>  // Direct include
#include <stdexcept>   // For std::runtime_error

namespace rlp {

namespace { // Anonymous namespace for internal helpers

// Encodes just the header bytes into a temporary buffer
Bytes encode_header_bytes(bool list, size_t payload_length) {
    Bytes header_bytes;
    header_bytes.reserve(1 + sizeof(uint64_t)); // Max possible header size

    uint8_t short_offset = list ? kShortListOffset : kShortStringOffset;
    uint8_t long_offset = list ? kLongListOffset : kLongStringOffset;

    if (payload_length <= kMaxShortStringLen) { // 55 bytes
        header_bytes.push_back(static_cast<uint8_t>(short_offset + payload_length));
    } else {
        Bytes len_be = endian::to_big_compact(static_cast<uint64_t>(payload_length));
        if (len_be.length() > 8) {
            // RLP spec limits length field to 8 bytes
            throw std::length_error("RLP payload length exceeds 64-bit limit");
        }
        header_bytes.reserve(header_bytes.size() + 1 + len_be.length());
        header_bytes.push_back(static_cast<uint8_t>(long_offset + len_be.length()));
        header_bytes.append(len_be.data(), len_be.length());
    }
    return header_bytes;
}

} // namespace

// --- Public Method Implementations ---

void RlpEncoder::add(ByteView bytes) {
    // Handle single byte literal case
    if (bytes.length() == 1 && static_cast<uint8_t>(bytes[0]) < kRlpSingleByteThreshold) {
        buffer_.push_back(bytes[0]);
    } else {
        buffer_.append(encode_header_bytes(false, bytes.length()));
        buffer_.append(bytes);
    }
}

void RlpEncoder::addRaw(ByteView bytes) {
    buffer_.append(bytes);
}

// Explicit overload for uint256
void RlpEncoder::add(const intx::uint256& n) {
     if (n == 0) {
        buffer_.push_back(kEmptyStringCode);
    } else if (n < kRlpSingleByteThreshold) {
        buffer_.push_back(static_cast<uint8_t>(n));
    } else {
        const Bytes be{endian::to_big_compact(n)};
        buffer_.append(encode_header_bytes(false, be.length()));
        buffer_.append(be);
    }
}

void RlpEncoder::begin_list() {
    list_start_positions_.push_back(buffer_.size());
}

void RlpEncoder::end_list() {
    if (list_start_positions_.empty()) {
        throw std::logic_error("RLP end_list called without matching begin_list");
    }

    size_t start_pos = list_start_positions_.back();
    list_start_positions_.pop_back();

    size_t current_pos = buffer_.size();
    Bytes payload(buffer_.begin() + start_pos, buffer_.end());
    size_t payload_len = payload.size();
    Bytes header = encode_header_bytes(true, payload_len);

    buffer_.resize(start_pos); // Remove payload
    buffer_.insert(buffer_.end(), header.begin(), header.end()); // Insert header
    buffer_.insert(buffer_.end(), payload.begin(), payload.end()); // Insert payload
}

const Bytes& RlpEncoder::get_bytes() const {
    if (!list_start_positions_.empty()) {
        throw std::logic_error("RLP encoder has unclosed lists");
    }
    return buffer_;
}

Bytes&& RlpEncoder::move_bytes() {
     if (!list_start_positions_.empty()) {
        throw std::logic_error("RLP encoder has unclosed lists");
    }
    return std::move(buffer_);
}

void RlpEncoder::clear() noexcept {
    buffer_.clear();
    list_start_positions_.clear();
}


} // namespace rlp
