// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx_error.hpp>

namespace rlpx {

const char* to_string(SessionError error) noexcept {
    switch (error) {
        case SessionError::kNetworkFailure: return "Network failure";
        case SessionError::kAuthenticationFailed: return "Authentication failed";
        case SessionError::kHandshakeFailed: return "Handshake failed";
        case SessionError::kInvalidState: return "Invalid state";
        case SessionError::kPeerDisconnected: return "Peer disconnected";
        case SessionError::kTimeout: return "Timeout";
        case SessionError::kCompressionError: return "Compression error";
        case SessionError::kEncryptionError: return "Encryption error";
        case SessionError::kDecryptionError: return "Decryption error";
        case SessionError::kInvalidMessage: return "Invalid message";
        case SessionError::kInvalidFrameSize: return "Invalid frame size";
        case SessionError::kBufferOverflow: return "Buffer overflow";
        default: return "Unknown error";
    }
}

const char* to_string(AuthError error) noexcept {
    switch (error) {
        case AuthError::kEciesEncryptFailed: return "ECIES encryption failed";
        case AuthError::kEciesDecryptFailed: return "ECIES decryption failed";
        case AuthError::kInvalidAuthMessage: return "Invalid auth message";
        case AuthError::kInvalidAckMessage: return "Invalid ack message";
        case AuthError::kSharedSecretFailed: return "Shared secret computation failed";
        case AuthError::kSignatureInvalid: return "Invalid signature";
        case AuthError::kKeyDerivationFailed: return "Key derivation failed";
        case AuthError::kInvalidPublicKey: return "Invalid public key";
        case AuthError::kInvalidNonce: return "Invalid nonce";
        default: return "Unknown auth error";
    }
}

const char* to_string(FramingError error) noexcept {
    switch (error) {
        case FramingError::kEncryptionFailed: return "Frame encryption failed";
        case FramingError::kDecryptionFailed: return "Frame decryption failed";
        case FramingError::kMacMismatch: return "MAC mismatch";
        case FramingError::kInvalidFrameSize: return "Invalid frame size";
        case FramingError::kBufferTooSmall: return "Buffer too small";
        case FramingError::kInvalidHeader: return "Invalid header";
        case FramingError::kCompressionFailed: return "Compression failed";
        case FramingError::kDecompressionFailed: return "Decompression failed";
        default: return "Unknown framing error";
    }
}

} // namespace rlpx
