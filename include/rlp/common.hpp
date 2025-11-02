#ifndef RLP_COMMON_HPP
#define RLP_COMMON_HPP

// Main header that includes all RLP components
// This provides a single entry point for users

#include "types.hpp"
#include "constants.hpp"
#include "errors.hpp"
#include "result.hpp"
#include "traits.hpp"

namespace rlp {

// Utility function - implementation in common.cpp
std::string hexToString(ByteView bv) noexcept;

} // namespace rlp

#endif // RLP_COMMON_HPP
    