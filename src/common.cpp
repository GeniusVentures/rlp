#include <common.hpp> // Direct include

// Currently empty. Could be used for:
// 1. Defining an std::error_category for rlp::DecodingError if integrating
//    with std::error_code system is desired (Boost.Outcome supports this).
// 2. Providing string conversion functions for DecodingError.

namespace rlp {

    // Example string conversion (optional)
    const char* decoding_error_to_string(DecodingError err) {
        switch (err) {
            case DecodingError::kOverflow: return "RLP Overflow";
            case DecodingError::kLeadingZero: return "RLP Leading Zero";
            case DecodingError::kInputTooShort: return "RLP Input Too Short";
            case DecodingError::kInputTooLong: return "RLP Input Too Long";
            case DecodingError::kNonCanonicalSize: return "RLP Non-Canonical Size";
            case DecodingError::kUnexpectedList: return "RLP Unexpected List";
            case DecodingError::kUnexpectedString: return "RLP Unexpected String";
            case DecodingError::kUnexpectedListElements: return "RLP Unexpected List Elements";
            case DecodingError::kInvalidVrsValue: return "RLP Invalid VRS Value";
            case DecodingError::kListLengthMismatch: return "RLP List Length Mismatch";
            case DecodingError::kNotInList: return "RLP Operation requires being in a list context";
            default: return "RLP Unknown Error";
        }
    }

} // namespace rlp
