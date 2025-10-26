#include <rlp/rlp_encoder.hpp> // Direct include
#include <rlp/endian.hpp>  // Direct include
#include <stdexcept>   // For std::runtime_error
#include <cstring>     // For std::memcpy

namespace rlp {

namespace { // Anonymous namespace for internal helpers

// Encodes just the header bytes into a temporary buffer
Bytes encode_header_bytes(bool list, size_t payload_size_bytes) {
    Bytes header_bytes;
    header_bytes.reserve(1 + sizeof(uint64_t)); // Max possible header size

    uint8_t short_offset = list ? kShortListOffset : kShortStringOffset;
    uint8_t long_offset = list ? kLongListOffset : kLongStringOffset;

    if ( payload_size_bytes <= kMaxShortStringLen ) { // 55 bytes
        header_bytes.push_back(static_cast<uint8_t>(short_offset + payload_size_bytes));
    } else {
        Bytes len_be = endian::to_big_compact(static_cast<uint64_t>(payload_size_bytes));
        if ( len_be.length() > 8 ) {
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
    if ( bytes.length() == 1 && static_cast<uint8_t>(bytes[0]) < kRlpSingleByteThreshold ) {
        buffer_.push_back(bytes[0]);
    } else {
        Bytes header = encode_header_bytes(false, bytes.length());
        size_t old_size = buffer_.size();
        buffer_.resize(old_size + header.length() + bytes.length());
        std::memcpy(buffer_.data() + old_size, header.data(), header.length());
        std::memcpy(buffer_.data() + old_size + header.length(), bytes.data(), bytes.length());
    }
}

/**
 * Appends raw bytes directly to the internal buffer without RLP encoding.
 * 
 * Use this method only if you have already encoded the data according to RLP rules,
 * or if you need to append a pre-encoded RLP fragment. Unlike add(), this method
 * does not perform any encoding or validation, and may result in malformed RLP output
 * if used incorrectly.
 *
 * Prefer add() for normal usage. Use AddRaw() only if you know what you are doing.
 *
 * @param bytes Raw bytes to append. Must not be empty.
 * @throws std::invalid_argument if bytes is empty.
 */
void RlpEncoder::AddRaw(ByteView bytes) {
    if ( bytes.empty() ) {
        throw std::invalid_argument("AddRaw: input bytes must not be empty");
    }
    buffer_.append(bytes);
}

// Explicit overload for uint256
void RlpEncoder::add(const intx::uint256& n) {
     if ( n == 0 ) {
        buffer_.push_back(kEmptyStringCode);
    } else if ( n < kRlpSingleByteThreshold ) {
        buffer_.push_back(static_cast<uint8_t>(n));
    } else {
        const Bytes be{endian::to_big_compact(n)};
        Bytes header = encode_header_bytes(false, be.length());
        size_t old_size = buffer_.size();
        buffer_.resize(old_size + header.length() + be.length());
        std::memcpy(buffer_.data() + old_size, header.data(), header.length());
        std::memcpy(buffer_.data() + old_size + header.length(), be.data(), be.length());
    }
}

void RlpEncoder::BeginList() {
    list_start_positions_.push_back(buffer_.size());
}

void RlpEncoder::EndList() {
    if ( list_start_positions_.empty() ) {
        throw std::logic_error("RLP EndList called without matching BeginList");
    }

    size_t start_pos = list_start_positions_.back();
    list_start_positions_.pop_back();

    size_t payload_len = buffer_.size() - start_pos;
    Bytes header = encode_header_bytes(true, payload_len);

    // Insert header at start_pos by making room and copying
    buffer_.insert(buffer_.begin() + start_pos, header.begin(), header.end());
}

const Bytes& RlpEncoder::GetBytes() const {
    if ( !list_start_positions_.empty() ) {
        throw std::logic_error("RLP encoder has unclosed lists");
    }
    return buffer_;
}

Bytes&& RlpEncoder::MoveBytes() {
     if ( !list_start_positions_.empty() ) {
        throw std::logic_error("RLP encoder has unclosed lists");
    }
    return std::move(buffer_);
}

void RlpEncoder::clear() noexcept {
    buffer_.clear();
    list_start_positions_.clear();
}


} // namespace rlp
