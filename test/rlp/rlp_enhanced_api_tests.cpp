// Enhanced RLP API Tests
// Tests for streaming support, Ethereum-specific types, and advanced features

#include <gtest/gtest.h>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <rlp/common.hpp>
#include <rlp/rlp_streaming.hpp>
#include <vector>
#include <array>

using namespace rlp;

// ============================================================================
// Streaming Tests - Approach A: Large Single String (Header Patching)
// ============================================================================

TEST(RLPEnhancedAPITests, StreamingEncodeLargeString) {
    // Test streaming encoding of large data using Approach A (header patching)
    std::vector<uint8_t> large_data(10000);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }

    RlpEncoder encoder;
    auto encode_result = encodeLargeString(encoder, [&large_data](std::function<void(ByteView)> callback) {
        // Stream in chunks of 1000 bytes
        for (size_t i = 0; i < large_data.size(); i += 1000) {
            size_t chunk_size = std::min<size_t>(1000, large_data.size() - i);
            ByteView chunk(large_data.data() + i, chunk_size);
            callback(chunk);
        }
    });

    EXPECT_TRUE(encode_result);
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());

    // Decode and verify - result is a single RLP string
    RlpDecoder decoder(encoded);
    Bytes decoded;
    auto result = decoder.read(decoded);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(large_data.size(), decoded.size());
    EXPECT_TRUE(std::equal(large_data.begin(), large_data.end(), decoded.begin()));
}

TEST(RLPEnhancedAPITests, StreamingDecodeWithCallback) {
    // Create large encoded data
    std::vector<uint8_t> large_data(5000);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>((i * 3) % 256);
    }

    RlpEncoder encoder;
    encoder.add(ByteView(large_data.data(), large_data.size()));
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());

    // Decode using streaming callback (Approach A)
    RlpDecoder decoder(encoded);
    std::vector<uint8_t> decoded_data;
    
    auto result = decodeLargeString(decoder, [&decoded_data](ByteView chunk) {
        decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
    }, 500); // 500-byte chunks
    
    EXPECT_TRUE(result);
    EXPECT_EQ(large_data, decoded_data);
}

TEST(RLPEnhancedAPITests, StreamingVeryLargePayload) {
    // Test with > 55 bytes (requires long string encoding) using Approach A
    std::vector<uint8_t> large_data(100000); // 100KB
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    RlpEncoder encoder;
    encoder.reserve(large_data.size() + 10); // Pre-allocate
    
    {
        RlpLargeStringEncoder stream(encoder);
        
        // Add data in chunks
        for (size_t i = 0; i < large_data.size(); i += 8192) {
            size_t chunk_size = std::min<size_t>(8192, large_data.size() - i);
            auto write_result = stream.write(ByteView(large_data.data() + i, chunk_size));
            EXPECT_TRUE(write_result);
        }
        
        // Must explicitly flush
        auto flush_result = stream.flush();
        EXPECT_TRUE(flush_result);
    }
    
    auto encoded_result = encoder.GetBytes();
    
    ASSERT_TRUE(encoded_result);
    
    ByteView encoded(*encoded_result.value());

    // Verify decoding
    RlpDecoder decoder(encoded);
    Bytes decoded;
    EXPECT_TRUE(decoder.read(decoded));
    EXPECT_EQ(large_data.size(), decoded.size());
}

// ============================================================================
// Streaming Tests - Approach B: Chunked List Encoding
// ============================================================================

