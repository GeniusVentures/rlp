// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>
#include <optional>
#include <gsl/span>

namespace rlpx {

// Constant sizes
inline constexpr size_t kPublicKeySize = 64;
inline constexpr size_t kPrivateKeySize = 32;
inline constexpr size_t kNonceSize = 32;
inline constexpr size_t kSharedSecretSize = 32;
inline constexpr size_t kAesKeySize = 32;
inline constexpr size_t kMacKeySize = 32;
inline constexpr size_t kMacSize = 16;
inline constexpr size_t kAesBlockSize = 16;
inline constexpr size_t kFrameHeaderSize = 16;
inline constexpr size_t kMaxFrameSize = 16 * 1024 * 1024; // 16 MB

// Type aliases for better semantics
using PublicKey = std::array<uint8_t, kPublicKeySize>;
using PrivateKey = std::array<uint8_t, kPrivateKeySize>;
using Nonce = std::array<uint8_t, kNonceSize>;
using SharedSecret = std::array<uint8_t, kSharedSecretSize>;
using AesKey = std::array<uint8_t, kAesKeySize>;
using MacKey = std::array<uint8_t, kMacKeySize>;
using MacDigest = std::array<uint8_t, kMacSize>;
using FrameHeader = std::array<uint8_t, kFrameHeaderSize>;

// Hide implementation details behind type aliases (Law of Demeter)
using ByteBuffer = std::vector<uint8_t>;
using ByteView = gsl::span<const uint8_t>;
using MutableByteView = gsl::span<uint8_t>;

// Session state enum
enum class SessionState : uint8_t {
    kUninitialized = 0,
    kConnecting,
    kAuthenticating,
    kHandshaking,
    kActive,
    kDisconnecting,
    kClosed,
    kError
};

// Disconnect reasons (from RLPx spec)
enum class DisconnectReason : uint8_t {
    kRequested = 0x00,
    kTcpError = 0x01,
    kProtocolError = 0x02,
    kUselessPeer = 0x03,
    kTooManyPeers = 0x04,
    kAlreadyConnected = 0x05,
    kIncompatibleVersion = 0x06,
    kInvalidIdentity = 0x07,
    kClientQuitting = 0x08,
    kUnexpectedIdentity = 0x09,
    kSelfConnection = 0x0A,
    kTimeout = 0x0B,
    kSubprotocolError = 0x10
};

// Protocol message IDs
inline constexpr uint8_t kHelloMessageId = 0x00;
inline constexpr uint8_t kDisconnectMessageId = 0x01;
inline constexpr uint8_t kPingMessageId = 0x02;
inline constexpr uint8_t kPongMessageId = 0x03;

// Protocol version
inline constexpr uint8_t kProtocolVersion = 5;

} // namespace rlpx
