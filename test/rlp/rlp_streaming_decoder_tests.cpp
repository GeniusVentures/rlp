#include <gtest/gtest.h>
#include <rlp/rlp_streaming.hpp>
#include <vector>
#include <array>

using namespace rlp;

// ============================================================================
// Tests for RlpLargeStringDecoder (Two-Phase Approach A)
// ============================================================================

TEST(RLPStreamingDecoderTests, LargeStringTwoPhaseBasic) {
    // Phase 1: Encode large data
    std::vector<uint8_t> original_data(10000);
    for (size_t i = 0; i < original_data.size(); ++i) {
        original_data[i] = static_cast<uint8_t>((i * 13) % 256);
    }
    
    RlpEncoder encoder;
    encoder.add(ByteView(original_data.data(), original_data.size()));
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    // Phase 2: Decode with two-phase streaming decoder
    RlpDecoder decoder(encoded);
    RlpLargeStringDecoder stream_decoder(decoder);
    
    // Step 1: Peek size and allocate buffer
    auto size_result = stream_decoder.peekPayloadSize();
    ASSERT_TRUE(size_result);
    EXPECT_EQ(original_data.size(), size_result.value());
    
    // Pre-allocate buffer based on peeked size
    std::vector<uint8_t> decoded_data;
    decoded_data.reserve(size_result.value());
    
    // Step 2: Read chunks and fill buffer
    while (!stream_decoder.isFinished()) {
        auto chunk_result = stream_decoder.readChunk(1024);
        ASSERT_TRUE(chunk_result);
        
        ByteView chunk = chunk_result.value();
        if (chunk.empty()) break; // Finished
        
        decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
    }
    
    EXPECT_EQ(original_data, decoded_data);
    EXPECT_TRUE(stream_decoder.isFinished());
    EXPECT_EQ(stream_decoder.currentPosition(), stream_decoder.totalSize());
}

TEST(RLPStreamingDecoderTests, LargeStringTwoPhaseVeryLarge) {
    // Test with > 55 bytes (requires long string encoding)
    std::vector<uint8_t> original_data(100000); // 100KB
    for (size_t i = 0; i < original_data.size(); ++i) {
        original_data[i] = static_cast<uint8_t>(i & 0xFF);
    }
    
    RlpEncoder encoder;
    encoder.add(ByteView(original_data.data(), original_data.size()));
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    RlpLargeStringDecoder stream_decoder(decoder);
    
    // Peek size first
    auto size_result = stream_decoder.peekPayloadSize();
    ASSERT_TRUE(size_result);
    EXPECT_EQ(100000u, size_result.value());
    
    // Decode in 8KB chunks
    std::vector<uint8_t> decoded_data;
    decoded_data.reserve(size_result.value());
    
    size_t chunks_read = 0;
    while (!stream_decoder.isFinished()) {
        auto chunk_result = stream_decoder.readChunk(8192);
        ASSERT_TRUE(chunk_result);
        
        ByteView chunk = chunk_result.value();
        if (chunk.empty()) break;
        
        decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
        chunks_read++;
    }
    
    EXPECT_EQ(original_data, decoded_data);
    EXPECT_GT(chunks_read, 10u); // Should have read multiple chunks
}

TEST(RLPStreamingDecoderTests, LargeStringTwoPhaseSmallData) {
    // Test with small data (< 55 bytes, short string encoding)
    std::vector<uint8_t> original_data = {1, 2, 3, 4, 5};
    
    RlpEncoder encoder;
    encoder.add(ByteView(original_data.data(), original_data.size()));
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    RlpLargeStringDecoder stream_decoder(decoder);
    
    auto size_result = stream_decoder.peekPayloadSize();
    ASSERT_TRUE(size_result);
    EXPECT_EQ(5u, size_result.value());
    
    std::vector<uint8_t> decoded_data;
    auto chunk_result = stream_decoder.readChunk();
    ASSERT_TRUE(chunk_result);
    ByteView chunk = chunk_result.value();
    decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
    
    EXPECT_EQ(original_data, decoded_data);
    EXPECT_TRUE(stream_decoder.isFinished());
}

TEST(RLPStreamingDecoderTests, LargeStringTwoPhaseEmpty) {
    // Test with empty string
    RlpEncoder encoder;
    encoder.add(ByteView());
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    RlpLargeStringDecoder stream_decoder(decoder);
    
    auto size_result = stream_decoder.peekPayloadSize();
    ASSERT_TRUE(size_result);
    EXPECT_EQ(0u, size_result.value());
    
    auto chunk_result = stream_decoder.readChunk();
    ASSERT_TRUE(chunk_result);
    EXPECT_TRUE(chunk_result.value().empty());
    EXPECT_TRUE(stream_decoder.isFinished());
}

