// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/auth/auth_handshake.hpp>
#include <rlpx/auth/ecies_cipher.hpp>
#include <rlpx/crypto/ecdh.hpp>
#include <rlpx/crypto/kdf.hpp>
#include <rlpx/crypto/hmac.hpp>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <cstring>

namespace rlpx::auth {

namespace {

// Create auth message (initiator -> responder)
// Format: ECIES(signature || sha3(initiator-ephemeral-pubk) || initiator-pubk || initiator-nonce || 0x00)
AuthResult<ByteBuffer> create_auth_message(
    gsl::span<const uint8_t, kPrivateKeySize> local_private_key,
    gsl::span<const uint8_t, kPublicKeySize> local_public_key,
    const PublicKey& ephemeral_public_key,
    const PrivateKey& ephemeral_private_key,
    const Nonce& nonce,
    gsl::span<const uint8_t, kPublicKeySize> remote_public_key
) noexcept {
    ByteBuffer auth_body;
    auth_body.reserve(65 + 32 + 64 + 32 + 1); // signature + hash + pubkey + nonce + version

    // Create signature of (static-shared-secret ^ nonce) using ephemeral private key
    // For RLPx v4, this proves we know the static private key
    auto* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    if ( !ctx ) {
        return AuthError::kSignatureInvalid;
    }

    // Compute static shared secret
    auto static_shared_secret_result = rlpx::crypto::Ecdh::compute_shared_secret(remote_public_key, local_private_key);
    if (!static_shared_secret_result) {
        return static_shared_secret_result.error();
    }
    auto static_shared_secret = static_shared_secret_result.value();

    // XOR with nonce
    std::array<uint8_t, 32> message_hash;
    for ( size_t i = 0; i < 32; ++i ) {
        message_hash[i] = static_shared_secret[i] ^ nonce[i];
    }

    // Sign with ephemeral private key
    secp256k1_ecdsa_recoverable_signature sig;
    if ( !secp256k1_ecdsa_sign_recoverable(ctx, &sig, message_hash.data(),
                                          ephemeral_private_key.data(), nullptr, nullptr) ) {
        secp256k1_context_destroy(ctx);
        return AuthError::kSignatureInvalid;
    }

    // Serialize signature (64 bytes + 1 byte recovery id)
    std::array<uint8_t, 64> sig_data;
    int recid = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, sig_data.data(), &recid, &sig);
    secp256k1_context_destroy(ctx);

    // Add signature to auth body
    auth_body.insert(auth_body.end(), sig_data.begin(), sig_data.end());
    auth_body.push_back(static_cast<uint8_t>(recid));

    // Add SHA3-256(ephemeral public key)
    std::array<uint8_t, 32> eph_pub_hash;
    SHA256(ephemeral_public_key.data(), ephemeral_public_key.size(), eph_pub_hash.data());
    auth_body.insert(auth_body.end(), eph_pub_hash.begin(), eph_pub_hash.end());

    // Add initiator public key (64 bytes, uncompressed without 0x04 prefix)
    auth_body.insert(auth_body.end(), local_public_key.begin(), local_public_key.end());

    // Add nonce
    auth_body.insert(auth_body.end(), nonce.begin(), nonce.end());

    // Add version byte (0x00 for v4)
    auth_body.push_back(0x00);

    // Encrypt with ECIES
    EciesEncryptParams params{
        .plaintext = auth_body,
        .recipient_public_key = remote_public_key,
        .shared_mac_data = {}
    };

