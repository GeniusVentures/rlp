#include <rlp/endian.hpp> // Direct include
#include <rlp/common.hpp> // Direct include (for outcome)
#include <rlp/intx.hpp>   // Include intx header
#include <algorithm>
#include <stdexcept> // For errors potentially not covered by DecodingError
#include <limits>

namespace rlp::endian {

// --- Implementation for standard unsigned types ---
// Signature now matches the header's SFINAE-on-return-type style
template <typename T>
auto to_big_compact(const T& n) -> std::enable_if_t<is_unsigned_integral_v<T>, Bytes> {
    if (n == 0) {
        return {};
    }
    intx::uint256 val{n};
    size_t num_bytes = intx::count_significant_bytes(val);

    // Create a temporary buffer large enough for the full uint256
    uint8_t temp_buffer[32] = {0};

    // Store the full value in the temporary buffer
    intx::be::store(temp_buffer, val);

    // Only copy the significant bytes to the result
    Bytes bytes(&temp_buffer[32 - num_bytes], num_bytes);
    return bytes;
}

    template <typename T>
    auto from_big_compact(ByteView bytes, T& out) -> std::enable_if_t<is_unsigned_integral_v<T>, DecodingResult> {
    if (bytes.empty()) {
        out = 0;
        return outcome::success();
    }
    if (bytes[0] == 0 && bytes.length() > 1) {
        return DecodingError::kLeadingZero;
    }
    if (bytes.length() == 1 && bytes[0] == 0) {
        out = 0;
        return outcome::success();
    }

    // Check if bytes might be too large for the target type
    if (bytes.length() > sizeof(intx::uint256)) {
        return DecodingError::kOverflow;
    }

    // Create a properly padded buffer with zeros
    uint8_t padded[32] = {0};

    // Copy the input bytes to the end of the padded buffer for big-endian format
    std::memcpy(padded + (32 - bytes.length()), bytes.data(), bytes.length());

    // Use the safe load function
    intx::uint256 val = intx::be::load<intx::uint256>(padded);

    // Check if the value fits in the target type
    if (val > std::numeric_limits<T>::max()) {
        return DecodingError::kOverflow;
    }

    out = static_cast<T>(val);
    return outcome::success();
}

// --- Implementation for intx::uint256 --- (Signatures unchanged)
    // --- Implementation for intx::uint256 --- (Signatures unchanged)
    Bytes to_big_compact(const intx::uint256& n) {
    if (n == 0) {
        return {};
    }
    size_t num_bytes = intx::count_significant_bytes(n);

    // Create a temporary buffer large enough for the full uint256
    uint8_t temp_buffer[32] = {0};

    // Store the full value in the temporary buffer
    intx::be::store(temp_buffer, n);

    // Only copy the significant bytes to the result
    Bytes bytes(&temp_buffer[32 - num_bytes], num_bytes);
    return bytes;
}

    DecodingResult from_big_compact(ByteView bytes, intx::uint256& out) {
    if (bytes.empty()) {
        out = 0;
        return outcome::success();
    }
    if (bytes[0] == 0 && bytes.length() > 1) {
        return DecodingError::kLeadingZero;
    }
    if (bytes.length() == 1 && bytes[0] == 0) {
        out = 0;
        return outcome::success();
    }

    if (bytes.length() > sizeof(intx::uint256)) {
        return DecodingError::kOverflow;
    }

    // Create a properly padded buffer with zeros
    uint8_t padded[32] = {0};

    // Copy the input bytes to the end of the padded buffer
    // For big-endian, we need to copy to the last bytes
    // (32 - bytes.length()) gives us the offset where the significant bytes start
    std::memcpy(padded + (32 - bytes.length()), bytes.data(), bytes.length());

    // Use the safe load function
    out = intx::be::load<intx::uint256>(padded);

    return outcome::success();
}

// --- Explicit Template Instantiations ---
// The signature MUST exactly match the definition signature, including the SFINAE part

template auto to_big_compact<uint8_t>(const uint8_t&) -> std::enable_if_t<is_unsigned_integral_v<uint8_t>, Bytes>;
template auto to_big_compact<uint16_t>(const uint16_t&) -> std::enable_if_t<is_unsigned_integral_v<uint16_t>, Bytes>;
template auto to_big_compact<uint32_t>(const uint32_t&) -> std::enable_if_t<is_unsigned_integral_v<uint32_t>, Bytes>;
template auto to_big_compact<uint64_t>(const uint64_t&) -> std::enable_if_t<is_unsigned_integral_v<uint64_t>, Bytes>;

template auto from_big_compact<uint8_t>(ByteView, uint8_t&) -> std::enable_if_t<is_unsigned_integral_v<uint8_t>, DecodingResult>;
template auto from_big_compact<uint16_t>(ByteView, uint16_t&) -> std::enable_if_t<is_unsigned_integral_v<uint16_t>, DecodingResult>;
template auto from_big_compact<uint32_t>(ByteView, uint32_t&) -> std::enable_if_t<is_unsigned_integral_v<uint32_t>, DecodingResult>;
template auto from_big_compact<uint64_t>(ByteView, uint64_t&) -> std::enable_if_t<is_unsigned_integral_v<uint64_t>, DecodingResult>;


} // namespace rlp::endian
