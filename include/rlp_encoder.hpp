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

    // --- List Handling ---
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
    [[nodiscard]] const Bytes& get_bytes() const noexcept; // Get encoded result
    Bytes&& move_bytes() noexcept; // Move encoded result out
    void clear() noexcept; // Reset the encoder

   private:
    Bytes buffer_{};
    std::vector<size_t> list_start_positions_{}; // Stack to track list starts

    // --- Internal Template Implementation for Integrals ---
    // Needs to be in header if add<T> is public template method
    template <typename T>
    auto add_integral(const T& n) -> std::enable_if_t<is_unsigned_integral_v<T>>;
    void add_uint256(const intx::uint256& n);
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
    } else if constexpr (sizeof(T) == 1) {
         uint8_t val = static_cast<uint8_t>(n);
         if (val < kRlpSingleByteThreshold) {
             buffer_.push_back(val);
         } else {
             // Encode as short string
              Bytes header_bytes;
              size_t payload_len = 1;
              header_bytes.push_back(static_cast<uint8_t>(kShortStringOffset + payload_len));
             buffer_.append(header_bytes);
             buffer_.push_back(val);
         }
    } else {
         intx::uint256 val{n};
         if (val < kRlpSingleByteThreshold) {
              buffer_.push_back(static_cast<uint8_t>(val));
         } else {
              const Bytes be{endian::to_big_compact(val)};
              // Encode header
              Bytes header_bytes;
              size_t payload_len = be.length();
              if (payload_len < kMaxShortStringLen + 1) {
                   header_bytes.push_back(static_cast<uint8_t>(kShortStringOffset + payload_len));
              } else {
                   Bytes len_be = endian::to_big_compact(static_cast<uint64_t>(payload_len));
                   header_bytes.push_back(static_cast<uint8_t>(kLongStringOffset + len_be.length()));
                   header_bytes.append(len_be);
              }
              buffer_.append(header_bytes);
              buffer_.append(be);
         }
    }
}


} // namespace rlp

#endif // RLP_ENCODER_HPP
