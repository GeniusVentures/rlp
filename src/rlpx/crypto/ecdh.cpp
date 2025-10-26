// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/crypto/ecdh.hpp>
#include <secp256k1.h>
#include <secp256k1_ecdh.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <cstring>

namespace rlpx::crypto {

namespace {
    // Global secp256k1 context (thread-safe, read-only after creation)
    secp256k1_context* get_context() {
        static secp256k1_context* ctx = secp256k1_context_create(
            SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY
        );
        return ctx;
    }
}

CryptoResult<SharedSecret> Ecdh::compute_shared_secret(
    gsl::span<const uint8_t, kPublicKeySize> public_key,
    gsl::span<const uint8_t, kPrivateKeySize> private_key
) noexcept {
    auto* ctx = get_context();
    if ( !ctx ) {
        return CryptoError::kSecp256k1Error;
    }

    // Verify private key is valid
    if ( !secp256k1_ec_seckey_verify(ctx, private_key.data()) ) {
        return CryptoError::kInvalidPrivateKey;
    }

    // Parse public key (uncompressed format: 0x04 + 64 bytes)
    secp256k1_pubkey parsed_pubkey;
    
    // Add 0x04 prefix for uncompressed format
    std::array<uint8_t, 65> pubkey_with_prefix;
    pubkey_with_prefix[0] = 0x04;
    std::memcpy(pubkey_with_prefix.data() + 1, public_key.data(), kPublicKeySize);
    
    if ( !secp256k1_ec_pubkey_parse(ctx, &parsed_pubkey, pubkey_with_prefix.data(), 65) ) {
        return CryptoError::kInvalidPublicKey;
    }

    // Compute ECDH shared point
    SharedSecret shared_secret;
    
    // Custom hash function that returns the x-coordinate directly
    auto ecdh_hash_function = [](
        unsigned char* output,
        const unsigned char* x32,
        const unsigned char* y32,
        void* data
    ) -> int {
        (void)y32;  // Unused
        (void)data; // Unused
        // Return x-coordinate as shared secret
        std::memcpy(output, x32, 32);
        return 1;
    };

    if ( !secp256k1_ecdh(
            ctx,
            shared_secret.data(),
            &parsed_pubkey,
            private_key.data(),
            ecdh_hash_function,
            nullptr
        ) ) {
        return CryptoError::kEcdhFailed;
    }

    return shared_secret;
}

CryptoResult<Ecdh::KeyPair> Ecdh::generate_ephemeral_keypair() noexcept {
    auto* ctx = get_context();
    if ( !ctx ) {
        return CryptoError::kSecp256k1Error;
    }

    KeyPair keypair;

    // Generate random private key
    while ( true ) {
        if ( RAND_bytes(keypair.private_key.data(), kPrivateKeySize) != 1 ) {
            return CryptoError::kOpenSslError;
        }

        // Verify it's a valid private key
        if ( secp256k1_ec_seckey_verify(ctx, keypair.private_key.data()) ) {
            break;
        }
        // If invalid, try again (very rare)
    }

    // Compute corresponding public key
    secp256k1_pubkey pubkey;
    if ( !secp256k1_ec_pubkey_create(ctx, &pubkey, keypair.private_key.data()) ) {
        return CryptoError::kSecp256k1Error;
    }

    // Serialize public key in uncompressed format
    std::array<uint8_t, 65> serialized;
    size_t output_len = 65;
    if ( !secp256k1_ec_pubkey_serialize(
            ctx,
            serialized.data(),
            &output_len,
            &pubkey,
            SECP256K1_EC_UNCOMPRESSED
        ) ) {
        return CryptoError::kSecp256k1Error;
    }

    // Copy public key (skip 0x04 prefix)
    std::memcpy(keypair.public_key.data(), serialized.data() + 1, kPublicKeySize);

    return keypair;
}

bool Ecdh::verify_public_key(gsl::span<const uint8_t, kPublicKeySize> public_key) noexcept {
    auto* ctx = get_context();
    if ( !ctx ) {
        return false;
    }

    // Add 0x04 prefix for uncompressed format
    std::array<uint8_t, 65> pubkey_with_prefix;
    pubkey_with_prefix[0] = 0x04;
    std::memcpy(pubkey_with_prefix.data() + 1, public_key.data(), kPublicKeySize);

    secp256k1_pubkey parsed_pubkey;
    return secp256k1_ec_pubkey_parse(ctx, &parsed_pubkey, pubkey_with_prefix.data(), 65) == 1;
}

} // namespace rlpx::crypto
