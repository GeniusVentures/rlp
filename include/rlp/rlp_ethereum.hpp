#ifndef RLP_ETHEREUM_HPP
#define RLP_ETHEREUM_HPP

#include "common.hpp"
#include "rlp_encoder.hpp"
#include "rlp_decoder.hpp"
#include <array>

namespace rlp {

// ============================================================================
// Ethereum-Specific Type Aliases
// ============================================================================

using Address = std::array<uint8_t, 20>;      // Ethereum address (20 bytes)
using Hash256 = std::array<uint8_t, 32>;      // Keccak-256 hash (32 bytes)
using Signature = std::array<uint8_t, 65>;    // ECDSA signature (65 bytes: r, s, v)
using Bloom = std::array<uint8_t, 256>;       // Bloom filter (256 bytes)

// ============================================================================
// Free Functions for Ethereum Types - Encoder
// ============================================================================

// Encode Ethereum address (20 bytes)
inline EncodingOperationResult addAddress(RlpEncoder& encoder, const Address& addr) {
    return encoder.add(ByteView(addr.data(), addr.size()));
}

// Encode Ethereum hash (32 bytes)
inline EncodingOperationResult addHash(RlpEncoder& encoder, const Hash256& hash) {
    return encoder.add(ByteView(hash.data(), hash.size()));
}

// Encode Ethereum signature (65 bytes)
inline EncodingOperationResult addSignature(RlpEncoder& encoder, const Signature& sig) {
    return encoder.add(ByteView(sig.data(), sig.size()));
}

// Encode Ethereum bloom filter (256 bytes)
inline EncodingOperationResult addBloom(RlpEncoder& encoder, const Bloom& bloom) {
    return encoder.add(ByteView(bloom.data(), bloom.size()));
}

// ============================================================================
// Free Functions for Ethereum Types - Decoder
// ============================================================================

// Read Ethereum address (20 bytes)
inline DecodingResult readAddress(RlpDecoder& decoder, Address& addr) {
    return decoder.read(addr);
}

// Read Ethereum hash (32 bytes)
inline DecodingResult readHash(RlpDecoder& decoder, Hash256& hash) {
    return decoder.read(hash);
}

// Read Ethereum signature (65 bytes)
inline DecodingResult readSignature(RlpDecoder& decoder, Signature& sig) {
    return decoder.read(sig);
}

// Read Ethereum bloom filter (256 bytes)
inline DecodingResult readBloom(RlpDecoder& decoder, Bloom& bloom) {
    return decoder.read(bloom);
}

} // namespace rlp

#endif // RLP_ETHEREUM_HPP
