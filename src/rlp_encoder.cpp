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

// Explicit overload for uint256
void RlpEncoder::add(const intx::uint256& n) {
    add_uint256(n); // Call private implementation detail
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
    size_t payload_len = current_pos - start_pos;

    Bytes header = encode_header_bytes(true, payload_len);

    // Insert header at the start position
    buffer_.insert(buffer_.begin() + start_pos, header.begin(), header.end());
}

const Bytes& RlpEncoder::get_bytes() const noexcept {
    if (!list_start_positions_.empty()) {
        // Indicate misuse - maybe throw or log? For now, just return incomplete buffer.
    }
    return buffer_;
}

Bytes&& RlpEncoder::move_bytes() noexcept {
     if (!list_start_positions_.empty()) {
        // Indicate misuse
    }
    return std::move(buffer_);
}

void RlpEncoder::clear() noexcept {
    buffer_.clear();
    list_start_positions_.clear();
}

// --- Private Implementations ---
// Need to explicitly instantiate add_integral for common types if add<T> is non-template
// Since add<T> IS a template in the header, the template implementation MUST be in the header.
// Let's remove the add_integral and add_uint256 private methods and keep the implementation
// fully within the template methods in the header (as corrected before).
// The only non-template add methods that need implementing here are `add(ByteView)` and `add(bool)`.

// Re-evaluating the private add_uint256 - this CAN be implemented here
// as it's called by the explicit public overload add(const intx::uint256&).

void RlpEncoder::add_uint256(const intx::uint256& n) {
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


} // namespace rlp
