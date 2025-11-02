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
    // Factory method: Step 1 - Create and reserve space automatically
    // Returns error if reservation fails
    [[nodiscard]] static StreamingResult<RlpLargeStringEncoder> create(RlpEncoder& encoder);
    
    // Step 2 - Add chunk of payload data
    // Returns error if already finished
    [[nodiscard]] StreamingOperationResult addChunk(ByteView chunk);
    
    // Explicitly finish encoding and patch header
    // Can be called manually, or automatically called by destructor
    // Returns error if already finished
    [[nodiscard]] StreamingOperationResult finish();
    
    // Get current payload size
    [[nodiscard]] size_t payloadSize() const noexcept { return payload_size_; }
    
    // Check if already finished
    [[nodiscard]] bool isFinished() const noexcept { return finished_; }
    
    // Destructor automatically calls finish() if not already called
    ~RlpLargeStringEncoder();
    
    // Disable copying, enable moving with custom implementation
    RlpLargeStringEncoder(const RlpLargeStringEncoder&) = delete;
    RlpLargeStringEncoder& operator=(const RlpLargeStringEncoder&) = delete;
    RlpLargeStringEncoder(RlpLargeStringEncoder&& other) noexcept
        : encoder_(other.encoder_)
        , header_start_(other.header_start_)
        , payload_start_(other.payload_start_)
        , payload_size_(other.payload_size_)
        , finished_(other.finished_) {
        // Mark the moved-from object as finished to prevent double-finish
        other.finished_ = true;
    }
    RlpLargeStringEncoder& operator=(RlpLargeStringEncoder&&) = delete; // Not assignable after construction

private:
    // Private constructor - use create() factory method
    explicit RlpLargeStringEncoder(RlpEncoder& encoder);
    
    RlpEncoder& encoder_;
    size_t header_start_;      // Position where header starts
    size_t payload_start_;     // Position where payload starts (after reserved header)
    size_t payload_size_{0};   // Accumulated payload size
    bool finished_{false};     // Renamed from flushed_ for clarity
};

// Convenience function: encode large string with callback providing chunks
// Returns error if creation or write operation fails. Automatic finish() via RAII.
template <typename Func>
inline StreamingOperationResult encodeLargeString(RlpEncoder& encoder, Func&& generator) {
    BOOST_OUTCOME_TRY(auto stream, RlpLargeStringEncoder::create(encoder));
    
    StreamingOperationResult result = outcome::success();
    generator([&stream, &result](ByteView chunk) {
        if (!result) return; // Skip if already failed
        result = stream.addChunk(chunk);
    });
    
    return result; // finish() automatically called by destructor
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
    
    // Destructor automatically calls finish() if not already done
    ~RlpChunkedListEncoder();
    
    // Add data - automatically chunks into RLP strings within a list
    // Returns error if already finished
    [[nodiscard]] StreamingOperationResult addChunk(ByteView data);
    
    // Finalize encoding - flush any remaining buffered data as a final chunk
    // Returns error if already finished. Automatically called by destructor.
    [[nodiscard]] StreamingOperationResult finish();
    
    // Get number of chunks encoded
    [[nodiscard]] size_t chunkCount() const noexcept { return chunk_count_; }
    
    // Get total bytes encoded
    [[nodiscard]] size_t totalBytes() const noexcept { return total_bytes_; }
    
    // Check if already finished
    [[nodiscard]] bool isFinished() const noexcept { return finished_; }
    
    // Disable copying, enable moving with custom implementation
    RlpChunkedListEncoder(const RlpChunkedListEncoder&) = delete;
    RlpChunkedListEncoder& operator=(const RlpChunkedListEncoder&) = delete;
    RlpChunkedListEncoder(RlpChunkedListEncoder&& other) noexcept
        : encoder_(other.encoder_)
        , chunk_size_(other.chunk_size_)
        , buffer_(std::move(other.buffer_))
        , chunk_count_(other.chunk_count_)
        , total_bytes_(other.total_bytes_)
        , finished_(other.finished_)
        , list_started_(other.list_started_) {
        // Mark the moved-from object as finished to prevent double-finish
        other.finished_ = true;
    }
    RlpChunkedListEncoder& operator=(RlpChunkedListEncoder&&) = delete; // Not assignable after construction

private:
    // Private constructor - use create() factory method
    explicit RlpChunkedListEncoder(RlpEncoder& encoder, size_t chunk_size);
    
    void flushBuffer();
    
    RlpEncoder& encoder_;
    size_t chunk_size_;
    Bytes buffer_;
    size_t chunk_count_{0};
    size_t total_bytes_{0};
    bool finished_{false};
    bool list_started_{false};
};

