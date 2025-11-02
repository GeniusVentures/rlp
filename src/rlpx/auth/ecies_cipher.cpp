// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/auth/ecies_cipher.hpp>
#include <rlpx/crypto/ecdh.hpp>
#include <secp256k1.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <cstring>

namespace rlpx::auth {

namespace {

// Generate random IV for AES
std::array<uint8_t, kAesBlockSize> generate_iv() noexcept {
    std::array<uint8_t, kAesBlockSize> iv{};
    RAND_bytes(iv.data(), static_cast<int>(iv.size()));
    return iv;
}

// AES-256-CTR encryption
ByteBuffer aes_encrypt(ByteView plaintext, ByteView key, ByteView iv) noexcept {
    ByteBuffer ciphertext(plaintext.size());
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if ( !ctx ) return {};
    
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, key.data(), iv.data());
    
    int len = 0;
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), 
                     static_cast<int>(plaintext.size()));
    
    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(static_cast<size_t>(len));
    return ciphertext;
}

// AES-256-CTR decryption (same as encryption for CTR mode)
ByteBuffer aes_decrypt(ByteView ciphertext, ByteView key, ByteView iv) noexcept {
    return aes_encrypt(ciphertext, key, iv);
}

// HMAC-SHA256
std::array<uint8_t, 32> hmac_sha256(ByteView key, ByteView data) noexcept {
    std::array<uint8_t, 32> mac{};
    unsigned int mac_len = 0;
    
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         data.data(), data.size(), mac.data(), &mac_len);
    
    return mac;
}

} // anonymous namespace

AuthResult<ByteBuffer> EciesCipher::encrypt(const EciesEncryptParams& params) noexcept {
    // Generate ephemeral keypair
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    if ( !ctx ) {
        return AuthError::kEciesEncryptFailed;
    }
        
        PrivateKey ephemeral_private_key{};
        RAND_bytes(ephemeral_private_key.data(), static_cast<int>(ephemeral_private_key.size()));
        
        secp256k1_pubkey ephemeral_public_key;
        if ( !secp256k1_ec_pubkey_create(ctx, &ephemeral_public_key, ephemeral_private_key.data()) ) {
            secp256k1_context_destroy(ctx);
            return AuthError::kInvalidPublicKey;
        }
        
        // Compute shared secret
        auto shared_result = compute_shared_secret(params.recipient_public_key, 
                                                   ephemeral_private_key);
        if ( !shared_result ) {
            secp256k1_context_destroy(ctx);
            return shared_result.error();
        }
        
        SharedSecret shared_secret = shared_result.value();
        
        // Derive encryption keys
        AesKey aes_key = derive_aes_key(shared_secret);
        MacKey mac_key = derive_mac_key(shared_secret);
        
        // Generate IV and encrypt
        auto iv = generate_iv();
        ByteBuffer ciphertext = aes_encrypt(params.plaintext, aes_key, iv);
        
        // Serialize ephemeral public key
        PublicKey ephemeral_pub_serialized{};
        size_t pub_len = ephemeral_pub_serialized.size();
        secp256k1_ec_pubkey_serialize(ctx, ephemeral_pub_serialized.data(), &pub_len,
                                     &ephemeral_public_key, SECP256K1_EC_UNCOMPRESSED);
        
        // Compute MAC over (IV || ciphertext || shared_mac_data)
        ByteBuffer mac_input;
        mac_input.reserve(iv.size() + ciphertext.size() + params.shared_mac_data.size());
        mac_input.insert(mac_input.end(), iv.begin(), iv.end());
        mac_input.insert(mac_input.end(), ciphertext.begin(), ciphertext.end());
        mac_input.insert(mac_input.end(), params.shared_mac_data.begin(), params.shared_mac_data.end());
        
        auto mac = hmac_sha256(mac_key, mac_input);
        
        // Pack result: [ephemeral_public_key || iv || ciphertext || mac]
        ByteBuffer result;
        result.reserve(ephemeral_pub_serialized.size() + iv.size() + ciphertext.size() + mac.size());
        result.insert(result.end(), ephemeral_pub_serialized.begin(), ephemeral_pub_serialized.end());
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        result.insert(result.end(), mac.begin(), mac.end());
        
        secp256k1_context_destroy(ctx);
        return result;
}

