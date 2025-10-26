// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../rlpx_types.hpp"
#include "../rlpx_error.hpp"
#include "../auth/auth_keys.hpp"

namespace rlpx::framing {

// Frame encryption parameters
struct FrameEncryptParams {
    ByteView frame_data;
    bool is_first_frame;
};

// Frame decryption parameters
struct FrameDecryptParams {
    ByteView header_ciphertext;
    ByteView header_mac;
    ByteView frame_ciphertext;
    ByteView frame_mac;
};

// Frame cipher for RLPx message encryption
class FrameCipher {
public:
    // Initialize with secrets from handshake
    explicit FrameCipher(const auth::FrameSecrets& secrets) noexcept;

    // Encrypt frame: returns [header || header_mac || frame || frame_mac]
    [[nodiscard]] FramingResult<ByteBuffer>
    encrypt_frame(const FrameEncryptParams& params) noexcept;

    // Decrypt header to get frame size
    [[nodiscard]] FramingResult<size_t>
    decrypt_header(
        gsl::span<const uint8_t, kFrameHeaderSize> header_ciphertext,
        gsl::span<const uint8_t, kMacSize> header_mac
    ) noexcept;

    // Decrypt frame body
    [[nodiscard]] FramingResult<ByteBuffer>
    decrypt_frame(const FrameDecryptParams& params) noexcept;

    // Return const reference to secrets (grouped values)
    [[nodiscard]] const auth::FrameSecrets& secrets() const noexcept {
        return secrets_;
    }

private:
    // Update rolling MAC state
    void update_egress_mac(ByteView data) noexcept;
    void update_ingress_mac(ByteView data) noexcept;

    [[nodiscard]] MacDigest compute_header_mac(ByteView header_ct) noexcept;
    [[nodiscard]] MacDigest compute_frame_mac(ByteView frame_ct) noexcept;

    // Store secrets as member for grouped access
    auth::FrameSecrets secrets_;
    MacDigest egress_mac_state_;
    MacDigest ingress_mac_state_;
};

} // namespace rlpx::framing
