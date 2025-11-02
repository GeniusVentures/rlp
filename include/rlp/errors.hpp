#ifndef RLP_ERRORS_HPP
#define RLP_ERRORS_HPP

namespace rlp {

// Encoding errors
enum class EncodingError {
    kPayloadTooLarge,        // Payload exceeds 64-bit limit
    kEmptyInput,             // Empty input where not allowed
    kUnclosedList,           // Attempted to get bytes with unclosed lists
    kUnmatchedEndList,       // EndList called without matching BeginList
};

// Decoding errors
enum class DecodingError {
    kOverflow,
    kLeadingZero,
    kInputTooShort,
    kInputTooLong,
    kNonCanonicalSize,
    kUnexpectedList,
    kUnexpectedString,
    kUnexpectedLength,
    kUnexpectedListElements,
    kInvalidVrsValue,
    kListLengthMismatch,
    kNotInList,
    kMalformedHeader,
    // Streaming-specific errors (kept here for backwards compatibility)
    kAlreadyFinalized,
    kNotFinalized,
    kInvalidChunkSize,
    kHeaderSizeExceeded,
};

// Streaming operation errors
enum class StreamingError {
    kAlreadyFinalized,
    kNotFinalized,
    kInvalidChunkSize,
    kHeaderSizeExceeded,
};

// Error stringification - implementation in common.cpp
const char* encoding_error_to_string(EncodingError err) noexcept;
const char* decoding_error_to_string(DecodingError err) noexcept;
const char* streaming_error_to_string(StreamingError err) noexcept;

} // namespace rlp

#endif // RLP_ERRORS_HPP
