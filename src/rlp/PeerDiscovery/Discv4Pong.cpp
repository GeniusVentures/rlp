// discv4_pong.cpp
#include <rlp/PeerDiscovery/Discv4Pong.hpp>
#include <iostream>

inline constexpr uint8_t kHashSize{32};
inline constexpr uint8_t kSignSize{65};
inline constexpr uint8_t kHeaderSize{kHashSize+kSignSize};

inline constexpr uint8_t kPacketTypePong{2};

namespace discv4 {

rlp::Result<Discv4Pong> Discv4Pong::parse(rlp::ByteView raw) {
    // Skipping hash (32 bytes), sign (65 bytes), packet type (1 byte)
    if (raw.size() <= kHeaderSize)
    {
        return rlp::DecodingError::kUnexpectedString;
    }

    if (raw.at(kHeaderSize) != kPacketTypePong)
    {
        return rlp::DecodingError::kUnexpectedString;
    }

    // Ommitting header and packet type byte as it is not RLP data
    rlp::ByteView pong_payload(raw.data() + kHeaderSize + 1, raw.size() - kHeaderSize - 1);
    rlp::RlpDecoder decoder(pong_payload);

    Discv4Pong pong;
    
    // Check if the packet is a list (PONG should be a list with 3 elements)
    BOOST_OUTCOME_TRY( bool is_list, decoder.is_list() );
    if ( !is_list )
    {
        return rlp::DecodingError::kUnexpectedString;
    }

    // Read the list header
    BOOST_OUTCOME_TRY( size_t list_length, decoder.read_list_header_bytes() );
    
    // Parse to_endpoint (first element - should be a list of 3 elements)
    BOOST_OUTCOME_TRY( parse_endpoint( decoder, pong.toEndpoint ) );
    
    // Parse ping_hash (second element - 32 bytes)
    BOOST_OUTCOME_TRY( decoder.read( pong.pingHash ) );
    rlp::ByteView hashBv( pong.pingHash.data(), pong.pingHash.size() );

    // Parse expiration (third element - uint32)
    // TODO Fix decoder.read function
    std::array<uint8_t, 4> expiration;
    BOOST_OUTCOME_TRY( decoder.read( expiration ) );
    rlp::ByteView hexpBv( expiration.data(), expiration.size() );
    pong.expiration = 0;
    pong.expiration |= expiration[0] << 24;
    pong.expiration |= expiration[1] << 16;
    pong.expiration |= expiration[2] << 8;
    pong.expiration |= expiration[3] << 0;
    
    // Verify we consumed all the data
    if ( !decoder.is_finished() ) 
    {
        // Parse expiration (forth optional element - uint32)
        // TODO Fix decoder.read function
        std::array<uint8_t, 6> ersErqArray;
        BOOST_OUTCOME_TRY( decoder.read( ersErqArray ) );
        pong.ersErq |= (uint64_t)ersErqArray[0] << 40;
        pong.ersErq |= (uint64_t)ersErqArray[1] << 32;
        pong.ersErq |= ersErqArray[2] << 24;
        pong.ersErq |= ersErqArray[3] << 16;
        pong.ersErq |= ersErqArray[4] << 8;
        pong.ersErq |= ersErqArray[5] << 0;

        if ( !decoder.is_finished() ) 
        {
            return rlp::DecodingError::kInputTooLong;
        }
    }

    return pong;
}

rlp::DecodingResult Discv4Pong::parse_endpoint( rlp::RlpDecoder& decoder, Discv4Pong::Endpoint& endpoint )
{
    // Check if next item is a list (endpoint should be [ip, udp_port, tcp_port])
    BOOST_OUTCOME_TRY( bool is_list, decoder.is_list() );
    if ( !is_list ) 
    {
        return rlp::DecodingError::kUnexpectedString;
    }
    
    // Read endpoint list header
    BOOST_OUTCOME_TRY( size_t endpointListLength, decoder.read_list_header_bytes() );
    
    // Parse IP address (4 bytes)
    BOOST_OUTCOME_TRY( decoder.read( endpoint.ip ) );
    
    // Parse UDP port
    std::array<uint8_t, 2> port;
    BOOST_OUTCOME_TRY( decoder.read( port ) );
    endpoint.udpPort = port[0] << 8;
    endpoint.udpPort |= port[1];
    
    // Parse TCP port  
    BOOST_OUTCOME_TRY( decoder.read( port ) );
    endpoint.tcpPort = port[0] << 8;
    endpoint.tcpPort |= port[1];
    
    return rlp::outcome::success();
}
}
