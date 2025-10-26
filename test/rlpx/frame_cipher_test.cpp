// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <rlpx/framing/frame_cipher.hpp>
#include <rlpx/crypto/kdf.hpp>
#include <vector>

using namespace rlpx;
using namespace rlpx::framing;
using namespace rlpx::auth;

// Helper to create test secrets
FrameSecrets create_test_secrets() {
    FrameSecrets secrets;
    secrets.aes_secret.fill(0x42);
    secrets.mac_secret.fill(0x55);
    secrets.egress_mac_seed.fill(0xAA);
    secrets.ingress_mac_seed.fill(0xBB);
    return secrets;
}

// Helper to create flipped secrets for decrypt side
// (alice's egress = bob's ingress, alice's ingress = bob's egress)
FrameSecrets create_flipped_secrets() {
    FrameSecrets secrets;
    secrets.aes_secret.fill(0x42);
    secrets.mac_secret.fill(0x55);
    secrets.egress_mac_seed.fill(0xBB);  // Swapped
    secrets.ingress_mac_seed.fill(0xAA);  // Swapped
    return secrets;
}

TEST(FrameCipherTest, ConstructorInitialization) {
    auto secrets = create_test_secrets();
    FrameCipher cipher(secrets);
    
    EXPECT_EQ(cipher.secrets().aes_secret, secrets.aes_secret);
    EXPECT_EQ(cipher.secrets().mac_secret, secrets.mac_secret);
}

TEST(FrameCipherTest, EncryptFrame) {
    auto secrets = create_test_secrets();
    FrameCipher cipher(secrets);
    
    std::vector<uint8_t> frame_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    FrameEncryptParams params{
        .frame_data = frame_data,
        .is_first_frame = true
    };
    
    auto result = cipher.encrypt_frame(params);
    
    ASSERT_TRUE(result.has_value());
    
    // Encrypted frame should be: header(16) + header_mac(16) + frame + frame_mac(16)
    size_t expected_size = kFrameHeaderSize + kMacSize + frame_data.size() + kMacSize;
    EXPECT_EQ(result.value().size(), expected_size);
}

TEST(FrameCipherTest, EncryptEmptyFrame) {
    auto secrets = create_test_secrets();
    FrameCipher cipher(secrets);
    
    std::vector<uint8_t> empty_data;
    
    FrameEncryptParams params{
        .frame_data = empty_data,
        .is_first_frame = true
    };
    
    auto result = cipher.encrypt_frame(params);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FramingError::kInvalidFrameSize);
}

TEST(FrameCipherTest, EncryptTooLargeFrame) {
    auto secrets = create_test_secrets();
    FrameCipher cipher(secrets);
    
    std::vector<uint8_t> too_large_data(kMaxFrameSize + 1, 0xFF);
    
    FrameEncryptParams params{
        .frame_data = too_large_data,
        .is_first_frame = true
    };
    
    auto result = cipher.encrypt_frame(params);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FramingError::kInvalidFrameSize);
}

TEST(FrameCipherTest, DecryptHeader) {
    auto secrets = create_test_secrets();
    FrameCipher cipher_encrypt(secrets);
    
    std::vector<uint8_t> frame_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    FrameEncryptParams params{
        .frame_data = frame_data,
        .is_first_frame = true
    };
    
    auto encrypted = cipher_encrypt.encrypt_frame(params);
    ASSERT_TRUE(encrypted.has_value());
    
    // Extract header and header MAC
    std::array<uint8_t, kFrameHeaderSize> header_ct;
    std::array<uint8_t, kMacSize> header_mac;
    
    std::memcpy(header_ct.data(), encrypted.value().data(), kFrameHeaderSize);
    std::memcpy(header_mac.data(), encrypted.value().data() + kFrameHeaderSize, kMacSize);
    
    // Decrypt header with flipped secrets (alice's egress = bob's ingress)
    FrameCipher cipher_decrypt(create_flipped_secrets());
    auto size_result = cipher_decrypt.decrypt_header(header_ct, header_mac);
    
    ASSERT_TRUE(size_result.has_value());
    EXPECT_EQ(size_result.value(), frame_data.size());
}

