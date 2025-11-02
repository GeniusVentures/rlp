#ifndef RLP_STREAMING_HPP
#define RLP_STREAMING_HPP

#include "common.hpp"
#include "rlp_encoder.hpp"
#include "rlp_decoder.hpp"
#include <functional>

namespace rlp {

// ============================================================================
// Approach A: Reserve & Patch Header (Single Large RLP String)
// ============================================================================
// Use this for producing canonical single RLP strings for large payloads.
// Common for: contract bytecode, large calldata, block bodies.
// 
// Benefits:
// - Produces canonical single RLP string (not chunked)
// - Minimal memory overhead (no intermediate buffering)
// - Single output stream
//
// Requirements:
// - Need random-access to output buffer (seeks back to patch header)
// - Must know or calculate total payload size before flushing

class RlpLargeStringEncoder {
public:
    explicit RlpLargeStringEncoder(RlpEncoder& encoder);
    
    // Add chunk of payload data
    // Returns error if already flushed
    [[nodiscard]] StreamingOperationResult write(ByteView chunk);
    
    // Flush - patches the header with actual payload size
    // Must be called before using the encoder output
    // Returns error if already flushed
    [[nodiscard]] StreamingOperationResult flush();
    
    // Get current payload size
    [[nodiscard]] size_t payloadSize() const noexcept { return payload_size_; }
    
    // Check if already flushed
    [[nodiscard]] bool isFlushed() const noexcept { return flushed_; }
    
    // Destructor does NOT auto-flush - caller must explicitly flush()
    ~RlpLargeStringEncoder() = default;
    
    // Disable copying, allow moving
    RlpLargeStringEncoder(const RlpLargeStringEncoder&) = delete;
    RlpLargeStringEncoder& operator=(const RlpLargeStringEncoder&) = delete;
    RlpLargeStringEncoder(RlpLargeStringEncoder&&) = default;
    RlpLargeStringEncoder& operator=(RlpLargeStringEncoder&&) = default;

private:
    RlpEncoder& encoder_;
    size_t header_start_;      // Position where header starts
    size_t payload_start_;     // Position where payload starts (after reserved header)
    size_t payload_size_{0};   // Accumulated payload size
    bool flushed_{false};
};

// Convenience function: encode large string with callback providing chunks
// Returns error if any write or flush operation fails
template <typename Func>
inline StreamingOperationResult encodeLargeString(RlpEncoder& encoder, Func&& generator) {
    RlpLargeStringEncoder stream(encoder);
    
    StreamingOperationResult result = outcome::success();
    generator([&stream, &result](ByteView chunk) {
        if (!result) return; // Skip if already failed
        result = stream.write(chunk);
    });
    
    if (!result) {
        return result;
    }
    
    return stream.flush();
}

// ============================================================================
// Approach B: Chunked List Encoding (Multiple RLP Strings)
// ============================================================================
// Use when both encoder/decoder agree on chunked format.
// Common for: streaming protocols, append-only logs, progressive data transfer.
//
// Benefits:
// - No header patching required (append-only)
// - Can start transmitting before knowing total size
// - Natural for streaming/progressive scenarios
//
// Trade-offs:
// - Not canonical (produces list-of-strings, not single string)
// - Requires decoder to reassemble chunks
// - Both sides must agree on chunked encoding protocol

class RlpChunkedListEncoder {
public:
    // Constructor validates chunk_size > 0
    [[nodiscard]] static StreamingResult<RlpChunkedListEncoder> create(RlpEncoder& encoder, size_t chunk_size = 32768);
    
    // Add data - automatically chunks into RLP strings within a list
    // Returns error if already flushed
    [[nodiscard]] StreamingOperationResult write(ByteView data);
    
    // Flush any remaining buffered data as a final chunk
    // Returns error if already flushed
    [[nodiscard]] StreamingOperationResult flush();
    
    // Get number of chunks encoded
    [[nodiscard]] size_t chunkCount() const noexcept { return chunk_count_; }
    