TEST(RLPStreamingDecoderTests, LargeStringTwoPhaseErrorOnList) {
    // Test error handling when encountering list instead of string
    RlpEncoder encoder;
    encoder.BeginList();
    encoder.add(uint32_t{42});
    encoder.EndList();
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    RlpLargeStringDecoder stream_decoder(decoder);
    
    auto size_result = stream_decoder.peekPayloadSize();
    EXPECT_FALSE(size_result);
    EXPECT_EQ(size_result.error(), DecodingError::kUnexpectedList);
}

// ============================================================================
// Tests for RlpChunkedListDecoder (Two-Phase Approach B)
// ============================================================================

TEST(RLPStreamingDecoderTests, ChunkedListTwoPhaseBasic) {
    // Encode data as chunked list
    std::vector<uint8_t> original_data(50000); // 50KB
    for (size_t i = 0; i < original_data.size(); ++i) {
        original_data[i] = static_cast<uint8_t>((i * 7) % 256);
    }
    
    RlpEncoder encoder;
    auto encode_result = encodeChunkedList(encoder, [&original_data](std::function<void(ByteView)> callback) {
        for (size_t i = 0; i < original_data.size(); i += 1000) {
            size_t chunk_size = std::min<size_t>(1000, original_data.size() - i);
            callback(ByteView(original_data.data() + i, chunk_size));
        }
    }, 8192);
    
    ASSERT_TRUE(encode_result);
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    // Decode with two-phase streaming decoder
    RlpDecoder decoder(encoded);
    RlpChunkedListDecoder stream_decoder(decoder);
    
    // Step 1: Peek total size and chunk count
    auto total_size_result = stream_decoder.peekTotalSize();
    ASSERT_TRUE(total_size_result);
    EXPECT_EQ(original_data.size(), total_size_result.value());
    
    auto chunk_count_result = stream_decoder.peekChunkCount();
    ASSERT_TRUE(chunk_count_result);
    EXPECT_GT(chunk_count_result.value(), 5u); // Should have multiple chunks
    
    // Pre-allocate buffer
    std::vector<uint8_t> decoded_data;
    decoded_data.reserve(total_size_result.value());
    
    // Step 2: Read chunks
    size_t chunks_read = 0;
    while (!stream_decoder.isFinished()) {
        auto chunk_result = stream_decoder.readChunk();
        ASSERT_TRUE(chunk_result);
        
        ByteView chunk = chunk_result.value();
        if (chunk.empty()) break;
        
        decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
        chunks_read++;
    }
    
    EXPECT_EQ(original_data, decoded_data);
    EXPECT_EQ(chunks_read, chunk_count_result.value());
    EXPECT_TRUE(stream_decoder.isFinished());
}

TEST(RLPStreamingDecoderTests, ChunkedListTwoPhaseSmall) {
    // Test with small data
    std::vector<uint8_t> original_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    RlpEncoder encoder;
    auto encode_result = encodeChunkedList(encoder, [&original_data](std::function<void(ByteView)> callback) {
        // Split into 3 chunks
        callback(ByteView(original_data.data(), 3));
        callback(ByteView(original_data.data() + 3, 3));
        callback(ByteView(original_data.data() + 6, 4));
    }, 100);
    
    ASSERT_TRUE(encode_result);
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    RlpChunkedListDecoder stream_decoder(decoder);
    
    auto total_size_result = stream_decoder.peekTotalSize();
    ASSERT_TRUE(total_size_result);
    EXPECT_EQ(10u, total_size_result.value());
    
    auto chunk_count_result = stream_decoder.peekChunkCount();
    ASSERT_TRUE(chunk_count_result);
    EXPECT_EQ(3u, chunk_count_result.value());
    
    std::vector<uint8_t> decoded_data;
    while (!stream_decoder.isFinished()) {
        auto chunk_result = stream_decoder.readChunk();
        ASSERT_TRUE(chunk_result);
        
        ByteView chunk = chunk_result.value();
        if (chunk.empty()) break;
        
        decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
    }
    
    EXPECT_EQ(original_data, decoded_data);
}

TEST(RLPStreamingDecoderTests, ChunkedListTwoPhaseEmpty) {
    // Test with empty list
    RlpEncoder encoder;
    encoder.BeginList();
    encoder.EndList();
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    RlpChunkedListDecoder stream_decoder(decoder);
    
    auto total_size_result = stream_decoder.peekTotalSize();
    ASSERT_TRUE(total_size_result);
    EXPECT_EQ(0u, total_size_result.value());
    
    auto chunk_count_result = stream_decoder.peekChunkCount();
    ASSERT_TRUE(chunk_count_result);
    EXPECT_EQ(0u, chunk_count_result.value());
    
    auto chunk_result = stream_decoder.readChunk();
    ASSERT_TRUE(chunk_result);
    EXPECT_TRUE(chunk_result.value().empty());
    EXPECT_TRUE(stream_decoder.isFinished());
}

