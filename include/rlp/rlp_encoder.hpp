#ifndef RLP_ENCODER_HPP
#define RLP_ENCODER_HPP

#include <vector>
#include <span> // For span overloads if desired
#include "common.hpp"
#include "intx.hpp"
#include "endian.hpp"
#include <iostream>
namespace rlp {

class RlpEncoder {
   public:
    RlpEncoder() = default;

    // --- Add basic types ---
    EncodingOperationResult add(ByteView bytes) noexcept; // Add raw bytes (encoded as RLP string)
    EncodingOperationResult AddRaw(ByteView bytes) noexcept; // Add raw bytes without header
    // Add unsigned integrals (using SFINAE for C++17)
    template <typename T>
        auto add(const T& n) noexcept -> std::enable_if_t<is_unsigned_integral_v<T>, EncodingOperationResult>;
    EncodingOperationResult add(const intx::uint256& n) noexcept; // Explicit overload for uint256
    EncodingOperationResult BeginList() noexcept;
    EncodingOperationResult EndList() noexcept; // Calculates and inserts the list header

    // --- Convenience for Vectors ---
    // Note: Implementation needs to be here or in .ipp due to template
    template <typename T>
    EncodingOperationResult add(const std::vector<T>& vec) noexcept {
        BOOST_OUTCOME_TRY(BeginList());
        for (const auto& item : vec) {
            BOOST_OUTCOME_TRY(add(item)); // Recursively call add for each element
        }
        return EndList();
    }
     // Convenience for Spans (similar)
    template <typename T>
    EncodingOperationResult add(std::span<const T> vec_span) noexcept {
        BOOST_OUTCOME_TRY(BeginList());
        for (const auto& item : vec_span) {
            BOOST_OUTCOME_TRY(add(item)); // Recursively call add for each element
        }
        return EndList();
    }

    // --- Fixed-size array support ---
    template <size_t N>
    EncodingOperationResult add(const std::array<uint8_t, N>& arr) noexcept {
        return add(ByteView(arr.data(), arr.size()));
    }

    // --- Output ---
    [[nodiscard]] EncodingResult<const Bytes*> GetBytes() const noexcept;
    [[nodiscard]] EncodingResult<Bytes*> GetBytes() noexcept; // Mutable access for streaming
    [[nodiscard]] EncodingResult<Bytes> MoveBytes() noexcept;
    void clear() noexcept; // Reset the encoder
    
    // Get current size without copying
    [[nodiscard]] size_t size() const noexcept { return buffer_.size(); }
    
    // Reserve capacity for better performance
    void reserve(size_t capacity) noexcept { buffer_.reserve(capacity); }

   private:
    Bytes buffer_{};
    std::vector<size_t> list_start_positions_{}; // Stack to track list starts

    // --- Internal Template Implementation for Integrals ---
    // Needs to be in header if add<T> is public template method
    template <typename T>
    auto add_integral(const T& n) noexcept -> std::enable_if_t<is_unsigned_integral_v<T>, EncodingOperationResult>;
};

template <typename T>
inline auto RlpEncoder::add(const T& n) noexcept -> std::enable_if_t<is_unsigned_integral_v<T>, EncodingOperationResult> {
    if constexpr (std::is_same_v<T, bool>) {
        // Handle boolean values
        buffer_.push_back(n ? uint8_t{1} : kEmptyStringCode);
        return outcome::success();
    } else {
        // Handle other unsigned integral types
        return add_integral(n);
    }
}

template <typename T>
inline auto RlpEncoder::add_integral(const T& n) noexcept -> std::enable_if_t<is_unsigned_integral_v<T>, EncodingOperationResult> {
    if ( n == 0 ) {
        buffer_.push_back(kEmptyStringCode);
        return outcome::success();
    }
    if constexpr (sizeof(T) == 1) {
        uint8_t val = static_cast<uint8_t>(n);
        if ( val < kRlpSingleByteThreshold ) {
            buffer_.push_back(val);
        } else {
            buffer_.push_back(static_cast<uint8_t>(kShortStringOffset + 1));
            buffer_.push_back(val);
        }
    } else if constexpr (sizeof(T) == 2) {
        uint16_t val = n;
        if ( val < kRlpSingleByteThreshold ) {
            buffer_.push_back(static_cast<uint8_t>(val));
        } else {
            uint8_t buf[2];
            size_t len = 0;
            if ( val >> 8 ) buf[len++] = static_cast<uint8_t>(val >> 8);
            buf[len++] = static_cast<uint8_t>(val & 0xFF);
            buffer_.push_back(static_cast<uint8_t>(kShortStringOffset + len));
            buffer_.append(buf, len);
        }
    } else if constexpr (sizeof(T) == 4) {
        uint32_t val = n;
        if ( val < kRlpSingleByteThreshold ) {
            buffer_.push_back(static_cast<uint8_t>(val));
        } else {
            uint8_t buf[4];
            size_t len = 0;
            if ( val >> 24 ) buf[len++] = static_cast<uint8_t>(val >> 24);
            if ( val >> 16 ) buf[len++] = static_cast<uint8_t>(val >> 16);
            if ( val >> 8 ) buf[len++] = static_cast<uint8_t>(val >> 8);
            buf[len++] = static_cast<uint8_t>(val & 0xFF);
            buffer_.push_back(static_cast<uint8_t>(kShortStringOffset + len));
            buffer_.append(buf, len);
        }
    } else if constexpr (sizeof(T) == 8) {
        uint64_t val = n;
        if ( val < kRlpSingleByteThreshold ) {
            buffer_.push_back(static_cast<uint8_t>(val));
        } else {
            uint8_t buf[8];
            size_t len = 0;
            if ( val >> 56 ) buf[len++] = static_cast<uint8_t>(val >> 56);
            if ( val >> 48 ) buf[len++] = static_cast<uint8_t>(val >> 48);
            if ( val >> 40 ) buf[len++] = static_cast<uint8_t>(val >> 40);
            if ( val >> 32 ) buf[len++] = static_cast<uint8_t>(val >> 32);
            if ( val >> 24 ) buf[len++] = static_cast<uint8_t>(val >> 24);
            if ( val >> 16 ) buf[len++] = static_cast<uint8_t>(val >> 16);
            if ( val >> 8 ) buf[len++] = static_cast<uint8_t>(val >> 8);
            buf[len++] = static_cast<uint8_t>(val & 0xFF);
            buffer_.push_back(static_cast<uint8_t>(kShortStringOffset + len));
            buffer_.append(buf, len);
        }
    }
    return outcome::success();
}

} // namespace rlp

#endif // RLP_ENCODER_HPP
