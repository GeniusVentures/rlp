// packet_factory.cpp
#include <rlp/PeerDiscovery/packet_factory.hpp>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <iostream>
#include <boost/outcome/try.hpp>

#include <rlp/PeerDiscovery/Discv4Ping.hpp>
#include <rlp/PeerDiscovery/Discv4Pong.hpp>

namespace discv4 {

PacketResult PacketFactory::SendPingAndWait(
    asio::io_context& io,
    const std::string& fromIp, uint16_t fUdp, uint16_t fTcp,
    const std::string& toIp, uint16_t tUdp, uint16_t tTcp,
    const std::vector<uint8_t>& privKeyHex,
    SendCallback callback )
{

    auto ping = std::make_unique<Discv4Ping>( fromIp, fUdp, fTcp, toIp, tUdp, tTcp );

    std::vector<uint8_t> msg;
    auto signResult = SignAndBuildPacket( ping.get(), privKeyHex, msg );
    if ( !signResult )
    {
        return outcome::failure( signResult.error() );
    }

    // Create socket
    udp::socket socket( io, udp::v4() );
    socket.set_option( udp::socket::reuse_address( true ) );
    socket.bind( udp::endpoint( boost::asio::ip::address_v4::any(), 53093 ) );

    // Send
    udp::endpoint target( boost::asio::ip::address_v4::from_string( toIp ), tUdp );
    udp::endpoint sender;
    SendPacket( socket, msg, target );

    rlp::ByteView msbBv( msg.data(), msg.size() );
    std::cout << "Sending PING: " << rlp::hexToString( msbBv ) << "\n\n";

    // Receive async
    std::array<uint8_t, 2048> arrayBuffer;
    boost::system::error_code ec;
    size_t bytesTransferred = socket.receive_from( boost::asio::buffer( arrayBuffer ), sender, 0, ec );

    if ( !ec )
    {
        std::cout << "Received " << bytesTransferred << " bytes\n\n";
        std::vector<uint8_t> data( arrayBuffer.data(), arrayBuffer.data() + bytesTransferred );
        callback( data, sender );
    }

    // Run io_context (in a loop)
    io.run();

    return outcome::success();
}

PacketResult PacketFactory::SignAndBuildPacket(
    Discv4Packet* packet,
    const std::vector<uint8_t>& privKeyHex,
    std::vector<uint8_t>& out )
{

    if ( packet == nullptr )
    {
        return outcome::failure( PacketError::kNullPacket );
    }

    auto payload = packet->RlpPayload();

    // Hash with keccak-256
    std::array<uint8_t, 32> hash = Discv4Packet::Keccak256( payload );

    // Sign with secp256k1
    auto ctx = secp256k1_context_create( SECP256K1_CONTEXT_SIGN );
    secp256k1_ecdsa_recoverable_signature sig;
    int success = secp256k1_ecdsa_sign_recoverable( ctx, &sig, hash.data(), privKeyHex.data(),
                                                  secp256k1_nonce_function_rfc6979, nullptr );


    if ( !success )
    {
        secp256k1_context_destroy( ctx );
        return outcome::failure( PacketError::kSignFailure );
    }

    uint8_t serialized[65];
    int recid;
    secp256k1_ecdsa_recoverable_signature_serialize_compact( ctx, serialized, &recid, &sig );
    secp256k1_context_destroy( ctx );

    out.reserve( 32 + 65 + payload.size() );
    out.insert( out.end(), 32, 0 ); // Skip for hash for whole out message
    out.insert( out.end(), serialized, serialized + 64 );
    out.push_back( recid );
    out.insert( out.end(), payload.begin(), payload.end() );

    // Hash for whole out message
    auto payloadHash = nil::crypto3::hash<nil::crypto3::hashes::keccak_1600<256>>( out.begin() + 32, out.end() );
    std::array<uint8_t, 32> payloadArray = payloadHash;
    std::copy( payloadArray.begin(), payloadArray.end(), out.begin() );

    return outcome::success();
}

void PacketFactory::SendPacket(
    asio::ip::udp::socket& socket,
    const std::vector<uint8_t>& msg,
    const udp::endpoint& target )
{
    socket.send_to( boost::asio::buffer( msg ), target );
}

} // namespace discv4
