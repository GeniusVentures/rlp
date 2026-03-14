// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

/// @file enr_response_test.cpp
/// @brief Unit tests for discv4_enr_response wire decode.
///
/// Mirrors go-ethereum v4wire_test.go coverage for ENRResponse.
/// Wire fixture is assembled manually (zeroed hash/sig) to stay independent of
/// the signing path — the same approach used by discv4_protocol_test.cpp for PONG.

#include <gtest/gtest.h>
#include "discv4/discv4_enr_response.hpp"
#include "discv4/discv4_constants.hpp"
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// @brief Build a minimal ENR record RLP: RLP([signature_64bytes, seq_uint64])
///        with no key-value pairs — the smallest valid EIP-778 record.
std::vector<uint8_t> make_minimal_enr_record( uint64_t seq = 0U )
{
    rlp::RlpEncoder enc;
    EXPECT_TRUE( enc.BeginList() );

    // Signature — 64 zero bytes (placeholder; Parse() does not verify it).
    const std::array<uint8_t, 64U> sig{};
    EXPECT_TRUE( enc.add( rlp::ByteView( sig.data(), sig.size() ) ) );

    // Sequence number.
    EXPECT_TRUE( enc.add( seq ) );

    EXPECT_TRUE( enc.EndList() );

    auto res = enc.MoveBytes();
    EXPECT_TRUE( res.has_value() );
    return std::vector<uint8_t>( res.value().begin(), res.value().end() );
}

/// @brief Build a full discv4 ENRResponse wire packet (zeroed hash + sig).
///        Layout: hash(32) || sig(65) || type(1) || RLP([reply_tok(32), record])
std::vector<uint8_t> make_enr_response_wire(
    const std::array<uint8_t, discv4::kWireHashSize>& reply_tok,
    const std::vector<uint8_t>&                       record_rlp )
{
    // Build RLP payload: [reply_tok, record]
    rlp::RlpEncoder enc;
    EXPECT_TRUE( enc.BeginList() );
    EXPECT_TRUE( enc.add( rlp::ByteView( reply_tok.data(), reply_tok.size() ) ) );
    // Embed the record as a raw already-encoded RLP item.
    EXPECT_TRUE( enc.AddRaw( rlp::ByteView( record_rlp.data(), record_rlp.size() ) ) );
    EXPECT_TRUE( enc.EndList() );

    auto rlp_res = enc.MoveBytes();
    EXPECT_TRUE( rlp_res.has_value() );

    // Assemble wire packet with zeroed hash and signature.
    std::vector<uint8_t> wire;
    wire.resize( discv4::kWireHashSize, 0U );                 // hash  (32)
    wire.resize( discv4::kWireHashSize + discv4::kWireSigSize, 0U ); // sig   (65)
    wire.push_back( discv4::kPacketTypeEnrResponse );          // type  (1)
    wire.insert( wire.end(),
                 rlp_res.value().begin(),
                 rlp_res.value().end() );                      // payload
    return wire;
}

} // namespace

// ---------------------------------------------------------------------------
// ENRResponse parse tests
// ---------------------------------------------------------------------------

/// @brief Parse() succeeds on a well-formed wire packet.
TEST( EnrResponseParseTest, ParseSucceeds )
{
    const std::array<uint8_t, discv4::kWireHashSize> reply_tok{};
    const auto record = make_minimal_enr_record();
    const auto wire   = make_enr_response_wire( reply_tok, record );

    const rlp::ByteView bv( wire.data(), wire.size() );
    auto result = discv4::discv4_enr_response::Parse( bv );

    ASSERT_TRUE( result.has_value() ) << "Parse() must succeed on a valid ENRResponse wire packet";
}

/// @brief request_hash round-trips through encode → parse.
TEST( EnrResponseParseTest, RequestHashRoundTrips )
{
    std::array<uint8_t, discv4::kWireHashSize> reply_tok{};
    // Fill with a recognisable pattern.
    for ( size_t i = 0U; i < reply_tok.size(); ++i )
    {
        reply_tok[i] = static_cast<uint8_t>( i + 1U );
    }

    const auto record = make_minimal_enr_record();
    const auto wire   = make_enr_response_wire( reply_tok, record );

    const rlp::ByteView bv( wire.data(), wire.size() );
    auto result = discv4::discv4_enr_response::Parse( bv );

    ASSERT_TRUE( result.has_value() );
    EXPECT_EQ( result.value().request_hash, reply_tok );
}

/// @brief record_rlp contains the expected raw bytes of the ENR record.
TEST( EnrResponseParseTest, RecordRlpRoundTrips )
{
    const std::array<uint8_t, discv4::kWireHashSize> reply_tok{};
    const auto record = make_minimal_enr_record( 42U );
    const auto wire   = make_enr_response_wire( reply_tok, record );

    const rlp::ByteView bv( wire.data(), wire.size() );
    auto result = discv4::discv4_enr_response::Parse( bv );

    ASSERT_TRUE( result.has_value() );
    EXPECT_EQ( result.value().record_rlp, record );
}

/// @brief Parse() rejects a packet whose type byte is not 0x06.
TEST( EnrResponseParseTest, WrongPacketTypeIsRejected )
{
    const std::array<uint8_t, discv4::kWireHashSize> reply_tok{};
    const auto record = make_minimal_enr_record();
    auto wire         = make_enr_response_wire( reply_tok, record );

    // Corrupt the packet-type byte.
    wire[discv4::kWirePacketTypeOffset] = discv4::kPacketTypePong;

    const rlp::ByteView bv( wire.data(), wire.size() );
    auto result = discv4::discv4_enr_response::Parse( bv );

    EXPECT_FALSE( result.has_value() ) << "Parse() must reject wrong packet type";
}

/// @brief Parse() rejects a buffer that is too short to contain a wire header.
TEST( EnrResponseParseTest, TooShortInputIsRejected )
{
    // Anything shorter than kWireHeaderSize + 1 must fail.
    const std::vector<uint8_t> too_short( discv4::kWireHeaderSize - 1U, 0U );
    const rlp::ByteView bv( too_short.data(), too_short.size() );

    auto result = discv4::discv4_enr_response::Parse( bv );

    EXPECT_FALSE( result.has_value() ) << "Parse() must reject truncated input";
}

