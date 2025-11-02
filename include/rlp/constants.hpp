#ifndef RLP_CONSTANTS_HPP
#define RLP_CONSTANTS_HPP

#include <cstdint>

namespace rlp {

// RLP encoding constants
inline constexpr uint8_t kEmptyStringCode{0x80};
inline constexpr uint8_t kEmptyListCode{0xC0};
inline constexpr uint8_t kMaxShortStringLen{55}; // 0xB7 - 0x80
inline constexpr uint8_t kMaxShortListLen{55}; // 0xF7 - 0xC0
inline constexpr uint8_t kShortStringOffset{0x80};
inline constexpr uint8_t kLongStringOffset{0xB7}; // 0x80 + 55 + 1
inline constexpr uint8_t kShortListOffset{0xC0};
inline constexpr uint8_t kLongListOffset{0xF7}; // 0xC0 + 55 + 1
inline constexpr uint8_t kRlpSingleByteThreshold{0x80};  // Values below this are encoded as a single byte

// RLP header structure (internal use)
struct Header {
    bool list{false};
    size_t payload_size_bytes{0};  // Size of payload in bytes (not item count for lists)
    size_t header_size_bytes{0};   // Size of the RLP header/prefix itself in bytes
};

// Leftover handling mode for decoding
enum class Leftover {
    kProhibit,
    kAllow,
};

} // namespace rlp

#endif // RLP_CONSTANTS_HPP