TEST(FrameCipherTest, DecryptHeaderInvalidMac) {
    auto secrets = create_test_secrets();
    FrameCipher cipher_encrypt(secrets);
    
    std::vector<uint8_t> frame_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    FrameEncryptParams params{
        .frame_data = frame_data,
        .is_first_frame = true
    };
    
    auto encrypted = cipher_encrypt.encrypt_frame(params);
    ASSERT_TRUE(encrypted.has_value());
    
    // Extract header and corrupt MAC
    std::array<uint8_t, kFrameHeaderSize> header_ct;
    std::array<uint8_t, kMacSize> header_mac;
    
    std::memcpy(header_ct.data(), encrypted.value().data(), kFrameHeaderSize);
    std::memcpy(header_mac.data(), encrypted.value().data() + kFrameHeaderSize, kMacSize);
    
    // Corrupt MAC
    header_mac[0] ^= 0xFF;
    
    // Try to decrypt with flipped secrets
    FrameCipher cipher_decrypt(create_flipped_secrets());
    auto size_result = cipher_decrypt.decrypt_header(header_ct, header_mac);
    
    EXPECT_FALSE(size_result.has_value());
    EXPECT_EQ(size_result.error(), FramingError::kMacMismatch);
}

TEST(FrameCipherTest, EncryptDecryptRoundtrip) {
    auto secrets = create_test_secrets();
    
    std::vector<uint8_t> original_data = {
        0x48, 0x65, 0x6c, 0x6c, 0x6f,  // "Hello"
        0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64  // " World"
    };
    
    // Encrypt
    FrameCipher cipher_encrypt(secrets);
    FrameEncryptParams params{
        .frame_data = original_data,
        .is_first_frame = true
    };
    
    auto encrypted = cipher_encrypt.encrypt_frame(params);
    ASSERT_TRUE(encrypted.has_value());
    
    // Parse encrypted frame
    size_t offset = 0;
    std::array<uint8_t, kFrameHeaderSize> header_ct;
    std::array<uint8_t, kMacSize> header_mac;
    
    std::memcpy(header_ct.data(), encrypted.value().data() + offset, kFrameHeaderSize);
    offset += kFrameHeaderSize;
    
    std::memcpy(header_mac.data(), encrypted.value().data() + offset, kMacSize);
    offset += kMacSize;
    
    size_t frame_size = encrypted.value().size() - offset - kMacSize;
    std::vector<uint8_t> frame_ct(frame_size);
    std::memcpy(frame_ct.data(), encrypted.value().data() + offset, frame_size);
    offset += frame_size;
    
    std::array<uint8_t, kMacSize> frame_mac;
    std::memcpy(frame_mac.data(), encrypted.value().data() + offset, kMacSize);
    
    // Decrypt with flipped secrets (alice's egress = bob's ingress)
    FrameCipher cipher_decrypt(create_flipped_secrets());
    FrameDecryptParams decrypt_params{
        .header_ciphertext = ByteView(header_ct.data(), header_ct.size()),
        .header_mac = ByteView(header_mac.data(), header_mac.size()),
        .frame_ciphertext = ByteView(frame_ct.data(), frame_ct.size()),
        .frame_mac = ByteView(frame_mac.data(), frame_mac.size())
    };
    
    auto decrypted = cipher_decrypt.decrypt_frame(decrypt_params);
    ASSERT_TRUE(decrypted.has_value());
    EXPECT_EQ(decrypted.value(), original_data);
}

TEST(FrameCipherTest, MultipleFrames) {
    auto secrets = create_test_secrets();
    FrameCipher cipher(secrets);
    
    std::vector<std::vector<uint8_t>> frames = {
        {0x01, 0x02, 0x03},
        {0x04, 0x05, 0x06, 0x07},
        {0x08, 0x09}
    };
    
    for ( const auto& frame_data : frames ) {
        FrameEncryptParams params{
            .frame_data = frame_data,
            .is_first_frame = false
        };
        
        auto result = cipher.encrypt_frame(params);
        ASSERT_TRUE(result.has_value());
        
        // Each frame should have correct structure
        size_t expected_size = kFrameHeaderSize + kMacSize + frame_data.size() + kMacSize;
        EXPECT_EQ(result.value().size(), expected_size);
    }
}

TEST(FrameCipherTest, MaxFrameSize) {
    auto secrets = create_test_secrets();
    FrameCipher cipher(secrets);
    
    std::vector<uint8_t> max_frame(kMaxFrameSize, 0xAA);
    
    FrameEncryptParams params{
        .frame_data = max_frame,
        .is_first_frame = true
    };
    
    auto result = cipher.encrypt_frame(params);
    
    ASSERT_TRUE(result.has_value());
    size_t expected_size = kFrameHeaderSize + kMacSize + kMaxFrameSize + kMacSize;
    EXPECT_EQ(result.value().size(), expected_size);
}
