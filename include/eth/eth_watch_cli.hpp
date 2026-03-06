// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <eth/abi_decoder.hpp>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eth::cli {

/// @brief One watch subscription specified on the command line.
struct WatchSpec
{
    std::string contract_hex;    ///< 40 hex chars (20 bytes), empty = any contract
    std::string event_signature; ///< e.g. "Transfer(address,address,uint256)"
};

/// @brief Parse a hex nibble character.  Returns nullopt for invalid chars.
[[nodiscard]] inline std::optional<uint8_t> hex_nibble(char c) noexcept
{
    if (c >= '0' && c <= '9') { return static_cast<uint8_t>(c - '0'); }
    if (c >= 'a' && c <= 'f') { return static_cast<uint8_t>(10 + (c - 'a')); }
    if (c >= 'A' && c <= 'F') { return static_cast<uint8_t>(10 + (c - 'A')); }
    return std::nullopt;
}

/// @brief Parse a hex string into a fixed-size byte array.
/// @param hex  Must be exactly N*2 hex characters (no 0x prefix).
template <size_t N>
[[nodiscard]] bool parse_hex_array(std::string_view hex, std::array<uint8_t, N>& out) noexcept
{
    if (hex.size() != N * 2) { return false; }
    for (size_t i = 0; i < N; ++i)
    {
        auto hi = hex_nibble(hex[i * 2]);
        auto lo = hex_nibble(hex[i * 2 + 1]);
        if (!hi || !lo) { return false; }
        out[i] = static_cast<uint8_t>((*hi << 4) | *lo);
    }
    return true;
}

/// @brief Parse a 0x-prefixed or bare 40-hex-char Ethereum address.
/// @return Parsed address or nullopt if the format is invalid.
[[nodiscard]] inline std::optional<codec::Address> parse_address(std::string_view hex) noexcept
{
    if (hex.starts_with("0x") || hex.starts_with("0X"))
    {
        hex = hex.substr(2);
    }
    codec::Address addr{};
    if (!parse_hex_array(hex, addr)) { return std::nullopt; }
    return addr;
}

/// @brief Return ABI parameter descriptors for well-known event signatures.
///
/// Supports:
///   - Transfer(address,address,uint256)
///   - Approval(address,address,uint256)
///
/// Unknown signatures return an empty list — the filter still matches on
/// topic[0] but no ABI decoding of parameters is performed.
[[nodiscard]] inline std::vector<abi::AbiParam> infer_params(const std::string& sig) noexcept
{
    if (sig == "Transfer(address,address,uint256)")
    {
        return {
            {abi::AbiParamKind::kAddress, true,  "from"},
            {abi::AbiParamKind::kAddress, true,  "to"},
            {abi::AbiParamKind::kUint,    false, "value"},
        };
    }
    if (sig == "Approval(address,address,uint256)")
    {
        return {
            {abi::AbiParamKind::kAddress, true,  "owner"},
            {abi::AbiParamKind::kAddress, true,  "spender"},
            {abi::AbiParamKind::kUint,    false, "value"},
        };
    }
    return {};
}

} // namespace eth::cli

