#ifndef RLP_DECODER_HPP
#define RLP_DECODER_HPP

#include <vector>
#include <span> // For span overloads if desired
#include "common.hpp"
#include "intx.hpp"
#include "endian.hpp"

namespace rlp {

class RlpDecoder {
   public:
    explicit RlpDecoder(ByteView data) noexcept;

    // --- State Checks ---
    [[nodiscard]] bool is_finished() const noexcept; // No more data left
    [[nodiscard]] ByteView remaining() const noexcept; // View of remaining data

    // --- Type Checks (Peek) ---
    [[nodiscard]] Result<bool> is_list() const noexcept; // Is the next item a list?
    [[nodiscard]] Result<bool> is_string() const noexcept; // Is the next item a string?
    [[nodiscard]] Result<size_t> peek_payload_length() const noexcept; // Get next item's payload length
    [[nodiscard]] Result<Header> peek_header() const noexcept; // Get full header info

    // --- Read Basic Types (Consume) ---
    // Returns error or success. Value is output parameter.
    DecodingResult read(Bytes& out); // Read next item as bytes (string payload)
    DecodingResult read(intx::uint256& out); // Explicit overload for uint256

    // Template read for integral types and uint256
    template <typename T>
    auto read(T& out) -> std::enable_if_t<is_unsigned_integral_v<T> || std::is_same_v<T, intx::uint256> || std::is_same_v<T, bool>, DecodingResult> {
        if constexpr (std::is_same_v<T, intx::uint256>) {
            return read(static_cast<intx::uint256&>(out)); // Call explicit uint256 overload
        } else if constexpr (std::is_same_v<T, bool>) {
            return read_bool(out);
        } else {
            return read_integral(out);
        }
    }

    // --- List Handling (Consume) ---
    // Reads *only* the list header, returns payload length, consumes header bytes
    Result<size_t> read_list_header() noexcept;
    // Skips the next complete RLP item (header + payload)
    DecodingResult skip_item() noexcept;

    // Helper method for reading with a specified ByteView and leftover handling
    template <typename T>
    auto read(ByteView& data, T& out, Leftover leftover = Leftover::kProhibit) -> std::enable_if_t<is_unsigned_integral_v<T> || std::is_same_v<T, bool> || std::is_same_v<T, intx::uint256>, DecodingResult>
    {
        RlpDecoder temp_decoder(data);
        BOOST_OUTCOME_TRY(auto h, temp_decoder.peek_header());
        ByteView payload_view = temp_decoder.view_.substr(h.header_length, h.payload_length);
        BOOST_OUTCOME_TRY(temp_decoder.check_payload<T>(h, payload_view, temp_decoder.view_));

        if constexpr (std::is_same_v<T, bool>)
        {
            if (h.payload_length == 1)
            {
                if (payload_view[0] == 1)
                {
                    out = true;
                    temp_decoder.view_.remove_prefix(h.header_length + h.payload_length);
                }
                else
                {
                    BOOST_OUTCOME_TRY(temp_decoder.skip_item());
                    return DecodingError::kOverflow;
                }
            }
            else if (h.payload_length == 0)
            {
                if (h.header_length == 1 && temp_decoder.view_[0] == kEmptyStringCode)
                {
                    out = false;
                    temp_decoder.view_.remove_prefix(1);
                }
                else
                {
                    BOOST_OUTCOME_TRY(temp_decoder.skip_item());
                    return DecodingError::kOverflow;
                }
            }
            else
            {
                BOOST_OUTCOME_TRY(temp_decoder.skip_item());
                return DecodingError::kOverflow;
            }
        }
        else
        {
            BOOST_OUTCOME_TRY(endian::from_big_compact(payload_view, out));
            temp_decoder.view_.remove_prefix(h.header_length + h.payload_length);
        }
        data = temp_decoder.remaining();
        if (leftover == Leftover::kProhibit && !temp_decoder.is_finished())
        {
            return DecodingError::kInputTooLong;
        }
        return outcome::success();
    }

    template <typename T>
    auto check_payload(Header& h, ByteView& payload_view, ByteView& view) -> std::enable_if_t<is_unsigned_integral_v<T> || std::is_same_v<T, bool> || std::is_same_v<T, intx::uint256>, DecodingResult>
    {
        if (h.list)
        {
            return DecodingError::kUnexpectedList;
        }
        if (h.payload_length > 1 && payload_view[0] == 0)
        {
            return DecodingError::kLeadingZero;
        }
        if (h.payload_length == 1 && payload_view[0] == 0 && static_cast<uint8_t>(T{0U}) >= kRlpSingleByteThreshold)
        {
            return DecodingError::kLeadingZero;
        }
        if (h.payload_length > sizeof(T))
        {
            if constexpr (sizeof(T) < 32)
            {
                if (h.payload_length > sizeof(T))
                {
                    return DecodingError::kOverflow;
                }
            }
            else
            {
                if (h.payload_length > 32)
                {
                    return DecodingError::kOverflow;
                }
            }
        }
        if (view.length() < h.header_length + h.payload_length)
        {
            return DecodingError::kInputTooShort;
        }
        if constexpr (!std::is_same_v<T, bool>)
        {
            T temp_out;
            BOOST_OUTCOME_TRY(endian::from_big_compact(payload_view, temp_out));
            if (h.payload_length == 1 && static_cast<uint8_t>(temp_out) < kRlpSingleByteThreshold)
            {
                if (h.header_length > 0)
                {
                    return DecodingError::kNonCanonicalSize;
                }
            }
        }
        return outcome::success();
    }

