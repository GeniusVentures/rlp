// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/framing/message_stream.hpp>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <cstring>

namespace rlpx::framing {

MessageStream::MessageStream(std::unique_ptr<FrameCipher> cipher) noexcept
    : cipher_(std::move(cipher)) {
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
        encoder.Add(params.message_id);
        
        // Add payload as raw bytes if it's already RLP-encoded
        if ( !params.payload.empty() ) {
            encoder.AddRaw(params.payload);
        }
        
        encoder.EndList();
        
        ByteBuffer message_data = encoder.MoveBytes();
        
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
        
        // TODO: Send encrypted frame over socket
        // For now, this is a placeholder
        // co_await async_write(socket_, buffer(encrypted_frame_result.value()));
        
        co_return outcome::success();
        
    } catch ( ... ) {
        co_return SessionError::kInvalidMessage;
    }
}

Awaitable<Result<Message>> MessageStream::receive_message() noexcept {
    try {
        // TODO: Receive frame from socket
        // For now, this is a placeholder
        // This would typically:
        // 1. Read header (16 bytes) + header MAC (16 bytes)
        // 2. Decrypt header to get frame size
        // 3. Read frame data (frame_size bytes) + frame MAC (16 bytes)
        // 4. Decrypt frame data
        
        ByteBuffer frame_data; // Would be received from socket
        
        // Placeholder frame decryption
        // auto decrypt_result = cipher_->decrypt_frame(...);
        // if ( !decrypt_result ) {
        //     co_return SessionError::kDecryptionError;
        // }
        // ByteBuffer message_data = decrypt_result.value();
        
        // TODO: Decompress if enabled
        // if ( compression_enabled_ ) {
        //     message_data = decompress_snappy(message_data);
        // }
        
        // Parse RLP message: [msg-id, msg-data]
        ByteBuffer message_data; // Placeholder
        rlp::RlpDecoder decoder(message_data);
        
        if ( !decoder.IsList() ) {
            co_return SessionError::kInvalidMessage;
        }
        
        Message msg;
        
        // Read message ID
        BOOST_OUTCOME_TRY(id_bytes, decoder.ReadListHeaderBytes());
        if ( id_bytes.size() == 1 ) {
            msg.id = id_bytes[0];
        } else {
            co_return SessionError::kInvalidMessage;
        }
        
        // Read message payload (rest of the list)
        if ( !decoder.IsFinished() ) {
            BOOST_OUTCOME_TRY(payload_bytes, decoder.ReadListHeaderBytes());
            msg.payload = ByteBuffer(payload_bytes.begin(), payload_bytes.end());
        }
        
        co_return msg;
        
    } catch ( ... ) {
        co_return SessionError::kInvalidMessage;
    }
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
    
    // TODO: Send over socket
    // co_await async_write(socket_, buffer(encrypted_result.value()));
    
    co_return outcome::success();
}

Awaitable<FramingResult<ByteBuffer>> MessageStream::receive_frame() noexcept {
    // TODO: Receive and decrypt frame from socket
    // This would:
    // 1. Read header + header MAC
    // 2. Decrypt header to get size
    // 3. Read frame + frame MAC  
    // 4. Decrypt frame
    
    ByteBuffer frame_data; // Placeholder
    co_return frame_data;
}

} // namespace rlpx::framing
