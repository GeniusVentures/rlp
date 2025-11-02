#ifndef RLP_TYPES_HPP
#define RLP_TYPES_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace rlp {

// Basic byte container types
using Bytes = std::basic_string<uint8_t>;
using ByteView = std::basic_string_view<uint8_t>;

} // namespace rlp

#endif // RLP_TYPES_HPP
