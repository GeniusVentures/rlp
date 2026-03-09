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

    // Parse expiration (third element - big-endian uint32)
    std::array<uint8_t, sizeof(uint32_t)> expiration;
    BOOST_OUTCOME_TRY( decoder.read( expiration ) );
    pong.expiration  = static_cast<uint32_t>(expiration[0]) << 24U;
    pong.expiration |= static_cast<uint32_t>(expiration[1]) << 16U;
    pong.expiration |= static_cast<uint32_t>(expiration[2]) <<  8U;
    pong.expiration |= static_cast<uint32_t>(expiration[3]);

    // Verify we consumed all the data
    if ( !decoder.IsFinished() )
    {
        // Optional ENR sequence number (kEnrSeqSize-byte big-endian uint48)
        std::array<uint8_t, kEnrSeqSize> ersErqArray;
        BOOST_OUTCOME_TRY( decoder.read( ersErqArray ) );
        pong.ersErq  = static_cast<uint64_t>(ersErqArray[0]) << 40U;
        pong.ersErq |= static_cast<uint64_t>(ersErqArray[1]) << 32U;
        pong.ersErq |= static_cast<uint64_t>(ersErqArray[2]) << 24U;
        pong.ersErq |= static_cast<uint64_t>(ersErqArray[3]) << 16U;
        pong.ersErq |= static_cast<uint64_t>(ersErqArray[4]) <<  8U;
        pong.ersErq |= static_cast<uint64_t>(ersErqArray[5]);

        if ( !decoder.IsFinished() )
        {
            return rlp::DecodingError::kInputTooLong;
        }
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
    
    // Parse UDP port (big-endian uint16)
    std::array<uint8_t, sizeof(uint16_t)> port;
    BOOST_OUTCOME_TRY( decoder.read( port ) );
    endpoint.udpPort  = static_cast<uint16_t>(port[0]) << 8U;
    endpoint.udpPort |= static_cast<uint16_t>(port[1]);

    // Parse TCP port (big-endian uint16)
    BOOST_OUTCOME_TRY( decoder.read( port ) );
    endpoint.tcpPort  = static_cast<uint16_t>(port[0]) << 8U;
    endpoint.tcpPort |= static_cast<uint16_t>(port[1]);

    return rlp::outcome::success();
}
}