    // --- Convenience for Vectors (Consume) ---
    // Reads a complete list assuming all items are of type T
    // Note: Implementation needs to be here or in .ipp due to template
    template <typename T>
    DecodingResult read_vector(std::vector<T>& vec) {
        BOOST_OUTCOME_TRY(size_t payload_len, read_list_header());

        vec.clear();
        ByteView list_payload = view_.substr(0, payload_len); // View only the list payload
        ByteView original_list_payload = list_payload; // To check consumption

        while (!list_payload.empty()) {
            vec.emplace_back();
            // Use the main read<T> method, allowing leftovers within the list payload view
            auto read_res = read(list_payload, vec.back(), Leftover::kAllow);
            if (!read_res) {
                vec.pop_back(); // Clean up failed element
                return read_res.error();
            }
        }

        // Check if the entire list payload was consumed
        if (!list_payload.empty()){
             // This implies an error during item decoding that wasn't caught?
             return DecodingError::kListLengthMismatch;
        }

        // Consume the list payload from the main view
        if (view_.length() < payload_len) return DecodingError::kInputTooShort; // Should not happen
        view_.remove_prefix(payload_len);

        return outcome::success(); // Success
    }

     // --- Convenience for Fixed-Size Arrays/Spans (Consume) ---
     // Reads next item into a fixed-size span/array
    template <size_t N>
    DecodingResult read(std::span<uint8_t, N> out_span) {
         BOOST_OUTCOME_TRY(auto h, peek_header()); // Peek first

         if (h.list) {
              return DecodingError::kUnexpectedList;
         }

         bool single_byte_literal = (h.payload_length == 1 && h.header_length == 0);

         if (h.payload_length != N) {
              // Allow decoding single-byte literal into span<byte, 1>
              if (!(N == 1 && single_byte_literal)) {
                  return rlp::DecodingError::kUnexpectedLength;
              }
         }

         // Now consume header (if any) and payload
         BOOST_OUTCOME_TRY(skip_header_internal()); // Consume header

         if (view_.length() < N) {
              return DecodingError::kInputTooShort; // Not enough payload data
         }

         std::memcpy(out_span.data(), view_.data(), N);
         view_.remove_prefix(N); // Consume payload

         return outcome::success();
    }

    template <size_t N>
    DecodingResult read(std::array<uint8_t, N>& out_array){
        return read<N>(std::span<uint8_t, N>{out_array});
    }
    template <size_t N>
    DecodingResult read(uint8_t (&out_c_array)[N]){
        return read<N>(std::span<uint8_t, N>{out_c_array});
    }


   private:
    ByteView view_{}; // The remaining data to be decoded

    // --- Internal Template Implementation for Integrals ---
    // Needs to be in header if read<T> is public template method
    template <typename T>
    auto read_integral(T& out) -> std::enable_if_t<is_unsigned_integral_v<T>, DecodingResult> {
        BOOST_OUTCOME_TRY(auto h, peek_header());
        
        if (h.list) {
            return DecodingError::kUnexpectedList;
        }
        
        // Check for leading zeros (except for zero value)
        if (h.payload_length > 1) {
            ByteView payload_view = view_.substr(h.header_length, h.payload_length);
            if (payload_view[0] == 0) {
                return DecodingError::kLeadingZero;
            }
        }
        
        // Check for overflow
        if (h.payload_length > sizeof(T)) {
            return DecodingError::kOverflow;
        }
        
        // Check for non-canonical single byte encoding
        if (h.payload_length == 1 && h.header_length > 0) {
            ByteView payload_view = view_.substr(h.header_length, h.payload_length);
            if (payload_view[0] < kRlpSingleByteThreshold) {
                return DecodingError::kNonCanonicalSize;
            }
        }
        
        // Check input length
        if (view_.length() < h.header_length + h.payload_length) {
            return DecodingError::kInputTooShort;
        }
        
        // Extract payload and decode
        ByteView payload_view = view_.substr(h.header_length, h.payload_length);
        BOOST_OUTCOME_TRY(endian::from_big_compact(payload_view, out));
        
        // Consume the item
        view_.remove_prefix(h.header_length + h.payload_length);
        
        return outcome::success();
    }
    
    DecodingResult read_bool(bool& out) {
        BOOST_OUTCOME_TRY(auto h, peek_header());
        
        if (h.list) {
            return DecodingError::kUnexpectedList;
        }
        
        // Check input length
        if (view_.length() < h.header_length + h.payload_length) {
            return DecodingError::kInputTooShort;
        }
        
        if (h.payload_length == 1) {
            ByteView payload_view = view_.substr(h.header_length, h.payload_length);
            if (payload_view[0] == 1) {
                out = true;
            } else if (payload_view[0] == 0) {
                out = false;
            } else {
                return DecodingError::kOverflow;
            }
        } else if (h.payload_length == 0 && h.header_length == 1 && view_[0] == kEmptyStringCode) {
            out = false;
        } else {
            return DecodingError::kOverflow;
        }
        
        // Consume the item
        view_.remove_prefix(h.header_length + h.payload_length);
        
        return outcome::success();
    }
    
    DecodingResult read_uint256(intx::uint256& out);

    // --- Internal Helpers (Declaration only) ---
    // Implementations will be in rlp_decoder.cpp
    Result<Header> decode_header_internal(ByteView& v) const noexcept;
    DecodingResult skip_header_internal() noexcept; // Consumes header from member view_
};


} // namespace rlp

#endif // RLP_DECODER_HPP