AuthResult<ByteBuffer> EciesCipher::decrypt(const EciesDecryptParams& params) noexcept {
    // Parse message: [public_key(65) || iv(16) || ciphertext || mac(32)]
    constexpr size_t kMinSize = 65 + 16 + 32;
    if ( params.ciphertext.size() < kMinSize ) {
        return AuthError::kEciesDecryptFailed;
    }
        
        ByteView ephemeral_public_key_data = params.ciphertext.subspan(0, 65);
        ByteView iv = params.ciphertext.subspan(65, 16);
        size_t ciphertext_len = params.ciphertext.size() - kMinSize;
        ByteView ciphertext = params.ciphertext.subspan(81, ciphertext_len);
        ByteView mac = params.ciphertext.subspan(81 + ciphertext_len, 32);
        
        // Parse ephemeral public key
        secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
        if ( !ctx ) {
            return AuthError::kEciesDecryptFailed;
        }
        
        secp256k1_pubkey ephemeral_public_key;
        if ( !secp256k1_ec_pubkey_parse(ctx, &ephemeral_public_key, 
                                       ephemeral_public_key_data.data(), 
                                       ephemeral_public_key_data.size()) ) {
            secp256k1_context_destroy(ctx);
            return AuthError::kInvalidPublicKey;
        }
        
        // Compute shared secret
        PublicKey ephemeral_pub_key{};
        std::memcpy(ephemeral_pub_key.data(), ephemeral_public_key_data.data(), 64);
        
        auto shared_result = compute_shared_secret(ephemeral_pub_key, 
                                                   params.recipient_private_key);
        if ( !shared_result ) {
            secp256k1_context_destroy(ctx);
            return shared_result.error();
        }
        
        SharedSecret shared_secret = shared_result.value();
        
        // Derive encryption keys
        AesKey aes_key = derive_aes_key(shared_secret);
        MacKey mac_key = derive_mac_key(shared_secret);
        
        // Verify MAC
        ByteBuffer mac_input;
        mac_input.reserve(iv.size() + ciphertext.size() + params.shared_mac_data.size());
        mac_input.insert(mac_input.end(), iv.begin(), iv.end());
        mac_input.insert(mac_input.end(), ciphertext.begin(), ciphertext.end());
        mac_input.insert(mac_input.end(), params.shared_mac_data.begin(), params.shared_mac_data.end());
        
        auto expected_mac = hmac_sha256(mac_key, mac_input);
        if ( std::memcmp(mac.data(), expected_mac.data(), 32) != 0 ) {
            secp256k1_context_destroy(ctx);
            return AuthError::kEciesDecryptFailed;
        }
        
        // Decrypt
        ByteBuffer plaintext = aes_decrypt(ciphertext, aes_key, iv);
        
        secp256k1_context_destroy(ctx);
        return plaintext;
}

size_t EciesCipher::estimate_encrypted_size(size_t plaintext_size) noexcept {
    return 65 + 16 + plaintext_size + 32; // pubkey + iv + ciphertext + mac
}

AuthResult<SharedSecret> EciesCipher::compute_shared_secret(
    gsl::span<const uint8_t, kPublicKeySize> public_key,
    gsl::span<const uint8_t, kPrivateKeySize> private_key
) noexcept {
    // Use the existing ECDH implementation
    auto result = rlpx::crypto::Ecdh::compute_shared_secret(public_key, private_key);
    if (!result) {
        // Convert CryptoError to AuthError
        return AuthError::kSharedSecretFailed;
    }
    return result.value();
}

AesKey EciesCipher::derive_aes_key(ByteView shared_secret) noexcept {
    // NIST SP 800-56 Concatenation KDF for AES key
    AesKey key{};
    
    // Simple KDF: SHA256(0x00000001 || shared_secret)
    ByteBuffer input;
    input.reserve(4 + shared_secret.size());
    input.push_back(0x00);
    input.push_back(0x00);
    input.push_back(0x00);
    input.push_back(0x01);
    input.insert(input.end(), shared_secret.begin(), shared_secret.end());
    
    unsigned char hash[32];
    EVP_Digest(input.data(), input.size(), hash, nullptr, EVP_sha256(), nullptr);
    
    std::memcpy(key.data(), hash, kAesKeySize);
    return key;
}

MacKey EciesCipher::derive_mac_key(ByteView shared_secret) noexcept {
    // NIST SP 800-56 Concatenation KDF for MAC key
    MacKey key{};
    
    // Simple KDF: SHA256(0x00000002 || shared_secret)
    ByteBuffer input;
    input.reserve(4 + shared_secret.size());
    input.push_back(0x00);
    input.push_back(0x00);
    input.push_back(0x00);
    input.push_back(0x02);
    input.insert(input.end(), shared_secret.begin(), shared_secret.end());
    
    unsigned char hash[32];
    EVP_Digest(input.data(), input.size(), hash, nullptr, EVP_sha256(), nullptr);
    
    std::memcpy(key.data(), hash, kMacKeySize);
    return key;
}

} // namespace rlpx::auth
