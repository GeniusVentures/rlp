// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/framing/message_stream.hpp>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <snappy.h>
#include <cstring>

namespace rlpx::framing {

MessageStream::MessageStream(
    std::unique_ptr<FrameCipher> cipher,
    socket::SocketTransport transport
) noexcept
    : cipher_(std::move(cipher))
    , transport_(std::move(transport)) {
    // Pre-allocate buffers to reduce allocations
    send_buffer_.reserve(4096);
    recv_buffer_.reserve(4096);
}

Awaitable<VoidResult> MessageStream::send_message(const MessageSendParams& params) noexcept {
    // go-ethereum wire format: RLP-encoded uint(message_id) || raw payload bytes
    // No outer list — matches rlp.AppendUint64(code) + data in writeFrame.
    rlp::RlpEncoder encoder;
    if (auto res = encoder.add(params.message_id); !res) {
        co_return SessionError::kInvalidMessage;
    }
    if (!params.payload.empty()) {
        if (auto res = encoder.AddRaw(detail::to_rlp_view(params.payload)); !res) {
            co_return SessionError::kInvalidMessage;
        }
    }

    auto result = encoder.GetBytes();
    if (!result) co_return SessionError::kInvalidMessage;
    ByteBuffer message_data = detail::from_rlp_bytes(*result.value());
        
        // Compress if enabled (after HELLO, all messages use Snappy per go-ethereum SetSnappy)
        if (compression_enabled_) {
            std::string compressed;
            snappy::Compress(
                reinterpret_cast<const char*>(message_data.data()),
                message_data.size(),
                &compressed
            );
            message_data.assign(compressed.begin(), compressed.end());
        }
        // Frame the message (may need to split into multiple frames)
        // For now, send as single frame (max 16MB)
        if ( message_data.size() > kMaxFrameSize ) {
            co_return SessionError::kInvalidMessage;
        }
        
        FrameEncryptParams frame_params{
            .frame_data = message_data,
            .is_first_frame = true
        };
        
        auto encrypted_frame_result = cipher_->encrypt_frame(frame_params);
        if ( !encrypted_frame_result ) {
            co_return SessionError::kEncryptionError;
        }
        
        // Send encrypted frame over socket
        auto write_result = co_await transport_.write_all(encrypted_frame_result.value());
        if ( !write_result ) {
            co_return write_result.error();
        }
        
        co_return outcome::success();
}

Awaitable<Result<Message>> MessageStream::receive_message() noexcept {
    // Receive encrypted frame from socket
    auto frame_result = co_await receive_frame();
    if ( !frame_result || frame_result.value().empty() ) {
        co_return SessionError::kInvalidMessage;
    }
        
        const auto& frame_data = frame_result.value();
        
        // Decompress if Snappy is enabled (after HELLO exchange)
        ByteBuffer decompressed;
        const ByteBuffer* decode_src = &frame_result.value();
        if (compression_enabled_) {
            size_t uncompressed_len = 0;
            if (!snappy::GetUncompressedLength(
                    reinterpret_cast<const char*>(decode_src->data()),
                    decode_src->size(),
                    &uncompressed_len)) {
                co_return SessionError::kInvalidMessage;
            }
            decompressed.resize(uncompressed_len);
            if (!snappy::RawUncompress(
                    reinterpret_cast<const char*>(decode_src->data()),
                    decode_src->size(),
                    reinterpret_cast<char*>(decompressed.data()))) {
                co_return SessionError::kInvalidMessage;
            }
            decode_src = &decompressed;
        }
        // go-ethereum wire format: RLP-encoded uint(code) || raw payload bytes
        // Matches rlp.SplitUint64(frame) in Read().
        rlp::RlpDecoder decoder(detail::to_rlp_view(*decode_src));

        uint64_t msg_id = 0;
        if (!decoder.read(msg_id)) {
            co_return SessionError::kInvalidMessage;
        }

        // Remaining bytes are the raw (possibly RLP-encoded) payload
        ByteBuffer payload;
        ByteView remaining_view = decoder.Remaining();
        if (!remaining_view.empty()) {
            payload.assign(remaining_view.begin(), remaining_view.end());
        }

        Message msg{static_cast<uint8_t>(msg_id), std::move(payload)};
        co_return msg;
}

Awaitable<FramingResult<void>> MessageStream::send_frame(ByteView frame_data) noexcept {
    // Encrypt and send frame
    FrameEncryptParams params{
        .frame_data = frame_data,
        .is_first_frame = true
    };
        
        auto encrypted_result = cipher_->encrypt_frame(params);
        if ( !encrypted_result ) {
            co_return encrypted_result.error();
        }
        
        // Send over socket
        auto write_result = co_await transport_.write_all(encrypted_result.value());
        if ( !write_result ) {
            co_return FramingError::kEncryptionFailed;
        }
        
        co_return outcome::success();
}

Awaitable<FramingResult<ByteBuffer>> MessageStream::receive_frame() noexcept {
    // Read frame header (32 bytes total = 16 header + 16 MAC)
    constexpr size_t kFrameHeaderWithMacSize = kFrameHeaderSize + kMacSize;
    auto header_with_mac_result = co_await transport_.read_exact(kFrameHeaderWithMacSize);
    if ( !header_with_mac_result ) {
        ByteBuffer empty;
        co_return empty;
    }
        
        const auto& header_data = header_with_mac_result.value();
        
        // Split into header and MAC
        gsl::span<const uint8_t, kFrameHeaderSize> header_span(
            header_data.data(), 
            kFrameHeaderSize
        );
        gsl::span<const uint8_t, kMacSize> header_mac_span(
            header_data.data() + kFrameHeaderSize, 
            kMacSize
        );
        
        // Decrypt header to get frame size
        auto frame_size_result = cipher_->decrypt_header(header_span, header_mac_span);
        if ( !frame_size_result ) {
            ByteBuffer empty;
            co_return empty;
        }
        
        size_t frame_size = frame_size_result.value();
        
        // Frame body is padded to 16-byte boundary on the wire; followed by 16-byte MAC.
        const size_t padding    = (frame_size % 16 != 0) ? (16 - frame_size % 16) : 0;
        const size_t padded_size = frame_size + padding;
        size_t total_frame_bytes = padded_size + kMacSize;
        auto frame_with_mac_result = co_await transport_.read_exact(total_frame_bytes);
        if ( !frame_with_mac_result ) {
            ByteBuffer empty;
            co_return empty;
        }
        
        const auto& frame_data = frame_with_mac_result.value();
        
        // Split into frame and MAC
        ByteView frame_ciphertext(frame_data.data(), padded_size);
        gsl::span<const uint8_t, kMacSize> frame_mac_span(
            frame_data.data() + padded_size, 
            kMacSize
        );
        
        // Decrypt frame body — header MAC already advanced in decrypt_header above
        auto decrypted_result = cipher_->decrypt_frame_body(
            frame_size,
            frame_ciphertext,
            ByteView(frame_mac_span.data(), kMacSize)
        );
        if ( !decrypted_result ) {
            ByteBuffer empty;
            co_return empty;
        }
        
        co_return decrypted_result.value();
}

} // namespace rlpx::framing
