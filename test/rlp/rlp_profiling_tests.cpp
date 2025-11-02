/**
 * RLP Profiling Tests
 * 
 * These tests profile critical paths in RLP encoding/decoding:
 * - Large list encoding
 * - Deeply nested structures
 * - Large string encoding
 * - Mixed workloads
 * 
 * Run with performance monitoring tools:
 * - Linux: perf record ./rlp_profiling_tests
 * - Valgrind: valgrind --tool=callgrind ./rlp_profiling_tests
 * - Built-in timing measurements
 */

#include <gtest/gtest.h>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <chrono>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace rlp;

// Helper to measure execution time
class Timer {
public:
    Timer(const std::string& name) : name_(name) {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    ~Timer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        std::cout << "[PROFILE] " << name_ << ": " 
                  << duration.count() << " μs ("
                  << (duration.count() / 1000.0) << " ms)" << std::endl;
    }
    
private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

// Profile large flat list encoding
TEST(RLPProfiling, LargeFlatListEncoding) {
    const size_t NUM_ELEMENTS = 10000;
    
    std::cout << "\n=== Profiling Large Flat List Encoding ===" << std::endl;
    std::cout << "Number of elements: " << NUM_ELEMENTS << std::endl;
    
    // Encoding phase
    RlpEncoder encoder;
    
    {
        Timer timer("Encode " + std::to_string(NUM_ELEMENTS) + " uint64_t elements");
        
        auto begin_result = encoder.BeginList();
        ASSERT_TRUE(begin_result) << "BeginList failed";
        
        for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
            auto add_result = encoder.add(static_cast<uint64_t>(i));
            ASSERT_TRUE(add_result) << "Add failed at index " << i;
        }
        
        auto end_result = encoder.EndList();
        ASSERT_TRUE(end_result) << "EndList failed";
    }
    
    auto result = encoder.GetBytes();
    ASSERT_TRUE(result) << "GetBytes failed";
    
    const Bytes& encoded = *result.value();
    std::cout << "Encoded size: " << encoded.size() << " bytes" << std::endl;
    std::cout << "Bytes per element: " << (encoded.size() / static_cast<double>(NUM_ELEMENTS)) << std::endl;
    
    // Decoding phase
    {
        Timer timer("Decode " + std::to_string(NUM_ELEMENTS) + " uint64_t elements");
        RlpDecoder decoder(encoded);
        
        auto header_result = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(header_result) << "ReadListHeaderBytes failed";
        
        for (size_t i = 0; i < NUM_ELEMENTS; ++i) {
            uint64_t value;
            auto read_result = decoder.read(value);
            ASSERT_TRUE(read_result) << "Read failed at index " << i;
            EXPECT_EQ(value, i) << "Value mismatch at index " << i;
        }
    }
}

// Profile deeply nested list structures
TEST(RLPProfiling, DeeplyNestedStructures) {
    const size_t NESTING_DEPTH = 100;
    
    std::cout << "\n=== Profiling Deeply Nested Structures ===" << std::endl;
    std::cout << "Nesting depth: " << NESTING_DEPTH << std::endl;
    
    // Encoding phase
    RlpEncoder encoder;
    
    {
        Timer timer("Encode " + std::to_string(NESTING_DEPTH) + " nested lists");
        
        // Create deeply nested structure: [[[...]]]
        for (size_t i = 0; i < NESTING_DEPTH; ++i) {
            auto begin_result = encoder.BeginList();
            ASSERT_TRUE(begin_result) << "BeginList failed at depth " << i;
        }
        
        // Add a value at the innermost level
        auto add_result = encoder.add(uint64_t(42));
        ASSERT_TRUE(add_result) << "Add failed";
        
        // Close all lists
        for (size_t i = 0; i < NESTING_DEPTH; ++i) {
            auto end_result = encoder.EndList();
            ASSERT_TRUE(end_result) << "EndList failed at depth " << i;
        }
    }
    
    auto result = encoder.GetBytes();
    ASSERT_TRUE(result) << "GetBytes failed";
    
    const Bytes& encoded = *result.value();
    std::cout << "Encoded size: " << encoded.size() << " bytes" << std::endl;
    
    // Decoding phase
    {
        Timer timer("Decode " + std::to_string(NESTING_DEPTH) + " nested lists");
        RlpDecoder decoder(encoded);
        
        // Navigate through nested lists
        for (size_t i = 0; i < NESTING_DEPTH; ++i) {
            auto header_result = decoder.ReadListHeaderBytes();
            ASSERT_TRUE(header_result) << "ReadListHeaderBytes failed at depth " << i;
        }
        
        uint64_t value;
        auto read_result = decoder.read(value);
        ASSERT_TRUE(read_result) << "Read failed";
        EXPECT_EQ(value, 42u);
    }
}

