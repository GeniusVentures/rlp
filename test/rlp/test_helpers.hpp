/**
 * @file test_helpers.hpp
 * @brief Common test helper functions for RLP test suites
 * 
 * This file provides utility functions for converting between bytes and hex strings,
 * which are used across multiple test files.
 */

#ifndef RLP_TEST_HELPERS_HPP
#define RLP_TEST_HELPERS_HPP

#include "common.hpp"
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace rlp {
namespace test {

/**
 * @brief Convert a hex string to a Bytes object
 * @param hex Hexadecimal string (e.g., "48656c6c6f")
 * @return Bytes object containing the decoded bytes
 * @throws std::invalid_argument if hex string has odd length
 */
inline Bytes hex_to_bytes(std::string_view hex) {
    Bytes result;
    if (hex.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have even length");
    }
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_string = std::string(hex.substr(i, 2));
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_string, nullptr, 16));
        result.push_back(byte);
    }
    return result;
}

/**
 * @brief Convert a Bytes object to a hex string
 * @param bytes Bytes object to convert
 * @return Lowercase hexadecimal string representation (e.g., "48656c6c6f")
 */
inline std::string bytes_to_hex(const Bytes& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : bytes) {
        ss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return ss.str();
}

/**
 * @brief Convert a ByteView to a hex string
 * @param bytes ByteView object to convert
 * @return Lowercase hexadecimal string representation (e.g., "48656c6c6f")
 */
inline std::string bytes_to_hex(ByteView bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : bytes) {
        ss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return ss.str();
}

// Aliases for convenience
inline std::string to_hex(ByteView bytes) { return bytes_to_hex(bytes); }
inline std::string to_hex(const Bytes& bytes) { return bytes_to_hex(bytes); }
inline Bytes from_hex(std::string_view hex) { return hex_to_bytes(hex); }

} // namespace test
} // namespace rlp

#endif // RLP_TEST_HELPERS_HPP