TEST(RLPEnhancedAPITests, ChunkedListEncoding) {
    // Test chunked list encoding (Approach B)
    std::vector<uint8_t> large_data(50000); // 50KB
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>((i * 7) % 256);
    }

    RlpEncoder encoder;
    
    // Encode as chunked list with 8KB chunks
    auto encode_result = encodeChunkedList(encoder, [&large_data](std::function<void(ByteView)> callback) {
        for (size_t i = 0; i < large_data.size(); i += 1000) {
            size_t chunk_size = std::min<size_t>(1000, large_data.size() - i);
            callback(ByteView(large_data.data() + i, chunk_size));
        }
    }, 8192);
    
    EXPECT_TRUE(encode_result);
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());

    // Decode chunked list
    RlpDecoder decoder(encoded);
    auto decoded_result = decodeChunkedListFull(decoder);
    
    EXPECT_TRUE(decoded_result);
    EXPECT_EQ(large_data.size(), decoded_result.value().size());
    EXPECT_EQ(large_data, std::vector<uint8_t>(decoded_result.value().begin(), decoded_result.value().end()));
}

TEST(RLPEnhancedAPITests, ChunkedListDecodeWithCallback) {
    // Test decoding chunked list with callback
    std::vector<uint8_t> data(25000);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }

    RlpEncoder encoder;
    auto create_result = RlpChunkedListEncoder::create(encoder, 5000);
    ASSERT_TRUE(create_result);
    
    auto& chunked = create_result.value();
    auto write_result = chunked.write(ByteView(data.data(), data.size()));
    EXPECT_TRUE(write_result);
    
    auto flush_result = chunked.flush();
    EXPECT_TRUE(flush_result);
    
    auto encoded_result = encoder.GetBytes();
    
    ASSERT_TRUE(encoded_result);
    
    ByteView encoded(*encoded_result.value());

    // Decode with callback
    RlpDecoder decoder(encoded);
    std::vector<uint8_t> reassembled;
    size_t chunk_count = 0;
    
    auto result = decodeChunkedList(decoder, [&reassembled, &chunk_count](ByteView chunk, size_t index) {
        reassembled.insert(reassembled.end(), chunk.begin(), chunk.end());
        chunk_count++;
    });
    
    EXPECT_TRUE(result);
    EXPECT_EQ(data, reassembled);
    EXPECT_GT(chunk_count, 1); // Should have multiple chunks
}

// ============================================================================
// Enhanced List Processing Tests
// ============================================================================
// Note: ReadListItemsWithCallback test removed as readListItems() function
// was part of old streaming API. Manual list iteration should be used instead.

// ============================================================================
// PeekPayload Tests
// ============================================================================

TEST(RLPEnhancedAPITests, PeekPayloadWithoutConsuming) {
    Bytes data = {0x48, 0x65, 0x6c, 0x6c, 0x6f}; // "Hello"
    
    RlpEncoder encoder;
    encoder.add(ByteView(data));
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());

    RlpDecoder decoder(encoded);
    
    // Peek payload multiple times
    auto peek1 = decoder.PeekPayload();
    EXPECT_TRUE(peek1);
    EXPECT_EQ(5, peek1.value().size());
    
    auto peek2 = decoder.PeekPayload();
    EXPECT_TRUE(peek2);
    EXPECT_EQ(5, peek2.value().size());
    
    // Data should not be consumed
    EXPECT_FALSE(decoder.IsFinished());
    
    // Now actually read it
    Bytes decoded_data;
    EXPECT_TRUE(decoder.read(decoded_data));
    EXPECT_EQ(data, decoded_data);
    EXPECT_TRUE(decoder.IsFinished());
}

// ============================================================================
// Size and Reserve Tests
// ============================================================================

TEST(RLPEnhancedAPITests, EncoderSizeAndReserve) {
    RlpEncoder encoder;
    EXPECT_EQ(0, encoder.size());
    
    encoder.reserve(1000);
    
    encoder.add(uint64_t(12345));
    EXPECT_GT(encoder.size(), 0);
    
    size_t size_after_first = encoder.size();
    
    encoder.add(uint64_t(67890));
    EXPECT_GT(encoder.size(), size_after_first);
}

// ============================================================================
// Roundtrip Validation Tests
// ============================================================================
// Ethereum-specific tests removed - see rlp_ethereum_tests.cpp for domain-specific examples

// ============================================================================
// Edge Cases
// ============================================================================

