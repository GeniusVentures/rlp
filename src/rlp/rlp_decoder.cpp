#include <rlp/rlp_decoder.hpp> // Direct include
#include <rlp/endian.hpp>  // Direct include
#include <cstring>     // For memcpy
#include <stdexcept>   // For std::length_error

namespace rlp {

namespace { // Anonymous namespace for internal helpers

// Decodes RLP header FROM THE START of the provided view 'v'
// Modifies 'v' to remove the consumed header bytes.
// Returns header info including header length itself.
[[nodiscard]] Result<Header> decode_header_impl(ByteView& v) noexcept {
    if ( v.empty() ) {
        return DecodingError::kInputTooShort;
    }

    Header h{.list = false, .payload_size_bytes = 0, .header_size_bytes = 0};
    const uint8_t b{v[0]};
    const size_t input_len = v.length();

    // Reserved bytes (0xf9–0xff) as single bytes are always invalid
    if ( input_len == 1 && b >= 0xf9 && b <= 0xff ) {
        return DecodingError::kMalformedHeader;
    }

    if ( b < kShortStringOffset ) { // 0x80
        // Single byte literal (valid for 0x00–0x7f)
        h.payload_size_bytes = 1;
        h.header_size_bytes = 0; // Header is implicit
        // Do not consume 'v' here, payload is the byte itself
    } else if ( b <= kMaxShortStringLen + kShortStringOffset ) { // 0xB7
        // Short string
        h.header_size_bytes = 1;
        if ( input_len < h.header_size_bytes ) return DecodingError::kInputTooShort;
        h.payload_size_bytes = b - kShortStringOffset;
        if ( h.payload_size_bytes == 1 ) {
            if ( input_len < h.header_size_bytes + 1 ) return DecodingError::kInputTooShort;
            if ( static_cast<uint8_t>(v[1]) < kShortStringOffset ) {
                return DecodingError::kNonCanonicalSize; // byte < 0x80 must be encoded as itself
            }
        }
        v.remove_prefix(h.header_size_bytes); // Consume header byte
    } else if ( b < kShortListOffset ) { // 0xC0
        // Long string
        h.header_size_bytes = 1 + (b - kLongStringOffset);
        if ( input_len < h.header_size_bytes ) return DecodingError::kInputTooShort;

        uint64_t len64{0};
        ByteView len_bytes = v.substr(1, h.header_size_bytes - 1);

        auto len_res = endian::from_big_compact(len_bytes, len64);
        if ( !len_res ) return len_res.error();

        if ( len64 <= kMaxShortStringLen ) { // Must use short form if length <= 55
            return DecodingError::kNonCanonicalSize;
        }
        // Check for overflow if size_t is smaller than uint64_t
        if ( len64 > std::numeric_limits<size_t>::max() ) {
            return DecodingError::kOverflow;
        }
        h.payload_size_bytes = static_cast<size_t>(len64);
        v.remove_prefix(h.header_size_bytes); // Consume header + length bytes
    } else if ( b <= kMaxShortListLen + kShortListOffset ) { // 0xF7
        // Short list
        h.list = true;
        h.header_size_bytes = 1;
        if ( input_len < h.header_size_bytes ) return DecodingError::kInputTooShort;
        h.payload_size_bytes = b - kShortListOffset;
        v.remove_prefix(h.header_size_bytes); // Consume header byte
    } else {
        // Long list
        h.list = true;
        h.header_size_bytes = 1 + (b - kLongListOffset);
        if ( input_len < h.header_size_bytes ) return DecodingError::kInputTooShort;

        uint64_t len64{0};
        ByteView len_bytes = v.substr(1, h.header_size_bytes - 1);

        auto len_res = endian::from_big_compact(len_bytes, len64);
        if ( !len_res ) return len_res.error();

        if ( len64 <= kMaxShortListLen ) { // Must use short form if length <= 55
            return DecodingError::kNonCanonicalSize;
        }
        if ( len64 > std::numeric_limits<size_t>::max() ) {
            return DecodingError::kOverflow;
        }
        h.payload_size_bytes = static_cast<size_t>(len64);
        v.remove_prefix(h.header_size_bytes); // Consume header + length bytes
    }

    // Final check: Is remaining data sufficient for the payload?
    // Exception: single byte literal case already checked implicitly by v not being empty.
    if ( b >= kShortStringOffset ) { // Check only if header was consumed
        if ( v.length() < h.payload_size_bytes ) {
            return DecodingError::kInputTooShort;
        }
    } else { // Single byte literal case, check original length implicitly via initial check
        if ( input_len < 1 ) return DecodingError::kInputTooShort; // Should be caught by initial empty check
    }


    return h;
}


} // namespace


// --- Constructor ---
RlpDecoder::RlpDecoder(ByteView data) noexcept : view_(data) {}

// --- State Checks ---
bool RlpDecoder::IsFinished() const noexcept {
    return view_.empty();
}

ByteView RlpDecoder::Remaining() const noexcept {
    return view_;
}

// --- Type Checks (Peek) ---
Result<bool> RlpDecoder::IsList() const noexcept {
    if ( view_.empty() ) return DecodingError::kInputTooShort;
    uint8_t b = view_[0];
    return (b >= kShortListOffset); // Covers short and long lists
}

Result<bool> RlpDecoder::IsString() const noexcept {
    if ( view_.empty() ) return DecodingError::kInputTooShort;
    uint8_t b = view_[0];
    return (b < kShortListOffset); // Covers single byte, short string, long string
}

Result<size_t> RlpDecoder::PeekPayloadSizeBytes() const noexcept {
    ByteView temp_view = view_; // Copy view to peek without consuming
    BOOST_OUTCOME_TRY(auto h, decode_header_impl(temp_view));
    return h.payload_size_bytes;
}

Result<Header> RlpDecoder::PeekHeader() const noexcept {
    ByteView temp_view = view_; // Copy view to peek without consuming
    return decode_header_impl(temp_view); // Decode from the copy
}

// --- Read Basic Types (Consume) ---

DecodingResult RlpDecoder::read(Bytes& out) noexcept {
    BOOST_OUTCOME_TRY(auto h, PeekHeader()); // Peek header first

    if ( h.list ) {
        return DecodingError::kUnexpectedList;
    }

    BOOST_OUTCOME_TRY(skip_header_internal()); // Consume header from member view_

    if ( view_.length() < h.payload_size_bytes ) return DecodingError::kInputTooShort; // Double check

    // Assign payload
    out.assign(reinterpret_cast<const uint8_t*>(view_.data()), h.payload_size_bytes);

    // Consume payload
    view_.remove_prefix(h.payload_size_bytes);

    return outcome::success(); // Success
}

// Explicit overload for uint256
DecodingResult RlpDecoder::read(intx::uint256& out) noexcept {
    return read_uint256(out); // Call private implementation detail
}

// --- List Handling (Consume) ---

Result<size_t> RlpDecoder::ReadListHeaderBytes() noexcept {
    BOOST_OUTCOME_TRY(auto h, PeekHeader()); // Peek first

    if ( !h.list ) {
        return DecodingError::kUnexpectedString;
    }

    BOOST_OUTCOME_TRY(skip_header_internal()); // Consume header from member view_

    return h.payload_size_bytes; // Return payload length in bytes
}

DecodingResult RlpDecoder::SkipItem() noexcept {
    BOOST_OUTCOME_TRY(auto h, PeekHeader()); // Peek first

    BOOST_OUTCOME_TRY(skip_header_internal()); // Consume header

    // Consume payload
    if ( view_.length() < h.payload_size_bytes ) return DecodingError::kInputTooShort;
    view_.remove_prefix(h.payload_size_bytes);

    return outcome::success(); // Success
}

// --- Internal Helpers (Implementation) ---

// Decodes header from VIEW 'v', advances 'v' past header bytes
Result<Header> RlpDecoder::decode_header_internal(ByteView& v) const noexcept {
    // Use the static helper implementation
    return decode_header_impl(v);
}

// Consumes header from MEMBER view_
DecodingResult RlpDecoder::skip_header_internal() noexcept {
    if ( view_.empty() ) return DecodingError::kInputTooShort;
    uint8_t b = view_[0];
    size_t header_len = 0;

    if ( b < kShortStringOffset ) { // Single byte literal
        header_len = 0; // No header bytes to skip
    } else if ( b <= kMaxShortStringLen + kShortStringOffset ) { // Short string
        header_len = 1;
    } else if ( b < kShortListOffset ) { // Long string
        header_len = 1 + (b - kLongStringOffset);
    } else if ( b <= kMaxShortListLen + kShortListOffset ) { // Short list
        header_len = 1;
    } else { // Long list
        header_len = 1 + (b - kLongListOffset);
    }

    if ( view_.length() < header_len ) return DecodingError::kInputTooShort;
    view_.remove_prefix(header_len);
    return outcome::success();
}


// --- Private Implementations for read ---
// Needs to be explicitly instantiated for common types if read<T> is non-template
// Since read<T> IS a template in the header, the implementation MUST be in the header.
// Let's remove the read_integral and read_uint256 private methods.
// The only non-template read methods needing impl here are read(Bytes&) and read(bool&).

