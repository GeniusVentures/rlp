// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <rlp/common.hpp>
#include <rlp/rlp_decoder.hpp>
#include <discv4/discv4_constants.hpp>

namespace discv4 {

/// @brief Parsed ENRResponse — discv4 wire type 0x06 (EIP-868).
///
/// Mirrors go-ethereum v4wire.ENRResponse:
/// @code
///   ENRResponse struct {
///       ReplyTok []byte   // Hash of the ENRRequest packet.
///       Record   enr.Record
///       Rest []rlp.RawValue `rlp:"tail"`
///   }
/// @endcode
///
/// Wire layout (incoming):
///   hash(32) || sig(65) || type(1) || RLP([reply_tok(32), record_rlp])
struct discv4_enr_response
{
    /// Hash of the originating ENRRequest packet (the ReplyTok field).
    std::array<uint8_t, kWireHashSize> request_hash{};

    /// Raw RLP bytes of the remote ENR record (EIP-778), including the RLP list header.
    /// Format: RLP([signature, seq, key0, val0, key1, val1, ...])
    std::vector<uint8_t> record_rlp{};

    /// @brief Parse a full discv4 wire packet into a discv4_enr_response.
    ///
    /// @param raw  Full wire bytes: hash(32) || sig(65) || type(1) || RLP_payload.
    /// @return Parsed response on success, decoding error on failure.
    [[nodiscard]] static rlp::Result<discv4_enr_response> Parse( rlp::ByteView raw ) noexcept;
};

} // namespace discv4