    // Get total bytes encoded
    [[nodiscard]] size_t totalBytes() const noexcept { return total_bytes_; }
    
    // Check if already flushed
    [[nodiscard]] bool isFlushed() const noexcept { return flushed_; }
    
    // Destructor does NOT auto-flush - caller must explicitly flush()
    ~RlpChunkedListEncoder() = default;
    
    // Disable copying, allow moving
    RlpChunkedListEncoder(const RlpChunkedListEncoder&) = delete;
    RlpChunkedListEncoder& operator=(const RlpChunkedListEncoder&) = delete;
    RlpChunkedListEncoder(RlpChunkedListEncoder&&) = default;
    RlpChunkedListEncoder& operator=(RlpChunkedListEncoder&&) = default;

private:
    // Private constructor - use create() factory method
    explicit RlpChunkedListEncoder(RlpEncoder& encoder, size_t chunk_size);
    
    void flushBuffer();
    
    RlpEncoder& encoder_;
    size_t chunk_size_;
    Bytes buffer_;
    size_t chunk_count_{0};
    size_t total_bytes_{0};
    bool flushed_{false};
    bool list_started_{false};
};

// Convenience function: encode data as chunked list
// Returns error if creation, write, or flush fails
template <typename Func>
inline StreamingOperationResult encodeChunkedList(RlpEncoder& encoder, Func&& generator, size_t chunk_size = 32768) {
    BOOST_OUTCOME_TRY(auto chunked, RlpChunkedListEncoder::create(encoder, chunk_size));
    
    StreamingOperationResult result = outcome::success();
    generator([&chunked, &result](ByteView chunk) {
        if (!result) return; // Skip if already failed
        result = chunked.write(chunk);
    });
    
    if (!result) {
        return result;
    }
    
    return chunked.flush();
}

// ============================================================================
// Streaming Decoder Classes (Two-Phase Approach)
// ============================================================================

// ============================================================================
// Approach A: Large String Decoder (Two-Phase)
// ============================================================================
// Two-phase streaming decoder for large RLP strings:
// Phase 1: Peek size → allows caller to allocate buffer
// Phase 2: Read chunks → decode into pre-allocated buffer
//
// Use this for memory-efficient decoding of large single RLP strings

class RlpLargeStringDecoder {
public:
    explicit RlpLargeStringDecoder(RlpDecoder& decoder);
    
    // Phase 1: Get total payload size (allows pre-allocation)
    [[nodiscard]] Result<size_t> peekPayloadSize() const noexcept;
    
    // Phase 2: Read next chunk of payload data
    // Returns ByteView of the chunk read (size <= max_chunk_size)
    // Returns empty view when no more data
    // Returns error if not initialized or already finished
    [[nodiscard]] Result<ByteView> readChunk(size_t max_chunk_size = 32768);
    
    // Get current position in payload
    [[nodiscard]] size_t currentPosition() const noexcept { return bytes_read_; }
    
    // Get total payload size (after initialization)
    [[nodiscard]] size_t totalSize() const noexcept { return payload_size_; }
    
    // Check if all data has been read
    [[nodiscard]] bool isFinished() const noexcept { return bytes_read_ >= payload_size_; }
    
    // Check if initialized (header consumed)
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

private:
    RlpDecoder& decoder_;
    size_t payload_size_{0};
    size_t bytes_read_{0};
    bool initialized_{false};
};

// ============================================================================
// Approach B: Chunked List Decoder (Two-Phase)
// ============================================================================
// Two-phase streaming decoder for chunked RLP lists:
// Phase 1: Peek total reassembled size → allows caller to allocate buffer
// Phase 2: Read list items → decode each chunk
//
// Use this when data is encoded as list of RLP strings

class RlpChunkedListDecoder {
public:
    explicit RlpChunkedListDecoder(RlpDecoder& decoder);
    
    // Phase 1: Get total reassembled size (sum of all chunk sizes)
    // Must scan through list to calculate total
    [[nodiscard]] Result<size_t> peekTotalSize();
    