    return EciesCipher::encrypt(params);
}

// Parse auth message (responder)
AuthResult<AuthKeyMaterial> parse_auth_message(
    ByteView encrypted_auth,
    gsl::span<const uint8_t, kPrivateKeySize> local_private_key
) noexcept {
    // Decrypt with ECIES
    EciesDecryptParams params{
        .ciphertext = encrypted_auth,
        .recipient_private_key = local_private_key,
        .shared_mac_data = {}
    };

    auto auth_body_result = EciesCipher::decrypt(params);
    if (!auth_body_result) {
        return auth_body_result.error();
    }
    auto auth_body = std::move(auth_body_result.value());

    // Parse auth body: signature(65) || eph-pubk-hash(32) || pubk(64) || nonce(32) || version(1)
    if ( auth_body.size() < 65 + 32 + 64 + 32 + 1 ) {
        return AuthError::kInvalidAuthMessage;
    }

    AuthKeyMaterial keys;

    // Extract signature
    size_t offset = 0;
    std::array<uint8_t, 65> signature;
    std::memcpy(signature.data(), auth_body.data() + offset, 65);
    offset += 65;

    // Extract ephemeral public key hash (we'll recover the actual key from signature)
    std::array<uint8_t, 32> eph_pub_hash;
    std::memcpy(eph_pub_hash.data(), auth_body.data() + offset, 32);
    offset += 32;

    // Extract initiator public key
    std::memcpy(keys.peer_public_key.data(), auth_body.data() + offset, kPublicKeySize);
    offset += kPublicKeySize;

    // Extract nonce
    std::memcpy(keys.initiator_nonce.data(), auth_body.data() + offset, kNonceSize);
    offset += kNonceSize;

    // Version byte (currently unused)
    // uint8_t version = auth_body[offset];

    // Store the auth message for MAC computation later
    keys.initiator_auth_message = std::move(auth_body);

    return keys;
}

// Create ack message (responder -> initiator)
// Format: ECIES(responder-ephemeral-pubk || responder-nonce || 0x00)
AuthResult<ByteBuffer> create_ack_message(
    const PublicKey& ephemeral_public_key,
    const Nonce& nonce,
    gsl::span<const uint8_t, kPublicKeySize> remote_public_key
) noexcept {
    ByteBuffer ack_body;
    ack_body.reserve(64 + 32 + 1); // ephemeral pubkey + nonce + version

    // Add ephemeral public key
    ack_body.insert(ack_body.end(), ephemeral_public_key.begin(), ephemeral_public_key.end());

    // Add nonce
    ack_body.insert(ack_body.end(), nonce.begin(), nonce.end());

    // Add version byte
    ack_body.push_back(0x00);

    // Encrypt with ECIES
    EciesEncryptParams params{
        .plaintext = ack_body,
        .recipient_public_key = remote_public_key,
        .shared_mac_data = {}
    };

    return EciesCipher::encrypt(params);
}

// Parse ack message (initiator)
AuthResult<void> parse_ack_message(
    ByteView encrypted_ack,
    gsl::span<const uint8_t, kPrivateKeySize> local_private_key,
    AuthKeyMaterial& keys
) noexcept {
    // Decrypt with ECIES
    EciesDecryptParams params{
        .ciphertext = encrypted_ack,
        .recipient_private_key = local_private_key,
        .shared_mac_data = {}
    };

    auto ack_body_result = EciesCipher::decrypt(params);
    if (!ack_body_result) {
        return ack_body_result.error();
    }
    auto ack_body = std::move(ack_body_result.value());

    // Parse ack body: eph-pubk(64) || nonce(32) || version(1)
    if ( ack_body.size() < 64 + 32 + 1 ) {
        return AuthError::kInvalidAckMessage;
    }

    size_t offset = 0;

    // Extract peer ephemeral public key
    std::memcpy(keys.peer_ephemeral_public_key.data(), ack_body.data() + offset, kPublicKeySize);
    offset += kPublicKeySize;

    // Extract recipient nonce
    std::memcpy(keys.recipient_nonce.data(), ack_body.data() + offset, kNonceSize);
    offset += kNonceSize;

    // Store ack message for MAC computation
    keys.recipient_ack_message = std::move(ack_body);

    return outcome::success();
}

} // anonymous namespace

AuthHandshake::AuthHandshake(const HandshakeConfig& config) noexcept
    : config_(config) {
}

