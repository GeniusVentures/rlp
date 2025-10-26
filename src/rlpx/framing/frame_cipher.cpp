// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/framing/frame_cipher.hpp>
#include <rlpx/crypto/aes.hpp>
#include <rlpx/crypto/hmac.hpp>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <cstring>

namespace rlpx::framing {

FrameCipher::FrameCipher(const auth::FrameSecrets& secrets) noexcept
    : secrets_(secrets)
    , egress_mac_state_(secrets.egress_mac_seed)
    , ingress_mac_state_(secrets.ingress_mac_seed) {
}

FramingResult<ByteBuffer> FrameCipher::encrypt_frame(const FrameEncryptParams& params) noexcept {
    if ( params.frame_data.empty() ) {
        return FramingError::kInvalidFrameSize;
    }

    if ( params.frame_data.size() > kMaxFrameSize ) {
        return FramingError::kInvalidFrameSize;
    }

    // Frame structure: [header(16) || header-mac(16) || frame-data || frame-mac(16)]
    // Header: [frame-size(3) || header-data(13)]
    
    // Create header: 3-byte frame size (big-endian) + 13 bytes padding (zeros for now)
    std::array<uint8_t, kFrameHeaderSize> header{};
    uint32_t frame_size = static_cast<uint32_t>(params.frame_data.size());
    header[0] = static_cast<uint8_t>((frame_size >> 16) & 0xFF);
    header[1] = static_cast<uint8_t>((frame_size >> 8) & 0xFF);
    header[2] = static_cast<uint8_t>(frame_size & 0xFF);
    // Remaining 13 bytes are protocol-specific header data (zeros for now)

    // Encrypt header with AES-CTR
    std::array<uint8_t, kAesBlockSize> iv{};  // IV initialized from MAC state
    std::memcpy(iv.data(), egress_mac_state_.data(), kAesBlockSize);

    ByteBuffer header_ciphertext(kFrameHeaderSize);
    std::memcpy(header_ciphertext.data(), header.data(), kFrameHeaderSize);
    
    auto encrypt_result = crypto::Aes::encrypt_ctr_inplace(
        secrets_.aes_secret,
        iv,
        MutableByteView(header_ciphertext.data(), header_ciphertext.size())
    );

    if ( !encrypt_result ) {
        return FramingError::kEncryptionFailed;
    }

    // Update egress MAC with header ciphertext
    update_egress_mac(header_ciphertext);
    MacDigest header_mac = compute_header_mac(header_ciphertext);

    // Encrypt frame data
    ByteBuffer frame_ciphertext(params.frame_data.size());
    std::memcpy(frame_ciphertext.data(), params.frame_data.data(), params.frame_data.size());

    // Update IV for frame encryption
    std::memcpy(iv.data(), egress_mac_state_.data(), kAesBlockSize);

    auto encrypt_frame_result = crypto::Aes::encrypt_ctr_inplace(
        secrets_.aes_secret,
        iv,
        MutableByteView(frame_ciphertext.data(), frame_ciphertext.size())
    );

    if ( !encrypt_frame_result ) {
        return FramingError::kEncryptionFailed;
    }

    // Update egress MAC with frame ciphertext
    update_egress_mac(frame_ciphertext);
    MacDigest frame_mac = compute_frame_mac(frame_ciphertext);

    // Assemble complete frame: header || header_mac || frame || frame_mac
    ByteBuffer complete_frame;
    complete_frame.reserve(kFrameHeaderSize + kMacSize + frame_ciphertext.size() + kMacSize);
    
    complete_frame.insert(complete_frame.end(), header_ciphertext.begin(), header_ciphertext.end());
    complete_frame.insert(complete_frame.end(), header_mac.begin(), header_mac.end());
    complete_frame.insert(complete_frame.end(), frame_ciphertext.begin(), frame_ciphertext.end());
    complete_frame.insert(complete_frame.end(), frame_mac.begin(), frame_mac.end());

    return complete_frame;
}

FramingResult<size_t> FrameCipher::decrypt_header(
    gsl::span<const uint8_t, kFrameHeaderSize> header_ciphertext,
    gsl::span<const uint8_t, kMacSize> header_mac
) noexcept {
    // Verify header MAC
    update_ingress_mac(header_ciphertext);
    MacDigest expected_mac = compute_header_mac(header_ciphertext);

    // Constant-time comparison
    if ( CRYPTO_memcmp(header_mac.data(), expected_mac.data(), kMacSize) != 0 ) {
        return FramingError::kMacMismatch;
    }

    // Decrypt header
    std::array<uint8_t, kAesBlockSize> iv{};
    std::memcpy(iv.data(), ingress_mac_state_.data(), kAesBlockSize);

    ByteBuffer header_plaintext(kFrameHeaderSize);
    std::memcpy(header_plaintext.data(), header_ciphertext.data(), kFrameHeaderSize);

    auto decrypt_result = crypto::Aes::decrypt_ctr_inplace(
        secrets_.aes_secret,
        iv,
        MutableByteView(header_plaintext.data(), header_plaintext.size())
    );

    if ( !decrypt_result ) {
        return FramingError::kDecryptionFailed;
    }

    // Extract frame size (first 3 bytes, big-endian)
    size_t frame_size = (static_cast<size_t>(header_plaintext[0]) << 16) |
                       (static_cast<size_t>(header_plaintext[1]) << 8) |
                       static_cast<size_t>(header_plaintext[2]);

    if ( frame_size == 0 || frame_size > kMaxFrameSize ) {
        return FramingError::kInvalidFrameSize;
    }

    return frame_size;
}

FramingResult<ByteBuffer> FrameCipher::decrypt_frame(const FrameDecryptParams& params) noexcept {
    // First decrypt and verify header to get frame size
    auto frame_size_result = decrypt_header(
        gsl::span<const uint8_t, kFrameHeaderSize>(params.header_ciphertext.data(), kFrameHeaderSize),
        gsl::span<const uint8_t, kMacSize>(params.header_mac.data(), kMacSize)
    );
    if ( !frame_size_result ) {
        return frame_size_result.error();
    }
    uint32_t frame_size = frame_size_result.value();

    // Verify frame size matches provided data
    if ( params.frame_ciphertext.size() != frame_size ) {
        return FramingError::kInvalidFrameSize;
    }

    // Verify frame MAC
    update_ingress_mac(params.frame_ciphertext);
    MacDigest expected_frame_mac = compute_frame_mac(params.frame_ciphertext);

    if ( CRYPTO_memcmp(params.frame_mac.data(), expected_frame_mac.data(), kMacSize) != 0 ) {
        return FramingError::kMacMismatch;
    }

    // Decrypt frame data
    std::array<uint8_t, kAesBlockSize> iv{};
    std::memcpy(iv.data(), ingress_mac_state_.data(), kAesBlockSize);

    ByteBuffer frame_plaintext(params.frame_ciphertext.size());
    std::memcpy(frame_plaintext.data(), params.frame_ciphertext.data(), params.frame_ciphertext.size());

    auto decrypt_result = crypto::Aes::decrypt_ctr_inplace(
        secrets_.aes_secret,
        iv,
        MutableByteView(frame_plaintext.data(), frame_plaintext.size())
    );

    if ( !decrypt_result ) {
        return FramingError::kDecryptionFailed;
    }

    return frame_plaintext;
}

void FrameCipher::update_egress_mac(ByteView data) noexcept {
    // Update MAC state with data
    // RLPx uses a rolling MAC based on SHA3-256
    // For simplicity, we use HMAC-SHA256 here (should be SHA3-256 in production)
    
    ByteBuffer mac_input;
    mac_input.reserve(egress_mac_state_.size() + data.size());
    mac_input.insert(mac_input.end(), egress_mac_state_.begin(), egress_mac_state_.end());
    mac_input.insert(mac_input.end(), data.begin(), data.end());

    std::array<uint8_t, 32> new_mac;
    SHA256(mac_input.data(), mac_input.size(), new_mac.data());

    // Update state (use first 16 bytes)
    std::memcpy(egress_mac_state_.data(), new_mac.data(), kMacSize);
}

void FrameCipher::update_ingress_mac(ByteView data) noexcept {
    // Update MAC state with data
    ByteBuffer mac_input;
    mac_input.reserve(ingress_mac_state_.size() + data.size());
    mac_input.insert(mac_input.end(), ingress_mac_state_.begin(), ingress_mac_state_.end());
    mac_input.insert(mac_input.end(), data.begin(), data.end());

    std::array<uint8_t, 32> new_mac;
    SHA256(mac_input.data(), mac_input.size(), new_mac.data());

    // Update state (use first 16 bytes)
    std::memcpy(ingress_mac_state_.data(), new_mac.data(), kMacSize);
}

MacDigest FrameCipher::compute_header_mac(ByteView header_ct) noexcept {
    // Compute MAC for header
    // In RLPx: mac-secret encrypted with egress-mac XORed with header-ciphertext
    // Simplified version using HMAC
    
    auto mac_result = crypto::Hmac::compute_mac(secrets_.mac_secret, header_ct);
    if ( mac_result ) {
        return mac_result.value();
    }
    
    // Return zero MAC on error
    MacDigest zero_mac{};
    return zero_mac;
}

MacDigest FrameCipher::compute_frame_mac(ByteView frame_ct) noexcept {
    // Compute MAC for frame
    auto mac_result = crypto::Hmac::compute_mac(secrets_.mac_secret, frame_ct);
    if ( mac_result ) {
        return mac_result.value();
    }
    
    // Return zero MAC on error
    MacDigest zero_mac{};
    return zero_mac;
}

} // namespace rlpx::framing
