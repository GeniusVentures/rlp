// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/framing/message_stream.hpp>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
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
    try {
        // Create message payload: message_id || rlp_payload
        // For RLPx, the message format is: [msg-id, msg-data]
        rlp::RlpEncoder encoder;
        encoder.BeginList();
        encoder.add(params.message_id);
        
        // Add payload as raw bytes if it's already RLP-encoded
        if ( !params.payload.empty() ) {
            encoder.AddRaw(detail::to_rlp_view(params.payload));
        }
        
        encoder.EndList();
        
        auto result = encoder.GetBytes();
        if (!result) co_return SessionError::kInvalidMessage;
        ByteBuffer message_data = detail::from_rlp_bytes(*result.value());
        
        // TODO: Compress if enabled
        // if ( params.compress && compression_enabled_ ) {
        //     message_data = compress_snappy(message_data);
        // }
        
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
        
    } catch ( ... ) {
        co_return SessionError::kInvalidMessage;
    }
}

Awaitable<Result<Message>> MessageStream::receive_message() noexcept {
    try {
        // Receive encrypted frame from socket
        auto frame_result = co_await receive_frame();
        if ( !frame_result || frame_result.value().empty() ) {
            co_return SessionError::kInvalidMessage;
        }
        
        const auto& frame_data = frame_result.value();
        
        // TODO: Decompress if needed
        // if ( compression_enabled_ ) {
        //     frame_data = decompress_snappy(frame_data);
        // }
        
        // Decode RLP message: [msg-id, msg-data]
        rlp::RlpDecoder decoder(detail::to_rlp_view(frame_data));
        
        auto list_size_result = decoder.ReadListHeaderBytes();
        if ( !list_size_result ) {
            co_return SessionError::kInvalidMessage;
        }
        
        // Read message ID
        uint8_t msg_id;
        if ( !decoder.read(msg_id) ) {
            co_return SessionError::kInvalidMessage;
        }
        
        // Read remaining payload as raw bytes
        ByteBuffer payload;
        ByteView remaining_view = decoder.Remaining();
        if ( !remaining_view.empty() ) {
            payload.assign(remaining_view.begin(), remaining_view.end());
        }
        
        Message msg{msg_id, std::move(payload)};
        co_return msg;
        
    } catch ( ... ) {
        co_return SessionError::kInvalidMessage;
    }
}

Awaitable<FramingResult<void>> MessageStream::send_frame(ByteView frame_data) noexcept {
    try {
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
    } catch ( ... ) {
        co_return FramingError::kEncryptionFailed;
    }
}

Awaitable<FramingResult<ByteBuffer>> MessageStream::receive_frame() noexcept {
    try {
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
        
        // Read frame body (frame_size + 16 bytes for MAC)
        size_t total_frame_bytes = frame_size + kMacSize;
        auto frame_with_mac_result = co_await transport_.read_exact(total_frame_bytes);
        if ( !frame_with_mac_result ) {
            ByteBuffer empty;
            co_return empty;
        }
        
        const auto& frame_data = frame_with_mac_result.value();
        
        // Split into frame and MAC
        ByteView frame_ciphertext(frame_data.data(), frame_size);
        gsl::span<const uint8_t, kMacSize> frame_mac_span(
            frame_data.data() + frame_size, 
            kMacSize
        );
        
        // Decrypt frame
        FrameDecryptParams decrypt_params{
            .header_ciphertext = header_span,
            .header_mac = header_mac_span,
            .frame_ciphertext = frame_ciphertext,
            .frame_mac = frame_mac_span
        };
        
        auto decrypted_result = cipher_->decrypt_frame(decrypt_params);
        if ( !decrypted_result ) {
            ByteBuffer empty;
            co_return empty;
        }
        
        co_return decrypted_result.value();
        
    } catch ( ... ) {
        ByteBuffer empty;
        co_return empty;
    }
}

} // namespace rlpx::framing
