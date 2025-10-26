// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../rlpx_types.hpp"
#include "../rlpx_error.hpp"

namespace rlpx::crypto {

// NIST SP 800-56 Concatenation Key Derivation Function
// Used to derive encryption keys from shared secrets
class Kdf {
public:
    Kdf() = delete;

    // Derive key material using concat-KDF with SHA-256
    // Parameters:
    //   shared_secret: Input keying material (IKM)
    //   key_data_len: Desired output length in bytes
    //   shared_info: Optional context/application-specific info
    [[nodiscard]] static CryptoResult<ByteBuffer>
    derive(
        ByteView shared_secret,
        size_t key_data_len,
        ByteView shared_info = {}
    ) noexcept;

    // Convenience functions for standard key derivation
    [[nodiscard]] static CryptoResult<AesKey>
    derive_aes_key(ByteView shared_secret, ByteView info = {}) noexcept;

    [[nodiscard]] static CryptoResult<MacKey>
    derive_mac_key(ByteView shared_secret, ByteView info = {}) noexcept;

    // Derive multiple keys at once (more efficient)
    struct DerivedKeys {
        AesKey aes_key;
        MacKey mac_key;
    };

    [[nodiscard]] static CryptoResult<DerivedKeys>
    derive_keys(ByteView shared_secret, ByteView info = {}) noexcept;
};

} // namespace rlpx::crypto