// Note: This is a simplified synchronous version
// The async version would use Boost.Asio coroutines for socket I/O
Awaitable<Result<HandshakeResult>> AuthHandshake::execute() noexcept {
    // Generate ephemeral keypair
    auto keypair_result = rlpx::crypto::Ecdh::generate_ephemeral_keypair();
    if ( !keypair_result ) {
        co_return SessionError::kAuthenticationFailed;
    }
    auto keypair = keypair_result.value();

    // Generate nonce
    Nonce local_nonce;
    if ( RAND_bytes(local_nonce.data(), kNonceSize) != 1 ) {
        co_return SessionError::kAuthenticationFailed;
    }

    HandshakeResult result;
    result.key_material.local_ephemeral_public_key = keypair.public_key;
    result.key_material.local_ephemeral_private_key = keypair.private_key;

    if ( is_initiator() ) {
        // Initiator: send auth, receive ack
        result.key_material.initiator_nonce = local_nonce;

        auto auth_msg_result = create_auth_message(
            config_.local_private_key,
            config_.local_public_key,
            keypair.public_key,
            keypair.private_key,
            local_nonce,
            config_.peer_public_key.value()
        );

        if ( !auth_msg_result ) {
            co_return SessionError::kAuthenticationFailed;
        }

        // TODO: Send auth_msg_result.value() over socket
        // TODO: Receive ack message from socket
        // For now, this is a placeholder
        ByteBuffer ack_msg; // Would be received from socket

        auto parse_result = parse_ack_message(ack_msg, config_.local_private_key, 
                                             result.key_material);
        if ( !parse_result ) {
            co_return SessionError::kAuthenticationFailed;
        }

        result.key_material.peer_public_key = config_.peer_public_key.value();
    } else {
        // Responder: receive auth, send ack
        result.key_material.recipient_nonce = local_nonce;

        // TODO: Receive auth message from socket
        ByteBuffer auth_msg; // Would be received from socket

        auto parse_result = parse_auth_message(auth_msg, config_.local_private_key);
        if ( !parse_result ) {
            co_return SessionError::kAuthenticationFailed;
        }

        result.key_material = parse_result.value();
        result.key_material.local_ephemeral_public_key = keypair.public_key;
        result.key_material.local_ephemeral_private_key = keypair.private_key;
        result.key_material.recipient_nonce = local_nonce;

        auto ack_msg_result = create_ack_message(
            keypair.public_key,
            local_nonce,
            result.key_material.peer_public_key
        );

        if ( !ack_msg_result ) {
            co_return SessionError::kAuthenticationFailed;
        }

        // TODO: Send ack_msg_result.value() over socket
    }

    // Derive frame secrets
    result.frame_secrets = derive_frame_secrets(result.key_material, is_initiator());

    // TODO: Exchange Hello messages
    // result.peer_client_id = ...
    // result.peer_listen_port = ...

    co_return result;
}

Awaitable<AuthResult<AuthKeyMaterial>> AuthHandshake::perform_auth() noexcept {
    // This method would contain the core auth logic
    // Currently integrated into execute() above
    co_return AuthError::kInvalidAuthMessage; // Placeholder
}

Awaitable<Result<void>> AuthHandshake::exchange_hello(
    ByteView aes_key,
    ByteView mac_key
) noexcept {
    // This would perform the Hello message exchange
    // Placeholder for now
    co_return SessionError::kHandshakeFailed;
}

