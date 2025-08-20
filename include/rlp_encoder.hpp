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
    void add(ByteView bytes); // Add raw bytes (encoded as RLP string)
    void addRaw(ByteView bytes); // Add raw bytes (encoded as RLP string) without header
    // Add unsigned integrals (using SFINAE for C++17)
    template <typename T, UnsignedIntegral<T>>
        void add(const T& n);
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
    void add_integral(const T& n);
    void add_uint256(const intx::uint256& n);
};

template <typename T, UnsignedIntegral<T>>
inline void RlpEncoder::add(const T& n) {
    if constexpr (std::is_same_v<T, bool>) {
        // Handle boolean values
        buffer_.push_back(n ? uint8_t{1} : kEmptyStringCode);
    } else {
        // Handle other unsigned integral types
        add_integral(n);
    }
}

template <typename T>
inline void RlpEncoder::add_integral(const T& n) {
    // Implementation moved to encoder.cpp, called from non-template add method
    // This function can be removed if add<T> implementation stays in header
    // If we keep non-template add() calling template add_integral(),
    // add_integral MUST be defined in the header. Let's move logic to cpp.
    // We will have explicit overloads in the header and implementations in cpp.
     if (n == 0) {
        buffer_.push_back(kEmptyStringCode);
    } else if constexpr (sizeof(T) == 1) {
         uint8_t val = static_cast<uint8_t>(n);
         if (val < kRlpSingleByteThreshold) {
             buffer_.push_back(val);
         } else {
             // Needs header logic from encode_helpers.cpp
             // Let's call a private non-template helper implemented in cpp
             // This design doesn't work well - keep template implementation here for now
              Bytes header_bytes;
              size_t payload_len = 1;
              if (payload_len < kMaxShortStringLen + 1) { // 56
                   header_bytes.push_back(static_cast<uint8_t>(kShortStringOffset + payload_len));
              } else {
                   // Should not happen for single byte, but general logic:
                   Bytes len_be = endian::to_big_compact(static_cast<uint64_t>(payload_len));
                   header_bytes.push_back(static_cast<uint8_t>(kLongStringOffset + len_be.length()));
                   header_bytes.append(len_be);
              }
             buffer_.append(header_bytes);
             buffer_.push_back(val);
         }
    } else {
         intx::uint256 val{n};
         if (val < kRlpSingleByteThreshold) {
              buffer_.push_back(static_cast<uint8_t>(val));
         } else {
              const Bytes be{endian::to_big_compact(val)};
              // Needs header logic
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
