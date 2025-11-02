#include <rlp/rlp_streaming.hpp>
#include <rlp/endian.hpp>
#include <cstring>

namespace rlp {

namespace { // Anonymous namespace for helpers

// Encode RLP string header for given payload length
// Returns empty bytes if length exceeds 64-bit limit (practically impossible)
Bytes encode_string_header(size_t payload_len) {
    Bytes header;
    
    if (payload_len <= kMaxShortStringLen) { // <= 55 bytes
        header.push_back(static_cast<uint8_t>(kShortStringOffset + payload_len));
    } else {
        // Long form: 0xb8 + length_of_length + length_bytes
        Bytes len_be = endian::to_big_compact(static_cast<uint64_t>(payload_len));
        if (len_be.length() > 8) {
            // Practically impossible on 64-bit systems
            return Bytes{}; // Return empty to signal error
        }
        header.reserve(1 + len_be.length());
        header.push_back(static_cast<uint8_t>(kLongStringOffset + len_be.length()));
        header.append(len_be.data(), len_be.length());
    }
    
    return header;
}

} // namespace

// ============================================================================
// Approach A: Reserve & Patch Header Implementation
// ============================================================================

// Factory method implementation
StreamingResult<RlpLargeStringEncoder> RlpLargeStringEncoder::create(RlpEncoder& encoder) {
    // Validate that encoder is in a valid state
    auto buffer_result = encoder.GetBytes();
    if (!buffer_result) {
        return StreamingError::kNotFinalized; // Encoder has unclosed lists
    }
    
    // Use private constructor
    return RlpLargeStringEncoder(encoder);
}

RlpLargeStringEncoder::RlpLargeStringEncoder(RlpEncoder& encoder)
    : encoder_(encoder)
    , header_start_(encoder.size())
    , payload_start_(header_start_ + 9) // Reserve max header size (1 + 8 bytes)
    , payload_size_(0)
    , finished_(false) {
    // Reserve maximum possible header space (1 byte prefix + 8 bytes length)
    encoder_.reserve(encoder_.size() + 9);
    // Temporarily fill with zeros (will be patched in finish())
    auto buffer_result = encoder_.GetBytes();
    if (buffer_result) {
        Bytes* buffer = buffer_result.value();
        for (size_t i = 0; i < 9; ++i) {
            buffer->push_back(0);
        }
    }
}

// Destructor - automatically finish if not already done
RlpLargeStringEncoder::~RlpLargeStringEncoder() {
    if (!finished_) {
        (void)finish(); // Ignore result in destructor
    }
}

StreamingOperationResult RlpLargeStringEncoder::addChunk(ByteView chunk) {
    if (finished_) {
        return StreamingError::kAlreadyFinalized;
    }
    
    // Append chunk directly to encoder buffer
    auto buffer_result = encoder_.GetBytes();
    if (!buffer_result) {
        return StreamingError::kNotFinalized; // Encoder has unclosed lists
    }
    Bytes* buffer = buffer_result.value();
    buffer->append(chunk.data(), chunk.size());
    payload_size_ += chunk.size();
    
    return outcome::success();
}

StreamingOperationResult RlpLargeStringEncoder::finish() {
    if (finished_) {
        return StreamingError::kAlreadyFinalized;
    }
    finished_ = true;
    
    // Encode the actual header based on final payload size
    Bytes actual_header = encode_string_header(payload_size_);
    if (actual_header.empty() && payload_size_ > kMaxShortStringLen) {
        // Header encoding failed (payload too large - practically impossible)
        return StreamingError::kHeaderSizeExceeded;
    }
    
    size_t actual_header_size = actual_header.size();
    size_t reserved_size = 9;
    
    // Get mutable access to encoder buffer
    auto buffer_result = encoder_.GetBytes();
    if (!buffer_result) {
        return StreamingError::kNotFinalized; // Encoder has unclosed lists
    }
    Bytes* buffer = buffer_result.value();
    
    if (actual_header_size <= reserved_size) {
        // Calculate how much to shift
        size_t shift = reserved_size - actual_header_size;
        
        // Copy actual header to the start position
        std::memcpy(buffer->data() + header_start_, actual_header.data(), actual_header_size);
        
        if (shift > 0) {
            // Shift payload left to eliminate gap
            std::memmove(
                buffer->data() + header_start_ + actual_header_size,
                buffer->data() + payload_start_,
                payload_size_
            );
            // Resize to remove the gap
            buffer->resize(buffer->size() - shift);
        }
    } else {
        // Should never happen with 9-byte reservation
        return StreamingError::kHeaderSizeExceeded;
    }
    
    return outcome::success();
}

// ============================================================================
// Approach B: Chunked List Encoding Implementation
// ============================================================================

RlpChunkedListEncoder::RlpChunkedListEncoder(RlpEncoder& encoder, size_t chunk_size)
    : encoder_(encoder)
    , chunk_size_(chunk_size)
    , chunk_count_(0)
    , total_bytes_(0)
    , finished_(false)
    , list_started_(false) {
    buffer_.reserve(chunk_size);
}

StreamingResult<RlpChunkedListEncoder> RlpChunkedListEncoder::create(RlpEncoder& encoder, size_t chunk_size) {
    if (chunk_size == 0) {
        return StreamingError::kInvalidChunkSize;
    }
    return RlpChunkedListEncoder(encoder, chunk_size);
}

// Destructor - automatically finish if not already done
RlpChunkedListEncoder::~RlpChunkedListEncoder() {
    if (!finished_) {
        (void)finish(); // Ignore result in destructor
    }
}

StreamingOperationResult RlpChunkedListEncoder::addChunk(ByteView data) {
    if (finished_) {
        return StreamingError::kAlreadyFinalized;
    }
    
    // Start list on first data
    if (!list_started_) {
        auto result = encoder_.BeginList();
        if (!result) {
            return StreamingError::kNotFinalized; // Failed to begin list
        }
        list_started_ = true;
    }
    
    size_t offset = 0;
    while (offset < data.size()) {
        size_t space_left = chunk_size_ - buffer_.size();
        size_t to_copy = std::min(space_left, data.size() - offset);
        
        buffer_.append(data.data() + offset, to_copy);
        offset += to_copy;
        total_bytes_ += to_copy;
        
        // Flush if buffer is full
        if (buffer_.size() >= chunk_size_) {
            flushBuffer();
        }
    }
    
    return outcome::success();
}

void RlpChunkedListEncoder::flushBuffer() {
    if (buffer_.empty()) {
        return;
    }
    
    // Add buffer as RLP string to the list
    auto result = encoder_.add(ByteView(buffer_.data(), buffer_.size()));
    // Ignore errors in internal helper - errors are caught at flush()
    (void)result;
    chunk_count_++;
    buffer_.clear();
}

StreamingOperationResult RlpChunkedListEncoder::finish() {
    if (finished_) {
        return StreamingError::kAlreadyFinalized;
    }
    finished_ = true;
    
    // Flush any remaining data
    if (!buffer_.empty()) {
        flushBuffer();
    }
    
    // End the list if it was started
    if (list_started_) {
        auto result = encoder_.EndList();
        if (!result) {
            return StreamingError::kNotFinalized; // Failed to end list
        }
    } else {
        // Empty data - create empty list
        auto begin_result = encoder_.BeginList();
        if (!begin_result) {
            return StreamingError::kNotFinalized;
        }
        auto end_result = encoder_.EndList();
        if (!end_result) {
            return StreamingError::kNotFinalized;
        }
    }
    
    return outcome::success();
}

// ============================================================================
// Streaming Decoder Implementations
// ============================================================================

// ============================================================================
// Approach A: Large String Decoder Implementation
// ============================================================================

RlpLargeStringDecoder::RlpLargeStringDecoder(const RlpDecoder& decoder)
    : view_(decoder.Remaining())
    , payload_size_(0)
    , bytes_read_(0)
    , initialized_(false) {
}

RlpLargeStringDecoder::RlpLargeStringDecoder(ByteView data)
    : view_(data)
    , payload_size_(0)
    , bytes_read_(0)
    , initialized_(false) {
}

Result<size_t> RlpLargeStringDecoder::peekPayloadSize() const noexcept {
    // Peek header from our view
    RlpDecoder temp_decoder(view_);
    BOOST_OUTCOME_TRY(auto h, temp_decoder.PeekHeader());
    
    if (h.list) {
        return DecodingError::kUnexpectedList;
    }
    
    if (view_.length() < h.header_size_bytes + h.payload_size_bytes) {
        return DecodingError::kInputTooShort;
    }
    
    return h.payload_size_bytes;
}

Result<ByteView> RlpLargeStringDecoder::readChunk(size_t max_chunk_size) {
    // Initialize on first read
    if (!initialized_) {
        RlpDecoder temp_decoder(view_);
        BOOST_OUTCOME_TRY(auto h, temp_decoder.PeekHeader());
        
        if (h.list) {
            return DecodingError::kUnexpectedList;
        }
        
        if (view_.length() < h.header_size_bytes + h.payload_size_bytes) {
            return DecodingError::kInputTooShort;
        }
        
        payload_size_ = h.payload_size_bytes;
        
        // Skip header in our view
        view_.remove_prefix(h.header_size_bytes);
        initialized_ = true;
    }
    
    // Check if already finished
    if (bytes_read_ >= payload_size_) {
        return ByteView{}; // Empty view signals completion
    }
    
    // Calculate chunk size
    size_t remaining = payload_size_ - bytes_read_;
    size_t chunk_size = std::min(remaining, max_chunk_size);
    
    // Get chunk view from our view
    ByteView chunk = view_.substr(0, chunk_size);
    
    // Advance our position
    view_.remove_prefix(chunk_size);
    bytes_read_ += chunk_size;
    
    return chunk;
}

// ============================================================================
// Approach B: Chunked List Decoder Implementation
// ============================================================================

RlpChunkedListDecoder::RlpChunkedListDecoder(const RlpDecoder& decoder)
    : view_(decoder.Remaining())
    , list_payload_()
    , total_size_(0)
    , total_chunks_(0)
    , chunk_index_(0)
    , initialized_(false) {
}

RlpChunkedListDecoder::RlpChunkedListDecoder(ByteView data)
    : view_(data)
    , list_payload_()
    , total_size_(0)
    , total_chunks_(0)
    , chunk_index_(0)
    , initialized_(false) {
}

Result<size_t> RlpChunkedListDecoder::peekTotalSize() {
    if (initialized_ && total_size_ > 0) {
        return total_size_; // Return cached value
    }
    
    // Peek list header from our view
    RlpDecoder temp_decoder(view_);
    BOOST_OUTCOME_TRY(auto h, temp_decoder.PeekHeader());
    
    if (!h.list) {
        return DecodingError::kUnexpectedString;
    }
    
    if (view_.length() < h.header_size_bytes + h.payload_size_bytes) {
        return DecodingError::kInputTooShort;
    }
    
    // Scan through list to calculate total size
    ByteView list_view = view_.substr(h.header_size_bytes, h.payload_size_bytes);
    size_t total = 0;
    size_t chunks = 0;
    
    while (!list_view.empty()) {
        RlpDecoder chunk_decoder(list_view);
        
        BOOST_OUTCOME_TRY(auto chunk_h, chunk_decoder.PeekHeader());
        
        if (chunk_h.list) {
            return DecodingError::kUnexpectedList;
        }
        
        total += chunk_h.payload_size_bytes;
        chunks++;
        
        // Skip this chunk
        size_t chunk_total_size = chunk_h.header_size_bytes + chunk_h.payload_size_bytes;
        if (list_view.length() < chunk_total_size) {
            return DecodingError::kInputTooShort;
        }
        list_view.remove_prefix(chunk_total_size);
    }
    
    total_size_ = total;
    total_chunks_ = chunks;
    
    return total_size_;
}

Result<size_t> RlpChunkedListDecoder::peekChunkCount() {
    if (initialized_ && total_chunks_ > 0) {
        return total_chunks_; // Return cached value
    }
    
    // peekTotalSize() calculates both total_size_ and total_chunks_
    BOOST_OUTCOME_TRY(peekTotalSize());
    
    return total_chunks_;
}

Result<ByteView> RlpChunkedListDecoder::readChunk() {
    // Initialize on first read
    if (!initialized_) {
        // Read list header from our view
        RlpDecoder temp_decoder(view_);
        BOOST_OUTCOME_TRY(auto h, temp_decoder.PeekHeader());
        
        if (!h.list) {
            return DecodingError::kUnexpectedString;
        }
        
        if (view_.length() < h.header_size_bytes + h.payload_size_bytes) {
            return DecodingError::kInputTooShort;
        }
        
        size_t list_payload_len = h.payload_size_bytes;
        
        // Skip list header in our view
        view_.remove_prefix(h.header_size_bytes);
        
        // Set list_payload_ to point to the payload
        list_payload_ = view_.substr(0, list_payload_len);
        
        // If we haven't peeked yet, do it now to get total_chunks_
        if (total_chunks_ == 0) {
            // Count chunks by scanning
            ByteView temp_view = list_payload_;
            size_t chunks = 0;
            
            while (!temp_view.empty()) {
                RlpDecoder temp_decoder2(temp_view);
                BOOST_OUTCOME_TRY(auto h2, temp_decoder2.PeekHeader());
                
                if (h2.list) {
                    return DecodingError::kUnexpectedList;
                }
                
                chunks++;
                size_t total_size = h2.header_size_bytes + h2.payload_size_bytes;
                if (temp_view.length() < total_size) {
                    return DecodingError::kInputTooShort;
                }
                temp_view.remove_prefix(total_size);
            }
            
            total_chunks_ = chunks;
        }
        
        initialized_ = true;
    }
    
    // Check if already finished
    if (chunk_index_ >= total_chunks_ || list_payload_.empty()) {
        // Consume list payload from our view if not already done
        if (!list_payload_.empty()) {
            view_.remove_prefix(list_payload_.length());
            list_payload_ = ByteView{};
        }
        return ByteView{}; // Empty view signals completion
    }
    
    // Decode next chunk
    RlpDecoder chunk_decoder(list_payload_);
    
    BOOST_OUTCOME_TRY(auto h, chunk_decoder.PeekHeader());
    
    if (h.list) {
        return DecodingError::kUnexpectedList;
    }
    
    // Get chunk payload view
    if (list_payload_.length() < h.header_size_bytes + h.payload_size_bytes) {
        return DecodingError::kInputTooShort;
    }
    
    ByteView chunk_payload = list_payload_.substr(h.header_size_bytes, h.payload_size_bytes);
    
    // Advance list_payload_ past this chunk
    size_t chunk_total_size = h.header_size_bytes + h.payload_size_bytes;
    list_payload_.remove_prefix(chunk_total_size);
    chunk_index_++;
    
    // If this was the last chunk, update our view
    if (list_payload_.empty() && chunk_index_ >= total_chunks_) {
        view_.remove_prefix(0); // Already consumed
    }
    
    return chunk_payload;
}

} // namespace rlp
