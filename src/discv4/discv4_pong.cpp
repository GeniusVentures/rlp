// discv4_pong.cpp
#include "discv4/discv4_pong.hpp"
#include "discv4/discv4_constants.hpp"

namespace discv4 {

rlp::Result<discv4_pong> discv4_pong::Parse(rlp::ByteView raw) {
    // Skipping hash (kWireHashSize bytes), sig (kWireSigSize bytes), packet type (kWirePacketTypeSize byte)
    if ( raw.size() <= kWireHeaderSize - kWirePacketTypeSize )
    {
        return rlp::DecodingError::kUnexpectedString;
    }

    if ( raw.at(kWirePacketTypeOffset) != kPacketTypePong )
    {
        return rlp::DecodingError::kUnexpectedString;
    }

    // Omitting header and packet type byte as it is not RLP data
    rlp::ByteView pong_payload(raw.data() + kWireHeaderSize, raw.size() - kWireHeaderSize);
    rlp::RlpDecoder decoder(pong_payload);

    discv4_pong pong;
    
    // Check if the packet is a list (PONG should be a list with 3 elements)
    BOOST_OUTCOME_TRY( bool is_list, decoder.IsList() );
    if ( !is_list )
    {
        return rlp::DecodingError::kUnexpectedString;
    }

    // Read the list header
    BOOST_OUTCOME_TRY( size_t list_length, decoder.ReadListHeaderBytes() );
    
    // Parse to_endpoint (first element - should be a list of 3 elements)
    BOOST_OUTCOME_TRY( ParseEndpoint( decoder, pong.toEndpoint ) );
    
    // Parse ping_hash (second element - 32 bytes)
    BOOST_OUTCOME_TRY( decoder.read( pong.pingHash ) );
    rlp::ByteView hashBv( pong.pingHash.data(), pong.pingHash.size() );

    // Parse expiration — variable-length RLP uint64 (go-ethereum encodes as uint64)
    uint64_t expiration = 0;
    BOOST_OUTCOME_TRY( decoder.read( expiration ) );
    pong.expiration = static_cast<uint32_t>(expiration);

    // Optional ENR sequence number (EIP-868) — variable-length uint64, may be absent.
    // Any further unknown fields are silently consumed for forward compatibility
    // (mirrors go-ethereum's Rest []rlp.RawValue `rlp:"tail"`).
    if ( !decoder.IsFinished() )
    {
        BOOST_OUTCOME_TRY( decoder.read( pong.ersErq ) );
    }
    while ( !decoder.IsFinished() )
    {
        BOOST_OUTCOME_TRY( decoder.SkipItem() );
    }

    return pong;
}

rlp::DecodingResult discv4_pong::ParseEndpoint( rlp::RlpDecoder& decoder, discv4_pong::Endpoint& endpoint )
{
    // Check if next item is a list (endpoint should be [ip, udp_port, tcp_port])
    BOOST_OUTCOME_TRY( bool is_list, decoder.IsList() );
    if ( !is_list ) 
    {
        return rlp::DecodingError::kUnexpectedString;
    }
    
    // Read endpoint list header
    BOOST_OUTCOME_TRY( size_t endpointListLength, decoder.ReadListHeaderBytes() );
    
    // Parse IP address (4 bytes)
    BOOST_OUTCOME_TRY( decoder.read( endpoint.ip ) );
    
    // Parse UDP port — variable-length RLP uint16
    BOOST_OUTCOME_TRY( decoder.read( endpoint.udpPort ) );

    // Parse TCP port — variable-length RLP uint16
    BOOST_OUTCOME_TRY( decoder.read( endpoint.tcpPort ) );

    return rlp::outcome::success();
}
}
