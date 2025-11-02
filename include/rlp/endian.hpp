#ifndef RLP_ENDIAN_HPP
#define RLP_ENDIAN_HPP

#include "intx.hpp"
#include "common.hpp" // Use direct include
#include <type_traits> // Include for enable_if

namespace rlp::endian {

    // Function to convert an unsigned integral type to its big-endian compact byte representation.
    // SFINAE moved to return type (or a dummy template parameter if return type is complex)
    // Using trailing return type for cleaner SFINAE with std::enable_if_t
    template <typename T>
    auto to_big_compact(const T& n) noexcept -> std::enable_if_t<is_unsigned_integral_v<T>, Bytes>;

    // Function to convert a big-endian compact byte representation back to an unsigned integral type.
    template <typename T>
    auto from_big_compact(ByteView bytes, T& out) noexcept -> std::enable_if_t<is_unsigned_integral_v<T>, DecodingResult>;

    // Overload for uint256 using intx (no SFINAE needed here)
    Bytes to_big_compact(const intx::uint256& n) noexcept;
    DecodingResult from_big_compact(ByteView bytes, intx::uint256& out) noexcept;

} // namespace rlp::endian

#endif // RLP_ENDIAN_HPP