// Profile large string encoding
TEST(RLPProfiling, LargeStringEncoding) {
    const size_t STRING_SIZES[] = {1024, 10240, 102400, 1024000}; // 1KB, 10KB, 100KB, 1MB
    
    std::cout << "\n=== Profiling Large String Encoding ===" << std::endl;
    
    for (size_t size : STRING_SIZES) {
        std::cout << "\nString size: " << size << " bytes" << std::endl;
        
        // Create test data
        Bytes test_data(size, uint8_t(0));
        for (size_t i = 0; i < size; ++i) {
            test_data[i] = static_cast<uint8_t>(i & 0xFF);
        }
        
        // Encoding phase
        RlpEncoder encoder;
        
        {
            Timer timer("Encode " + std::to_string(size) + " byte string");
            auto add_result = encoder.add(ByteView(test_data));
            ASSERT_TRUE(add_result) << "Add failed";
        }
        
        auto result = encoder.GetBytes();
        ASSERT_TRUE(result) << "GetBytes failed";
        
        const Bytes& encoded = *result.value();
        std::cout << "Encoded size: " << encoded.size() << " bytes" << std::endl;
        std::cout << "Overhead: " << (encoded.size() - size) << " bytes" << std::endl;
        
        // Decoding phase
        {
            Timer timer("Decode " + std::to_string(size) + " byte string");
            RlpDecoder decoder(encoded);
            
            Bytes decoded;
            auto read_result = decoder.read(decoded);
            ASSERT_TRUE(read_result) << "Read failed";
            EXPECT_EQ(decoded, test_data) << "Data mismatch";
        }
    }
}

// Profile mixed workload (realistic Ethereum block structure)
TEST(RLPProfiling, MixedWorkload) {
    const size_t NUM_TRANSACTIONS = 100;
    
    std::cout << "\n=== Profiling Mixed Workload (Simulated Block) ===" << std::endl;
    std::cout << "Number of transactions: " << NUM_TRANSACTIONS << std::endl;
    
    // Encoding phase - simulate a block with transactions
    RlpEncoder encoder;
    
    {
        Timer timer("Encode block with " + std::to_string(NUM_TRANSACTIONS) + " transactions");
        
        // Block header
        auto begin_result = encoder.BeginList();
        ASSERT_TRUE(begin_result);
        
        // Parent hash (32 bytes)
        Bytes parent_hash(32, uint8_t(0xAA));
        (void)encoder.add(ByteView(parent_hash));
        
        // Uncle hash (32 bytes)
        Bytes uncle_hash(32, uint8_t(0xBB));
        (void)encoder.add(ByteView(uncle_hash));
        
        // Coinbase (20 bytes)
        Bytes coinbase(20, uint8_t(0xCC));
        (void)encoder.add(ByteView(coinbase));
        
        // State root, tx root, receipt root (32 bytes each)
        Bytes root(32, uint8_t(0xDD));
        (void)encoder.add(ByteView(root));
        (void)encoder.add(ByteView(root));
        (void)encoder.add(ByteView(root));
        
        // Block number, gas limit, gas used, timestamp
        (void)encoder.add(uint64_t(1000000));
        (void)encoder.add(uint64_t(8000000));
        (void)encoder.add(uint64_t(7500000));
        (void)encoder.add(uint64_t(1609459200));
        
        // Extra data
        Bytes extra_data(32, uint8_t(0xEE));
        (void)encoder.add(ByteView(extra_data));
        
        // Transactions list
        (void)encoder.BeginList();
        for (size_t i = 0; i < NUM_TRANSACTIONS; ++i) {
            (void)encoder.BeginList();
            
            // Nonce
            (void)encoder.add(uint64_t(i));
            
            // Gas price
            (void)encoder.add(uint64_t(20000000000));
            
            // Gas limit
            (void)encoder.add(uint64_t(21000));
            
            // To address (20 bytes)
            Bytes to_addr(20, static_cast<uint8_t>(i & 0xFF));
            (void)encoder.add(ByteView(to_addr));
            
            // Value
            (void)encoder.add(uint64_t(1000000000000000000));
            
            // Data (varying size)
            Bytes tx_data(i % 256, uint8_t(0));
            (void)encoder.add(ByteView(tx_data));
            
            // v, r, s (signature)
            (void)encoder.add(uint8_t(27));
            Bytes sig(32, uint8_t(0xFF));
            (void)encoder.add(ByteView(sig));
            (void)encoder.add(ByteView(sig));
            
            (void)encoder.EndList();
        }
        (void)encoder.EndList();
        
        (void)encoder.EndList(); // End block
    }
    
    auto result = encoder.GetBytes();
    ASSERT_TRUE(result);
    
    const Bytes& encoded = *result.value();
    std::cout << "Total encoded size: " << encoded.size() << " bytes" << std::endl;
    std::cout << "Average per transaction: " << (encoded.size() / NUM_TRANSACTIONS) << " bytes" << std::endl;
    
    // Decoding phase
    {
        Timer timer("Decode block with " + std::to_string(NUM_TRANSACTIONS) + " transactions");
        RlpDecoder decoder(encoded);
        
        // Read block header
        auto header_result = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(header_result);
        
        // Skip header fields: 6 Bytes, 4 uint64_t, 1 Bytes
        Bytes temp_bytes;
        for (int i = 0; i < 6; ++i) {
            (void)decoder.read(temp_bytes);
        }
        uint64_t temp_uint;
        for (int i = 0; i < 4; ++i) {
            (void)decoder.read(temp_uint);
        }
        (void)decoder.read(temp_bytes); // extra data
        
        // Read transactions list
        auto tx_list_result = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(tx_list_result);
        
        for (size_t i = 0; i < NUM_TRANSACTIONS; ++i) {
            auto tx_header = decoder.ReadListHeaderBytes();
            ASSERT_TRUE(tx_header);
            
            // Skip transaction fields: 3 uint64_t, 1 Bytes, 1 uint64_t, 1 Bytes, 1 uint8_t, 2 Bytes
            (void)decoder.read(temp_uint);  // nonce
            (void)decoder.read(temp_uint);  // gas price
            (void)decoder.read(temp_uint);  // gas limit
            (void)decoder.read(temp_bytes); // to address
            (void)decoder.read(temp_uint);  // value
            (void)decoder.read(temp_bytes); // data
            uint8_t temp_uint8;
            (void)decoder.read(temp_uint8); // v
            (void)decoder.read(temp_bytes); // r
            (void)decoder.read(temp_bytes); // s
        }
    }
}