    DecodingResult RlpDecoder::read_uint256(intx::uint256& out) noexcept {
    // *** Implement directly instead of calling public template ***
    BOOST_OUTCOME_TRY(auto h, PeekHeader()); // Peek first

    if ( h.list ) {
        return DecodingError::kUnexpectedList;
    }

    ByteView payload_view = view_.substr(h.header_size_bytes, h.payload_size_bytes); // View payload

    // Perform checks on payload_view before decoding
    if ( h.payload_size_bytes > 1 && payload_view[0] == 0 ) {
        return DecodingError::kLeadingZero;
    }
    // Allow 0x00 to decode to 0, handled by from_big_compact.
    // Redundant check removed for clarity.

    if ( h.payload_size_bytes > 32 ) { // Max bytes for uint256
        return DecodingError::kOverflow;
    }

    // Decode from payload_view using endian function
    BOOST_OUTCOME_TRY(endian::from_big_compact(payload_view, out));

    // Check canonical single byte encoding AFTER decoding value
    if ( h.payload_size_bytes == 1 && out < kRlpSingleByteThreshold ) {
        // Ensure header was just the byte itself (header_size_bytes == 0)
        if ( h.header_size_bytes > 0 ) {
            return DecodingError::kNonCanonicalSize;
        }
    }

    // Consume header + payload from the main view_
    if ( view_.length() < h.header_size_bytes + h.payload_size_bytes ) return DecodingError::kInputTooShort;
    view_.remove_prefix(h.header_size_bytes + h.payload_size_bytes);

    return outcome::success(); // Success
}

// --- Streaming Support Implementation ---

Result<ByteView> RlpDecoder::PeekPayload() const noexcept {
    BOOST_OUTCOME_TRY(auto h, PeekHeader());
    
    if (h.list) {
        return DecodingError::kUnexpectedList;
    }
    
    if (view_.length() < h.header_size_bytes + h.payload_size_bytes) {
        return DecodingError::kInputTooShort;
    }
    
    // Return view of payload without consuming
    return view_.substr(h.header_size_bytes, h.payload_size_bytes);
}


} // namespace rlp


