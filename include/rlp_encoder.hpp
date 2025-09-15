#ifndef RLP_ENCODER_HPP
#define RLP_ENCODER_HPP

#include <vector>
#include <span> // For span overloads if desired
#include "common.hpp"
#include "intx.hpp"
#include "endian.hpp"

namespace rlp {

class RlpEncoder {
   public:
    RlpEncoder() = default;

    // --- Add basic types ---
    void add(ByteView bytes); // Add raw bytes (encoded as RLP string)
    // Add unsigned integrals (using SFINAE for C++17)
    template <typename T>
        auto add(const T& n) -> std::enable_if_t<is_unsigned_integral_v<T>>;
    void add(const intx::uint256& n); // Explicit overload for uint256
    void begin_list();
    void end_list(); // Calculates and inserts the list header

    // --- Convenience for Vectors ---
    // Note: Implementation needs to be here or in .ipp due to template
    template <typename T>
    void add(const std::vector<T>& vec) {
        begin_list();
        for (const auto& item : vec) {
            add(item); // Recursively call add for each element
        }
        end_list();
    }
     // Convenience for Spans (similar)
    template <typename T>
    void add(std::span<const T> vec_span) {
        begin_list();
        for (const auto& item : vec_span) {
            add(item); // Recursively call add for each element
        }
        end_list();
    }

    // --- Output ---
    [[nodiscard]] const Bytes& get_bytes() const;
    Bytes&& move_bytes();
    void clear() noexcept; // Reset the encoder

   private:
    Bytes buffer_{};
    std::vector<size_t> list_start_positions_{}; // Stack to track list starts

    // --- Internal Template Implementation for Integrals ---
    // Needs to be in header if add<T> is public template method
    template <typename T>
    auto add_integral(const T& n) -> std::enable_if_t<is_unsigned_integral_v<T>>;
};

template <typename T>
inline auto RlpEncoder::add(const T& n) -> std::enable_if_t<is_unsigned_integral_v<T>> {
    if constexpr (std::is_same_v<T, bool>) {
        // Handle boolean values
        buffer_.push_back(n ? uint8_t{1} : kEmptyStringCode);
    } else {
        // Handle other unsigned integral types
        add_integral(n);
    }
}

template <typename T>
inline auto RlpEncoder::add_integral(const T& n) -> std::enable_if_t<is_unsigned_integral_v<T>> {
    if (n == 0) {
        buffer_.push_back(kEmptyStringCode);
        return;
    }
    if constexpr (sizeof(T) == 1) {
        uint8_t val = static_cast<uint8_t>(n);
        if (val < kRlpSingleByteThreshold) {
            buffer_.push_back(val);
        } else {
            buffer_.push_back(static_cast<uint8_t>(kShortStringOffset + 1));
            buffer_.push_back(val);
        }
    } else if constexpr (sizeof(T) == 2) {
        uint16_t val = n;
        if (val < kRlpSingleByteThreshold) {
            buffer_.push_back(static_cast<uint8_t>(val));
        } else {
            uint8_t buf[2];
            size_t len = 0;
            if (val >> 8) buf[len++] = static_cast<uint8_t>(val >> 8);
            buf[len++] = static_cast<uint8_t>(val & 0xFF);
            buffer_.push_back(static_cast<uint8_t>(kShortStringOffset + len));
            buffer_.append(buf, len);
        }
    } else if constexpr (sizeof(T) == 4) {
        uint32_t val = n;
        if (val < kRlpSingleByteThreshold) {
            buffer_.push_back(static_cast<uint8_t>(val));
        } else {
            uint8_t buf[4];
            size_t len = 0;
            if (val >> 24) buf[len++] = static_cast<uint8_t>(val >> 24);
            if (val >> 16) buf[len++] = static_cast<uint8_t>(val >> 16);
            if (val >> 8) buf[len++] = static_cast<uint8_t>(val >> 8);
            buf[len++] = static_cast<uint8_t>(val & 0xFF);
            buffer_.push_back(static_cast<uint8_t>(kShortStringOffset + len));
            buffer_.append(buf, len);
        }
    } else if constexpr (sizeof(T) == 8) {
        uint64_t val = n;
        if (val < kRlpSingleByteThreshold) {
            buffer_.push_back(static_cast<uint8_t>(val));
        } else {
            uint8_t buf[8];
            size_t len = 0;
            if (val >> 56) buf[len++] = static_cast<uint8_t>(val >> 56);
            if (val >> 48) buf[len++] = static_cast<uint8_t>(val >> 48);
            if (val >> 40) buf[len++] = static_cast<uint8_t>(val >> 40);
            if (val >> 32) buf[len++] = static_cast<uint8_t>(val >> 32);
            if (val >> 24) buf[len++] = static_cast<uint8_t>(val >> 24);
            if (val >> 16) buf[len++] = static_cast<uint8_t>(val >> 16);
            if (val >> 8) buf[len++] = static_cast<uint8_t>(val >> 8);
            buf[len++] = static_cast<uint8_t>(val & 0xFF);
            buffer_.push_back(static_cast<uint8_t>(kShortStringOffset + len));
            buffer_.append(buf, len);
        }
    }
}


} // namespace rlp

#endif // RLP_ENCODER_HPP