// Convenience function: encode data as chunked list
// Returns error if creation or write fails. Automatic finish() via RAII.
template <typename Func>
inline StreamingOperationResult encodeChunkedList(RlpEncoder& encoder, Func&& generator, size_t chunk_size = 32768) {
    BOOST_OUTCOME_TRY(auto chunked, RlpChunkedListEncoder::create(encoder, chunk_size));
    
    StreamingOperationResult result = outcome::success();
    generator([&chunked, &result](ByteView chunk) {
        if (!result) return; // Skip if already failed
        result = chunked.addChunk(chunk);
    });
    
    return result; // finish() automatically called by destructor
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
//
// This decoder is self-contained and manages its own state without
// modifying the original RlpDecoder.

class RlpLargeStringDecoder {
public:
    // Create from RlpDecoder's remaining view
    // Creates an independent decoder by copying the remaining ByteView from the provided decoder.
    // The original decoder is not modified or affected.
    explicit RlpLargeStringDecoder(const RlpDecoder& decoder);
    
    // Create directly from ByteView
    explicit RlpLargeStringDecoder(ByteView data);
    
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
    
    // Get remaining view after decoding (for chaining)
    [[nodiscard]] ByteView remaining() const noexcept { return view_; }

private:
    ByteView view_;            // Our own view of the data
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
//
// This decoder is self-contained and manages its own state without
// modifying the original RlpDecoder.

class RlpChunkedListDecoder {
public:
    // Create from RlpDecoder's remaining view
    // The decoder is not modified - we take a copy of its view
    explicit RlpChunkedListDecoder(const RlpDecoder& decoder);
    
    // Create directly from ByteView
    explicit RlpChunkedListDecoder(ByteView data);
    
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
    
    // Get remaining view after decoding (for chaining)
    [[nodiscard]] ByteView remaining() const noexcept { return view_; }

private:
    ByteView view_;            // Our own view of the data
    ByteView list_payload_;    // View of the list payload being decoded
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
// Note: This is a legacy convenience function. Consider using RlpLargeStringDecoder directly.
template <typename Func>
inline DecodingResult decodeLargeString(RlpDecoder& decoder, Func&& callback, size_t read_chunk_size = 32768) {
    RlpLargeStringDecoder stream_decoder(decoder);
    
    // Initialize and read chunks
    while (true) {
        auto chunk_result = stream_decoder.readChunk(read_chunk_size);
        if (!chunk_result) {
            return chunk_result.error();
        }
        
        ByteView chunk = chunk_result.value();
        if (chunk.empty()) {
            break; // Finished
        }
        
        callback(chunk);
    }
    
    return outcome::success();
}

// Decode chunked list and reassemble (Approach B)
// Use for payloads encoded as list of RLP strings
// Note: This is a legacy convenience function. Consider using RlpChunkedListDecoder directly.
template <typename Func>
inline DecodingResult decodeChunkedList(RlpDecoder& decoder, Func&& callback) {
    RlpChunkedListDecoder stream_decoder(decoder);
    
    size_t chunk_index = 0;
    while (true) {
        auto chunk_result = stream_decoder.readChunk();
        if (!chunk_result) {
            return chunk_result.error();
        }
        
        ByteView chunk = chunk_result.value();
        if (chunk.empty()) {
            break; // Finished
        }
        
        callback(chunk, chunk_index++);
    }
    
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
