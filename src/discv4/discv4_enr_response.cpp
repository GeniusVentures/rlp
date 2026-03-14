// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include "discv4/discv4_enr_response.hpp"
#include "discv4/discv4_constants.hpp"

namespace discv4 {

rlp::Result<discv4_enr_response> discv4_enr_response::Parse( rlp::ByteView raw ) noexcept
{
    // Minimum: hash(32) + sig(65) + type(1) = kWireHeaderSize bytes before the payload.
    if ( raw.size() <= kWireHeaderSize )
    {
        return rlp::DecodingError::kInputTooShort;
    }

    if ( raw[kWirePacketTypeOffset] != kPacketTypeEnrResponse )
    {
        return rlp::DecodingError::kUnexpectedString;
    }

    // Payload starts after the wire header.
    rlp::ByteView payload( raw.data() + kWireHeaderSize, raw.size() - kWireHeaderSize );
    rlp::RlpDecoder decoder( payload );

    // Outer list: [reply_tok, record]
    BOOST_OUTCOME_TRY( bool is_list, decoder.IsList() );
    if ( !is_list )
    {
        return rlp::DecodingError::kUnexpectedString;
    }
    BOOST_OUTCOME_TRY( size_t outer_len, decoder.ReadListHeaderBytes() );
    (void)outer_len;

    discv4_enr_response response;

    // ReplyTok — 32-byte hash of the originating ENRRequest packet.
    BOOST_OUTCOME_TRY( decoder.read( response.request_hash ) );

    // Record — capture raw RLP bytes (header + payload) for later parsing.
    BOOST_OUTCOME_TRY( auto record_header, decoder.PeekHeader() );
    const size_t record_total = record_header.header_size_bytes + record_header.payload_size_bytes;

    if ( decoder.Remaining().size() < record_total )
    {
        return rlp::DecodingError::kInputTooShort;
    }

    response.record_rlp.assign(
        decoder.Remaining().data(),
        decoder.Remaining().data() + record_total );

    // Consume the record item; ignore any trailing `Rest` fields (forward compatibility).
    BOOST_OUTCOME_TRY( decoder.SkipItem() );

    return response;
}

} // namespace discv4