    // Get number of chunks in the list
    [[nodiscard]] Result<size_t> peekChunkCount();
    
    // Phase 2: Read next chunk from the list
    // Returns ByteView of the chunk
    // Returns empty view when no more chunks
    // Returns error if not initialized or decoding fails
    [[nodiscard]] Result<ByteView> readChunk();
    
    // Get current chunk index
    [[nodiscard]] size_t currentChunkIndex() const noexcept { return chunk_index_; }
    
    // Get total number of chunks (after initialization)
    [[nodiscard]] size_t totalChunks() const noexcept { return total_chunks_; }
    
    // Get total reassembled size (after peek)
    [[nodiscard]] size_t totalSize() const noexcept { return total_size_; }
    
    // Check if all chunks have been read
    [[nodiscard]] bool isFinished() const noexcept { return chunk_index_ >= total_chunks_; }
    
    // Check if initialized (list header consumed)
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

private:
    RlpDecoder& decoder_;
    ByteView list_payload_;
    size_t total_size_{0};
    size_t total_chunks_{0};
    size_t chunk_index_{0};
    bool initialized_{false};
};

// ============================================================================
// Callback-Based Decoder Functions (Legacy/Convenience)
// ============================================================================

// Decode large string with streaming callback (Approach A)
// Use for payloads encoded as single RLP string
template <typename Func>
inline DecodingResult decodeLargeString(RlpDecoder& decoder, Func&& callback, size_t read_chunk_size = 32768) {
    BOOST_OUTCOME_TRY(auto h, decoder.PeekHeader());
    
    if (h.list) {
        return DecodingError::kUnexpectedList;
    }
    
    if (decoder.Remaining().length() < h.header_size_bytes + h.payload_size_bytes) {
        return DecodingError::kInputTooShort;
    }
    
    // Skip header
    decoder.Advance(h.header_size_bytes);
    
    // Stream payload in chunks
    size_t remaining = h.payload_size_bytes;
    while (remaining > 0) {
        size_t current_chunk = std::min(remaining, read_chunk_size);
        ByteView chunk = decoder.Remaining().substr(0, current_chunk);
        callback(chunk);
        decoder.Advance(current_chunk);
        remaining -= current_chunk;
    }
    
    return outcome::success();
}

// Decode chunked list and reassemble (Approach B)
// Use for payloads encoded as list of RLP strings
template <typename Func>
inline DecodingResult decodeChunkedList(RlpDecoder& decoder, Func&& callback) {
    BOOST_OUTCOME_TRY(size_t list_payload_len, decoder.ReadListHeaderBytes());
    
    ByteView list_payload = decoder.Remaining().substr(0, list_payload_len);
    size_t chunk_index = 0;
    
    while (!list_payload.empty()) {
        RlpDecoder chunk_decoder(list_payload);
        
        // Each chunk should be a string (bytes)
        BOOST_OUTCOME_TRY(auto h, chunk_decoder.PeekHeader());
        if (h.list) {
            return DecodingError::kUnexpectedList;
        }
        
        // Read the chunk
        Bytes chunk_data;
        BOOST_OUTCOME_TRY(chunk_decoder.read(chunk_data));
        
        // Deliver to callback
        callback(ByteView(chunk_data), chunk_index++);
        
        list_payload = chunk_decoder.Remaining();
    }
    
    // Consume the list payload from main decoder
    decoder.Advance(list_payload_len);
    
    return outcome::success();
}

// Full reassembly helper for chunked lists
inline Result<Bytes> decodeChunkedListFull(RlpDecoder& decoder) {
    Bytes result;
    
    auto decode_result = decodeChunkedList(decoder, [&result](ByteView chunk, size_t) {
        result.append(chunk.data(), chunk.size());
    });
    
    if (!decode_result) {
        return decode_result.error();
    }
    
    return result;
}

} // namespace rlp

#endif // RLP_STREAMING_HPP