TEST(RLPStreamingDecoderTests, ChunkedListTwoPhaseErrorOnString) {
    // Test error handling when encountering string instead of list
    RlpEncoder encoder;
    encoder.add(ByteView("test", 4));
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    RlpChunkedListDecoder stream_decoder(decoder);
    
    auto size_result = stream_decoder.peekTotalSize();
    EXPECT_FALSE(size_result);
    EXPECT_EQ(size_result.error(), DecodingError::kUnexpectedString);
}

TEST(RLPStreamingDecoderTests, ChunkedListTwoPhaseErrorOnNestedList) {
    // Test error handling when list contains nested lists
    RlpEncoder encoder;
    encoder.BeginList();
    encoder.BeginList(); // Nested list - should error
    encoder.add(uint32_t{42});
    encoder.EndList();
    encoder.EndList();
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    RlpChunkedListDecoder stream_decoder(decoder);
    
    // peekTotalSize should fail when scanning nested list
    auto size_result = stream_decoder.peekTotalSize();
    EXPECT_FALSE(size_result);
    EXPECT_EQ(size_result.error(), DecodingError::kUnexpectedList);
}

// ============================================================================
// Integration Tests - Round-trip encoding/decoding
// ============================================================================

TEST(RLPStreamingDecoderTests, RoundTripLargeString) {
    // Test complete round-trip: encode → decode → verify
    std::vector<uint8_t> original_data(75000); // 75KB
    for (size_t i = 0; i < original_data.size(); ++i) {
        original_data[i] = static_cast<uint8_t>((i * 19) % 256);
    }
    
    // Encode using streaming encoder
    RlpEncoder encoder;
    auto stream_result = RlpLargeStringEncoder::create(encoder);
    ASSERT_TRUE(stream_result);
    auto stream_encoder = std::move(stream_result.value());
    
    for (size_t i = 0; i < original_data.size(); i += 4096) {
        size_t chunk_size = std::min<size_t>(4096, original_data.size() - i);
        auto write_result = stream_encoder.addChunk(ByteView(original_data.data() + i, chunk_size));
        ASSERT_TRUE(write_result);
    }
    // Automatic finish() via RAII
    
    auto encoded_result = encoder.GetBytes();
    
    ASSERT_TRUE(encoded_result);
    
    ByteView encoded(*encoded_result.value());
    
    // Decode using two-phase streaming decoder
    RlpDecoder decoder(encoded);
    RlpLargeStringDecoder stream_decoder(decoder);
    
    auto size_result = stream_decoder.peekPayloadSize();
    ASSERT_TRUE(size_result);
    EXPECT_EQ(original_data.size(), size_result.value());
    
    std::vector<uint8_t> decoded_data;
    decoded_data.reserve(size_result.value());
    
    while (!stream_decoder.isFinished()) {
        auto chunk_result = stream_decoder.readChunk(4096);
        ASSERT_TRUE(chunk_result);
        
        ByteView chunk = chunk_result.value();
        if (chunk.empty()) break;
        
        decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
    }
    
    EXPECT_EQ(original_data, decoded_data);
}

TEST(RLPStreamingDecoderTests, RoundTripChunkedList) {
    // Test complete round-trip with chunked list
    std::vector<uint8_t> original_data(60000); // 60KB
    for (size_t i = 0; i < original_data.size(); ++i) {
        original_data[i] = static_cast<uint8_t>((i * 23) % 256);
    }
    
    // Encode using chunked list encoder
    RlpEncoder encoder;
    {
        auto chunked_result = RlpChunkedListEncoder::create(encoder, 5000);
        ASSERT_TRUE(chunked_result);
        auto stream_encoder = std::move(chunked_result.value());
        
        for (size_t i = 0; i < original_data.size(); i += 2000) {
            size_t chunk_size = std::min<size_t>(2000, original_data.size() - i);
            auto write_result = stream_encoder.addChunk(ByteView(original_data.data() + i, chunk_size));
            ASSERT_TRUE(write_result);
        }
        // Automatic finish() via RAII
    }
    
    auto encoded_result = encoder.GetBytes();
    
    ASSERT_TRUE(encoded_result);
    
    ByteView encoded(*encoded_result.value());
    
    // Decode using two-phase chunked list decoder
    RlpDecoder decoder(encoded);
    RlpChunkedListDecoder stream_decoder(decoder);
    
    auto total_size_result = stream_decoder.peekTotalSize();
    ASSERT_TRUE(total_size_result);
    EXPECT_EQ(original_data.size(), total_size_result.value());
    
    std::vector<uint8_t> decoded_data;
    decoded_data.reserve(total_size_result.value());
    
    while (!stream_decoder.isFinished()) {
        auto chunk_result = stream_decoder.readChunk();
        ASSERT_TRUE(chunk_result);
        
        ByteView chunk = chunk_result.value();
        if (chunk.empty()) break;
        
        decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
    }
    
    EXPECT_EQ(original_data, decoded_data);
}