FrameSecrets AuthHandshake::derive_frame_secrets(
    const AuthKeyMaterial& keys,
    bool is_initiator
) noexcept {
    FrameSecrets secrets;

    // Compute shared secret from ephemeral keys
    auto shared_result = rlpx::crypto::Ecdh::compute_shared_secret(
        keys.peer_ephemeral_public_key,
        keys.local_ephemeral_private_key
    );

    if ( !shared_result ) {
        // Return empty secrets on error
        return secrets;
    }

    SharedSecret ephemeral_shared_secret = shared_result.value();

    // Derive AES and MAC keys using KDF
    // Per RLPx spec: keccak256(eph-shared-secret || keccak256(nonce || initiator-nonce))
    ByteBuffer nonce_material;
    nonce_material.insert(nonce_material.end(), keys.recipient_nonce.begin(), keys.recipient_nonce.end());
    nonce_material.insert(nonce_material.end(), keys.initiator_nonce.begin(), keys.initiator_nonce.end());

    std::array<uint8_t, 32> nonce_hash;
    SHA256(nonce_material.data(), nonce_material.size(), nonce_hash.data());

    ByteBuffer key_material;
    key_material.insert(key_material.end(), ephemeral_shared_secret.begin(), ephemeral_shared_secret.end());
    key_material.insert(key_material.end(), nonce_hash.begin(), nonce_hash.end());

    auto keys_result = rlpx::crypto::Kdf::derive_keys(key_material, {});
    if ( keys_result ) {
        secrets.aes_secret = keys_result.value().aes_key;
        secrets.mac_secret = keys_result.value().mac_key;
    }

    // Derive ingress/egress MAC seeds
    // egress-mac = keccak256(mac-secret ^ recipient-nonce || auth-ciphertext)
    // ingress-mac = keccak256(mac-secret ^ initiator-nonce || ack-ciphertext)

    if ( is_initiator ) {
        // Initiator: egress uses recipient nonce, ingress uses initiator nonce
        ByteBuffer egress_material;
        for ( size_t i = 0; i < kMacKeySize && i < kNonceSize; ++i ) {
            egress_material.push_back(secrets.mac_secret[i] ^ keys.recipient_nonce[i]);
        }
        egress_material.insert(egress_material.end(),
                             keys.initiator_auth_message.begin(),
                             keys.initiator_auth_message.end());

        auto egress_mac = rlpx::crypto::Hmac::compute_mac(secrets.mac_secret, egress_material);
        if ( egress_mac ) {
            secrets.egress_mac_seed = egress_mac.value();
        }

        ByteBuffer ingress_material;
        for ( size_t i = 0; i < kMacKeySize && i < kNonceSize; ++i ) {
            ingress_material.push_back(secrets.mac_secret[i] ^ keys.initiator_nonce[i]);
        }
        ingress_material.insert(ingress_material.end(),
                              keys.recipient_ack_message.begin(),
                              keys.recipient_ack_message.end());

        auto ingress_mac = rlpx::crypto::Hmac::compute_mac(secrets.mac_secret, ingress_material);
        if ( ingress_mac ) {
            secrets.ingress_mac_seed = ingress_mac.value();
        }
    } else {
        // Responder: opposite of initiator
        ByteBuffer ingress_material;
        for ( size_t i = 0; i < kMacKeySize && i < kNonceSize; ++i ) {
            ingress_material.push_back(secrets.mac_secret[i] ^ keys.recipient_nonce[i]);
        }
        ingress_material.insert(ingress_material.end(),
                              keys.initiator_auth_message.begin(),
                              keys.initiator_auth_message.end());

        auto ingress_mac = rlpx::crypto::Hmac::compute_mac(secrets.mac_secret, ingress_material);
        if ( ingress_mac ) {
            secrets.ingress_mac_seed = ingress_mac.value();
        }

        ByteBuffer egress_material;
        for ( size_t i = 0; i < kMacKeySize && i < kNonceSize; ++i ) {
            egress_material.push_back(secrets.mac_secret[i] ^ keys.initiator_nonce[i]);
        }
        egress_material.insert(egress_material.end(),
                             keys.recipient_ack_message.begin(),
                             keys.recipient_ack_message.end());

        auto egress_mac = rlpx::crypto::Hmac::compute_mac(secrets.mac_secret, egress_material);
        if ( egress_mac ) {
            secrets.egress_mac_seed = egress_mac.value();
        }
    }

    return secrets;
}

} // namespace rlpx::auth