// Profile repeated encode/decode cycles
TEST(RLPProfiling, RepeatedEncodeDecode) {
    const size_t NUM_ITERATIONS = 10000;
    
    std::cout << "\n=== Profiling Repeated Encode/Decode Cycles ===" << std::endl;
    std::cout << "Number of iterations: " << NUM_ITERATIONS << std::endl;
    
    // Create test data
    Bytes test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    {
        Timer timer("Encode " + std::to_string(NUM_ITERATIONS) + " simple lists");
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            RlpEncoder encoder;
            (void)encoder.BeginList();
            (void)encoder.add(ByteView(test_data));
            (void)encoder.add(uint64_t(i));
            (void)encoder.EndList();
            auto result = encoder.GetBytes();
            ASSERT_TRUE(result);
        }
    }
    
    // Pre-encode once for decode benchmark
    RlpEncoder encoder;
    (void)encoder.BeginList();
    (void)encoder.add(ByteView(test_data));
    (void)encoder.add(uint64_t(42));
    (void)encoder.EndList();
    auto encoded_result = encoder.GetBytes();
    ASSERT_TRUE(encoded_result);
    const Bytes& encoded = *encoded_result.value();
    
    {
        Timer timer("Decode " + std::to_string(NUM_ITERATIONS) + " simple lists");
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            RlpDecoder decoder(encoded);
            auto header = decoder.ReadListHeaderBytes();
            ASSERT_TRUE(header);
            
            Bytes decoded_bytes;
            (void)decoder.read(decoded_bytes);
            
            uint64_t decoded_uint;
            (void)decoder.read(decoded_uint);
        }
    }
}

// Profile memory allocation patterns
TEST(RLPProfiling, MemoryAllocationPatterns) {
    std::cout << "\n=== Profiling Memory Allocation Patterns ===" << std::endl;
    
    const size_t NUM_LISTS = 1000;
    std::vector<Bytes> encoded_data;
    encoded_data.reserve(NUM_LISTS);
    
    {
        Timer timer("Encode " + std::to_string(NUM_LISTS) + " lists (memory allocation)");
        
        for (size_t i = 0; i < NUM_LISTS; ++i) {
            RlpEncoder encoder;
            (void)encoder.BeginList();
            
            // Add varying amounts of data to trigger different allocation patterns
            for (size_t j = 0; j < (i % 10) + 1; ++j) {
                (void)encoder.add(uint64_t(j));
            }
            
            (void)encoder.EndList();
            auto result = encoder.GetBytes();
            ASSERT_TRUE(result);
            encoded_data.push_back(*result.value());
        }
    }
    
    size_t total_size = 0;
    for (const auto& data : encoded_data) {
        total_size += data.size();
    }
    
    std::cout << "Total encoded: " << total_size << " bytes" << std::endl;
    std::cout << "Average per list: " << (total_size / NUM_LISTS) << " bytes" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "\n=====================================" << std::endl;
    std::cout << "RLP Profiling Test Suite" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "\nFor detailed profiling, run with:" << std::endl;
    std::cout << "  Linux: perf record -g ./rlp_profiling_tests" << std::endl;
    std::cout << "         perf report" << std::endl;
    std::cout << "  Valgrind: valgrind --tool=callgrind ./rlp_profiling_tests" << std::endl;
    std::cout << "            kcachegrind callgrind.out.*" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    return RUN_ALL_TESTS();
}
