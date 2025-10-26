// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <boost/outcome/result.hpp>
#include "rlpx_types.hpp"

namespace rlpx {

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

enum class SessionError {
    kNetworkFailure,
    kAuthenticationFailed,
    kHandshakeFailed,
    kInvalidState,
    kPeerDisconnected,
    kTimeout,
    kCompressionError,
    kEncryptionError,
    kDecryptionError,
    kInvalidMessage,
    kInvalidFrameSize,
    kBufferOverflow
};

enum class AuthError {
    kEciesEncryptFailed,
    kEciesDecryptFailed,
    kInvalidAuthMessage,
    kInvalidAckMessage,
    kSharedSecretFailed,
    kSignatureInvalid,
    kKeyDerivationFailed,
    kInvalidPublicKey,
    kInvalidNonce
};

enum class FramingError {
    kEncryptionFailed,
    kDecryptionFailed,
    kMacMismatch,
    kInvalidFrameSize,
    kBufferTooSmall,
    kInvalidHeader,
    kCompressionFailed,
    kDecompressionFailed
};

enum class CryptoError {
    kAesEncryptFailed,
    kAesDecryptFailed,
    kHmacFailed,
    kKdfFailed,
    kEcdhFailed,
    kInvalidKeySize,
    kInvalidIvSize,
    kInvalidPublicKey,
    kInvalidPrivateKey,
    kOpenSslError,
    kSecp256k1Error
};

template<typename T>
using Result = outcome::result<T, SessionError>;

template<typename T>
using AuthResult = outcome::result<T, AuthError>;

template<typename T>
using FramingResult = outcome::result<T, FramingError>;

template<typename T>
using CryptoResult = outcome::result<T, CryptoError>;

using VoidResult = outcome::result<void, SessionError>;
using AuthVoidResult = outcome::result<void, AuthError>;
using FramingVoidResult = outcome::result<void, FramingError>;
using CryptoVoidResult = outcome::result<void, CryptoError>;

// Error message conversion
const char* to_string(SessionError error) noexcept;
const char* to_string(AuthError error) noexcept;
const char* to_string(FramingError error) noexcept;
const char* to_string(CryptoError error) noexcept;

} // namespace rlpx
