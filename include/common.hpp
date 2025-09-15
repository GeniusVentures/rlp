#ifndef RLP_COMMON_HPP
#define RLP_COMMON_HPP

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits> // For SFINAE / is_integral etc.
#include <boost/outcome/result.hpp>
#include <boost/outcome/try.hpp>

namespace rlp {

// --- Basic Types ---
using Bytes = std::basic_string<uint8_t>;
using ByteView = std::basic_string_view<uint8_t>;

// --- Concept Simulation (C++17) ---
template <typename T>
inline constexpr bool is_unsigned_integral_v = std::is_integral_v<T> && std::is_unsigned_v<T>;

// --- RLP Constants ---
inline constexpr uint8_t kEmptyStringCode{0x80};
inline constexpr uint8_t kEmptyListCode{0xC0};
inline constexpr uint8_t kMaxShortStringLen{55}; // 0xB7 - 0x80
inline constexpr uint8_t kMaxShortListLen{55}; // 0xF7 - 0xC0
inline constexpr uint8_t kShortStringOffset{0x80};
inline constexpr uint8_t kLongStringOffset{0xB7}; // 0x80 + 55 + 1
inline constexpr uint8_t kShortListOffset{0xC0};
inline constexpr uint8_t kLongListOffset{0xF7}; // 0xC0 + 55 + 1
inline constexpr uint8_t kRlpSingleByteThreshold{0x80};  // Values below this are encoded as a single byte

// --- RLP Header (Internal Use) ---
// Note: Users interact via Encoder/Decoder methods, not directly with Header
struct Header {
    bool list{false};
    size_t payload_length{0};
    size_t header_length{0}; // Length of the RLP prefix itself
};

// --- Error Handling ---
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
    kListLengthMismatch, // Added for decoder list helpers
    kNotInList, // Added for decoder if trying list ops outside a list context
};

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

template <class T>
using Result = outcome::result<T, DecodingError, outcome::policy::all_narrow>;

using DecodingResult = outcome::result<void, DecodingError, outcome::policy::all_narrow>;

// --- Leftover Handling (for decode methods) ---
enum class Leftover {
    kProhibit,
    kAllow,
};

// --- Error Stringification (Declaration) ---
// Implementation is in common.cpp
const char* decoding_error_to_string(DecodingError err); // <<< ADD THIS DECLARATION

} // namespace rlp

#endif // RLP_COMMON_HPP
