// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../rlpx_types.hpp"
#include "../rlpx_error.hpp"
#include "frame_cipher.hpp"
#include <boost/asio/awaitable.hpp>
#include <memory>

namespace rlpx::framing {

// Hide Boost types (Law of Demeter)
template<typename T>
using Awaitable = boost::asio::awaitable<T>;

// Protocol message structure
struct Message {
    uint8_t id;
    ByteBuffer payload;
};

// Message send parameters
struct MessageSendParams {
    uint8_t message_id;
    ByteView payload;
    bool compress;
};

// Message stream handles framing, encryption, and compression
class MessageStream {
public:
    // Takes ownership of cipher
    explicit MessageStream(std::unique_ptr<FrameCipher> cipher) noexcept;

    // Send message (encodes, compresses if enabled, frames, encrypts)
    [[nodiscard]] Awaitable<VoidResult>
    send_message(const MessageSendParams& params) noexcept;

    // Receive message (decrypts, deframes, decompresses, decodes)
    [[nodiscard]] Awaitable<Result<Message>>
    receive_message() noexcept;

    // Enable compression after hello exchange
    void enable_compression() noexcept { compression_enabled_ = true; }

    // Query state
    [[nodiscard]] bool is_compression_enabled() const noexcept { 
        return compression_enabled_; 
    }

    // Access cipher secrets (grouped values)
    [[nodiscard]] const auth::FrameSecrets& cipher_secrets() const noexcept {
        return cipher_->secrets();
    }

private:
    [[nodiscard]] Awaitable<FramingResult<void>>
    send_frame(ByteView frame_data) noexcept;

    [[nodiscard]] Awaitable<FramingResult<ByteBuffer>>
    receive_frame() noexcept;

    std::unique_ptr<FrameCipher> cipher_;
    bool compression_enabled_{false};

    // Reusable buffers to minimize allocations
    ByteBuffer send_buffer_;
    ByteBuffer recv_buffer_;
};

} // namespace rlpx::framing
