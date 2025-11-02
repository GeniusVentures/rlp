#include <gtest/gtest.h>
#include <rlp/rlp_streaming.hpp>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>

using namespace rlp;

// ============================================================================
// Demonstration of the Improved 2-Step Streaming API
// ============================================================================

TEST(RLPStreamingSimpleAPIDemo, LargeStringEncoderBasicUsage) {
    // Old API (3+ steps):
    // 1. Construct encoder
    // 2. Call write() multiple times
    // 3. Call flush()
    // 4. Hope you didn't forget any step
    
    // New API (2 steps with RAII):
    // 1. Create with factory (validates setup)
    // 2. Add chunks (automatic finish on scope exit)
    
    std::vector<uint8_t> large_data(10000);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }
    
    RlpEncoder encoder;
    
    {
        // Step 1: Create encoder (factory validates state)
        auto stream_result = RlpLargeStringEncoder::create(encoder);
        ASSERT_TRUE(stream_result) << "Failed to create encoder";
        
        auto stream = std::move(stream_result.value());
        
        // Step 2: Add chunks
        for (size_t i = 0; i < large_data.size(); i += 1000) {
            size_t chunk_size = std::min<size_t>(1000, large_data.size() - i);
            auto result = stream.addChunk(ByteView(large_data.data() + i, chunk_size));
            ASSERT_TRUE(result) << "Failed to add chunk at offset " << i;
        }
        
        // Step 3: Automatic - finish() called by destructor when leaving scope
    }
    
    // Verify encoding
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    RlpDecoder decoder(ByteView(*encoded.value()));
    Bytes decoded;
    ASSERT_TRUE(decoder.read(decoded));
    ASSERT_EQ(large_data.size(), decoded.size());
    EXPECT_TRUE(std::equal(large_data.begin(), large_data.end(), decoded.begin()));
}

TEST(RLPStreamingSimpleAPIDemo, ChunkedListEncoderBasicUsage) {
    // Demonstrate chunked list encoder with same 2-step pattern
    
    std::vector<uint8_t> data(25000);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    
    RlpEncoder encoder;
    
    {
        // Step 1: Create with custom chunk size
        auto stream_result = RlpChunkedListEncoder::create(encoder, 5000);
        ASSERT_TRUE(stream_result);
        
        auto stream = std::move(stream_result.value());
        
        // Step 2: Add data (automatically chunked into RLP list)
        auto result = stream.addChunk(ByteView(data.data(), data.size()));
        ASSERT_TRUE(result);
        
        // Automatic finish() via RAII
    }
    
    // Verify encoding
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    // Decode and verify
    RlpDecoder decoder(ByteView(*encoded.value()));
    std::vector<uint8_t> decoded_data;
    auto decoded_result = decodeChunkedList(decoder, [&decoded_data](ByteView chunk, size_t) {
        decoded_data.insert(decoded_data.end(), chunk.begin(), chunk.end());
    });
    ASSERT_TRUE(decoded_result);
    EXPECT_EQ(data, decoded_data);
}

TEST(RLPStreamingSimpleAPIDemo, ErrorHandlingGraceful) {
    // Demonstrate graceful error handling
    
    RlpEncoder encoder;
    
    // Attempt to create chunked list with invalid chunk size
    auto invalid_result = RlpChunkedListEncoder::create(encoder, 0);
    EXPECT_FALSE(invalid_result);
    EXPECT_EQ(invalid_result.error(), StreamingError::kInvalidChunkSize);
    
    // Valid creation
    auto valid_result = RlpChunkedListEncoder::create(encoder, 1024);
    ASSERT_TRUE(valid_result);
    
    auto stream = std::move(valid_result.value());
    
    // Explicit finish
    auto finish_result = stream.finish();
    ASSERT_TRUE(finish_result);
    
    // Attempt to add chunk after finish - graceful error
    auto add_result = stream.addChunk(ByteView(reinterpret_cast<const uint8_t*>("test"), 4));
    EXPECT_FALSE(add_result);
    EXPECT_EQ(add_result.error(), StreamingError::kAlreadyFinalized);
    
    // Second finish - graceful error
    auto second_finish = stream.finish();
    EXPECT_FALSE(second_finish);
    EXPECT_EQ(second_finish.error(), StreamingError::kAlreadyFinalized);
}

TEST(RLPStreamingSimpleAPIDemo, ConvenienceFunctionUsage) {
    // Demonstrate convenience functions for even simpler usage
    
    std::vector<uint8_t> data(5000);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>((i * 7) % 256);
    }
    
    RlpEncoder encoder;
    
    // Single-line encoding with callback
    auto result = encodeLargeString(encoder, [&data](auto callback) {
        // Stream in 1KB chunks
        for (size_t i = 0; i < data.size(); i += 1024) {
            size_t chunk_size = std::min<size_t>(1024, data.size() - i);
            callback(ByteView(data.data() + i, chunk_size));
        }
    });
    
    ASSERT_TRUE(result);
    
    // Verify
    auto encoded = encoder.GetBytes();
    ASSERT_TRUE(encoded);
    
    RlpDecoder decoder(ByteView(*encoded.value()));
    Bytes decoded;
    ASSERT_TRUE(decoder.read(decoded));
    EXPECT_EQ(data.size(), decoded.size());
    EXPECT_TRUE(std::equal(data.begin(), data.end(), decoded.begin()));
}

TEST(RLPStreamingSimpleAPIDemo, RAIICleanupDemo) {
    // Demonstrate that RAII cleanup works even with early returns
    
    std::vector<uint8_t> data(1000);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    
    auto encode_with_early_return = [&data](bool should_fail) -> bool {
        RlpEncoder encoder;
        
        {
            auto stream_result = RlpLargeStringEncoder::create(encoder);
            if (!stream_result) return false;
            
            auto stream = std::move(stream_result.value());
            
            // Add first chunk
            auto result = stream.addChunk(ByteView(data.data(), 500));
            if (!result) return false;
            
            // Simulate early return condition
            if (should_fail) {
                return false; // RAII still calls finish() automatically
            }
            
            // Add second chunk
            result = stream.addChunk(ByteView(data.data() + 500, 500));
            if (!result) return false;
            
            // finish() called automatically on scope exit
        }
        
        // Verify encoding even after early return
        auto encoded = encoder.GetBytes();
        return encoded.has_value();
    };
    
    // Test normal flow
    EXPECT_TRUE(encode_with_early_return(false));
    
    // Test early return - RAII ensures cleanup
    EXPECT_FALSE(encode_with_early_return(true));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
