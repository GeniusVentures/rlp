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
#include "intx.hpp"

namespace rlp {

// --- Basic Types ---
using Bytes = std::basic_string<uint8_t>;
using ByteView = std::basic_string_view<uint8_t>;

// --- Concept Simulation (C++17) ---
template <typename T>
inline constexpr bool is_unsigned_integral_v = std::is_integral_v<T> && std::is_unsigned_v<T>;

// --- Type Traits for Type Safety (SFINAE) ---
// Type trait to check if a type is RLP-encodable
template <typename T>
struct is_rlp_encodable : std::disjunction<
    std::conjunction<std::is_integral<T>, std::is_unsigned<T>>,
    std::is_same<T, bool>,
    std::is_same<T, intx::uint256>,
    std::is_same<T, Bytes>,
    std::is_same<T, ByteView>
> {};

template <typename T>
inline constexpr bool is_rlp_encodable_v = is_rlp_encodable<T>::value;

// Type trait to check if a type is RLP-decodable
template <typename T>
struct is_rlp_decodable : std::disjunction<
    std::conjunction<std::is_integral<T>, std::is_unsigned<T>>,
    std::is_same<T, bool>,
    std::is_same<T, intx::uint256>,
    std::is_same<T, Bytes>
> {};

template <typename T>
inline constexpr bool is_rlp_decodable_v = is_rlp_decodable<T>::value;

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
    size_t payload_size_bytes{0};  // Size of payload in bytes (not item count for lists)
    size_t header_size_bytes{0};   // Size of the RLP header/prefix itself in bytes
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
    kMalformedHeader, // Added for reserved/malformed header bytes
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

std::string hexToString(rlp::ByteView bv);
} // namespace rlp

#endif // RLP_COMMON_HPP
