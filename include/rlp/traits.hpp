#ifndef RLP_TRAITS_HPP
#define RLP_TRAITS_HPP

#include "types.hpp"
#include "intx.hpp"
#include <type_traits>

namespace rlp {

// Concept simulation for C++17
// Explicitly exclude bool from unsigned integral check
template <typename T>
inline constexpr bool is_unsigned_integral_v = std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<T, bool>;

// Type trait to check if a type is RLP-encodable
template <typename T>
struct is_rlp_encodable : std::disjunction<
    std::conjunction<std::is_integral<T>, std::is_unsigned<T>, std::negation<std::is_same<T, bool>>>,
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
    std::conjunction<std::is_integral<T>, std::is_unsigned<T>, std::negation<std::is_same<T, bool>>>,
    std::is_same<T, bool>,
    std::is_same<T, intx::uint256>,
    std::is_same<T, Bytes>
> {};

template <typename T>
inline constexpr bool is_rlp_decodable_v = is_rlp_decodable<T>::value;

} // namespace rlp

#endif // RLP_TRAITS_HPP