TEST(RLPEnhancedAPITests, LargeStringEmptyData) {
    // Test encoding empty data with large string encoder
    RlpEncoder encoder;
    auto result = encodeLargeString(encoder, [](std::function<void(ByteView)> callback) {
        // No data
    });
    
    EXPECT_TRUE(result);
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    RlpDecoder decoder(encoded);
    Bytes decoded;
    EXPECT_TRUE(decoder.read(decoded));
    EXPECT_EQ(0, decoded.size());
}

TEST(RLPEnhancedAPITests, ChunkedListEmptyData) {
    // Test encoding empty data with chunked list encoder
    RlpEncoder encoder;
    auto result = encodeChunkedList(encoder, [](std::function<void(ByteView)> callback) {
        // No data
    }, 1024);
    
    EXPECT_TRUE(result);
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    ByteView encoded(*encoded_result.value());
    
    // Should produce empty list
    RlpDecoder decoder(encoded);
    auto decoded_result = decodeChunkedListFull(decoder);
    EXPECT_TRUE(decoded_result);
    EXPECT_EQ(0, decoded_result.value().size());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(RLPEnhancedAPITests, DoubleFlushLargeString) {
    // Test that double flush returns error
    RlpEncoder encoder;
    RlpLargeStringEncoder stream(encoder);
    
    auto write_result = stream.write(ByteView(reinterpret_cast<const uint8_t*>("test"), 4));
    EXPECT_TRUE(write_result);
    
    auto flush1 = stream.flush();
    EXPECT_TRUE(flush1);
    
    // Second flush should fail
    auto flush2 = stream.flush();
    EXPECT_FALSE(flush2);
    EXPECT_EQ(flush2.error(), StreamingError::kAlreadyFinalized);
}

TEST(RLPEnhancedAPITests, WriteAfterFlushLargeString) {
    // Test that write after flush returns error
    RlpEncoder encoder;
    RlpLargeStringEncoder stream(encoder);
    
    auto flush_result = stream.flush();
    EXPECT_TRUE(flush_result);
    
    // Write after flush should fail
    auto write_result = stream.write(ByteView(reinterpret_cast<const uint8_t*>("test"), 4));
    EXPECT_FALSE(write_result);
    EXPECT_EQ(write_result.error(), StreamingError::kAlreadyFinalized);
}

TEST(RLPEnhancedAPITests, InvalidChunkSize) {
    // Test that chunk_size=0 returns error
    RlpEncoder encoder;
    auto create_result = RlpChunkedListEncoder::create(encoder, 0);
    EXPECT_FALSE(create_result);
    EXPECT_EQ(create_result.error(), StreamingError::kInvalidChunkSize);
}

TEST(RLPEnhancedAPITests, DoubleFlushChunkedList) {
    // Test that double flush returns error
    RlpEncoder encoder;
    auto create_result = RlpChunkedListEncoder::create(encoder, 1024);
    ASSERT_TRUE(create_result);
    
    auto& chunked = create_result.value();
    auto flush1 = chunked.flush();
    EXPECT_TRUE(flush1);
    
    // Second flush should fail
    auto flush2 = chunked.flush();
    EXPECT_FALSE(flush2);
    EXPECT_EQ(flush2.error(), StreamingError::kAlreadyFinalized);
}

TEST(RLPEnhancedAPITests, WriteAfterFlushChunkedList) {
    // Test that write after flush returns error
    RlpEncoder encoder;
    auto create_result = RlpChunkedListEncoder::create(encoder, 1024);
    ASSERT_TRUE(create_result);
    
    auto& chunked = create_result.value();
    auto flush_result = chunked.flush();
    EXPECT_TRUE(flush_result);
    
    // Write after flush should fail
    auto write_result = chunked.write(ByteView(reinterpret_cast<const uint8_t*>("test"), 4));
    EXPECT_FALSE(write_result);
    EXPECT_EQ(write_result.error(), StreamingError::kAlreadyFinalized);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
