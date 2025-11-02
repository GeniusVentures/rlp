/**
 * @file rlp_random_tests.cpp
 * @brief Random RLP Test Cases (Ethereum RandomRLPTests equivalent)
 * 
 * This file implements random RLP encoding/decoding tests similar to
 * the Ethereum RandomRLPTests directory. These tests use randomized inputs
 * to find edge cases and ensure robustness.
 * 
 * Based on: https://github.com/ethereum/tests/tree/develop/RLPTests/RandomRLPTests
 */

#include <gtest/gtest.h>
#include "rlp/rlp_encoder.hpp"
#include "rlp/rlp_decoder.hpp"
#include "rlp/common.hpp"
#include "test_helpers.hpp"
#include <random>
#include <variant>
#include <vector>
#include <string>
#include <algorithm>

using namespace rlp;
using namespace rlp::test;

class RandomRlpTest : public ::testing::Test {
protected:
    // Use a fixed seed for reproducible tests
    std::mt19937 rng{42};
    
    void SetUp() override {
        // Reinitialize RNG with fixed seed for each test
        rng.seed(42);
    }

    // Generate random bytes
    Bytes random_bytes(size_t length) {
        Bytes result(length, 0);
        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < length; ++i) {
            result[i] = static_cast<uint8_t>(dist(rng));
        }
        return result;
    }

    // Generate random integer
    template<typename T>
    T random_integer() {
        if constexpr (sizeof(T) == 1) {
            // For 8-bit types, use int distribution and cast
            std::uniform_int_distribution<int> dist(
                std::numeric_limits<T>::min(),
                std::numeric_limits<T>::max()
            );
            return static_cast<T>(dist(rng));
        } else {
            std::uniform_int_distribution<T> dist(
                std::numeric_limits<T>::min(),
                std::numeric_limits<T>::max()
            );
            return dist(rng);
        }
    }

    // Generate random list depth (1-10)
    int random_depth() {
        std::uniform_int_distribution<int> dist(1, 10);
        return dist(rng);
    }

    // Generate random string length (0-1000)
    size_t random_length() {
        std::uniform_int_distribution<size_t> dist(0, 1000);
        return dist(rng);
    }
};

// Test 1: Random byte strings of various lengths
TEST_F(RandomRlpTest, RandomByteStrings) {
    // Test 100 random byte strings
    for (int iteration = 0; iteration < 100; ++iteration) {
        size_t length = random_length();
        Bytes data = random_bytes(length);
        
        // Encode
        RlpEncoder encoder;
        encoder.add(data);
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Decode
        RlpDecoder decoder(encoded);
        Bytes decoded;
        auto result = decoder.read(decoded);
        
        ASSERT_TRUE(result) << "Failed to decode at iteration " << iteration 
                           << " with length " << length;
        EXPECT_EQ(data, decoded) << "Roundtrip failed at iteration " << iteration;
        EXPECT_TRUE(decoder.IsFinished()) << "Decoder not finished at iteration " << iteration;
    }
}

// Test 2: Random integers of various types
TEST_F(RandomRlpTest, RandomIntegers) {
    // Test 100 random integers of each type
    for (int iteration = 0; iteration < 100; ++iteration) {
        // Test uint8_t
        {
            uint8_t value = random_integer<uint8_t>();
            RlpEncoder encoder;
            encoder.add(value);
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto encoded = *encoded_result.value();
            
            RlpDecoder decoder(encoded);
            uint8_t decoded;
            ASSERT_TRUE(decoder.read(decoded));
            EXPECT_EQ(value, decoded);
        }
        
        // Test uint16_t
        {
            uint16_t value = random_integer<uint16_t>();
            RlpEncoder encoder;
            encoder.add(value);
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto encoded = *encoded_result.value();
            
            RlpDecoder decoder(encoded);
            uint16_t decoded;
            ASSERT_TRUE(decoder.read(decoded));
            EXPECT_EQ(value, decoded);
        }
        
        // Test uint32_t
        {
            uint32_t value = random_integer<uint32_t>();
            RlpEncoder encoder;
            encoder.add(value);
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto encoded = *encoded_result.value();
            
            RlpDecoder decoder(encoded);
            uint32_t decoded;
            ASSERT_TRUE(decoder.read(decoded));
            EXPECT_EQ(value, decoded);
        }
        
        // Test uint64_t
        {
            uint64_t value = random_integer<uint64_t>();
            RlpEncoder encoder;
            encoder.add(value);
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto encoded = *encoded_result.value();
            
            RlpDecoder decoder(encoded);
            uint64_t decoded;
            ASSERT_TRUE(decoder.read(decoded));
            EXPECT_EQ(value, decoded);
        }
    }
}

// Test 3: Random lists with random elements
TEST_F(RandomRlpTest, RandomLists) {
    for (int iteration = 0; iteration < 50; ++iteration) {
        RlpEncoder encoder;
        encoder.BeginList();
        
        // Random number of elements (0-20)
        std::uniform_int_distribution<int> count_dist(0, 20);
        int element_count = count_dist(rng);
        
        std::vector<uint32_t> values;
        for (int i = 0; i < element_count; ++i) {
            uint32_t value = random_integer<uint32_t>();
            values.push_back(value);
            encoder.add(value);
        }
        
        encoder.EndList();
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Decode and verify
        RlpDecoder decoder(encoded);
        auto list_header = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(list_header.has_value()) << "Failed to decode list header at iteration " << iteration;
        
        for (int i = 0; i < element_count; ++i) {
            uint32_t decoded;
            ASSERT_TRUE(decoder.read(decoded)) << "Failed to decode element " << i 
                                              << " at iteration " << iteration;
            EXPECT_EQ(values[i], decoded) << "Value mismatch at element " << i;
        }
        
        EXPECT_TRUE(decoder.IsFinished()) << "Decoder not finished at iteration " << iteration;
    }
}

// Test 4: Randomly nested lists
TEST_F(RandomRlpTest, RandomNestedLists) {
    for (int iteration = 0; iteration < 30; ++iteration) {
        int depth = random_depth();
        
        RlpEncoder encoder;
        
        // Create nested structure
        for (int i = 0; i < depth; ++i) {
            encoder.BeginList();
        }
        
        // Add a random value at the deepest level
        uint32_t value = random_integer<uint32_t>();
        encoder.add(value);
        
        // Close all lists
        for (int i = 0; i < depth; ++i) {
            encoder.EndList();
        }
        
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Decode and verify
        RlpDecoder decoder(encoded);
        
        // Navigate through nested lists
        for (int i = 0; i < depth; ++i) {
            auto list_header = decoder.ReadListHeaderBytes();
            ASSERT_TRUE(list_header.has_value()) 
                << "Failed to decode list at depth " << i 
                << " in iteration " << iteration;
        }
        
        // Read the value
        uint32_t decoded;
        ASSERT_TRUE(decoder.read(decoded)) 
            << "Failed to decode value at iteration " << iteration;
        EXPECT_EQ(value, decoded);
        EXPECT_TRUE(decoder.IsFinished());
    }
}

// Test 5: Random mixed-type lists
TEST_F(RandomRlpTest, RandomMixedTypeLists) {
    for (int iteration = 0; iteration < 50; ++iteration) {
        RlpEncoder encoder;
        encoder.BeginList();
        
        // Random number of elements (1-10)
        std::uniform_int_distribution<int> count_dist(1, 10);
        int element_count = count_dist(rng);
        
        std::vector<std::variant<uint8_t, uint32_t, Bytes>> values;
        
        for (int i = 0; i < element_count; ++i) {
            std::uniform_int_distribution<int> type_dist(0, 2);
            int type = type_dist(rng);
            
            if (type == 0) {
                // Add uint8_t
                uint8_t value = random_integer<uint8_t>();
                values.push_back(value);
                encoder.add(value);
            } else if (type == 1) {
                // Add uint32_t
                uint32_t value = random_integer<uint32_t>();
                values.push_back(value);
                encoder.add(value);
            } else {
                // Add bytes
                std::uniform_int_distribution<size_t> len_dist(0, 50);
                size_t length = len_dist(rng);
                Bytes value = random_bytes(length);
                values.push_back(value);
                encoder.add(value);
            }
        }
        
        encoder.EndList();
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Decode and verify
        RlpDecoder decoder(encoded);
        auto list_header = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(list_header.has_value());
        
        for (size_t i = 0; i < values.size(); ++i) {
            if (std::holds_alternative<uint8_t>(values[i])) {
                uint8_t decoded;
                ASSERT_TRUE(decoder.read(decoded));
                EXPECT_EQ(std::get<uint8_t>(values[i]), decoded);
            } else if (std::holds_alternative<uint32_t>(values[i])) {
                uint32_t decoded;
                ASSERT_TRUE(decoder.read(decoded));
                EXPECT_EQ(std::get<uint32_t>(values[i]), decoded);
            } else {
                Bytes decoded;
                ASSERT_TRUE(decoder.read(decoded));
                EXPECT_EQ(std::get<Bytes>(values[i]), decoded);
            }
        }
        
        EXPECT_TRUE(decoder.IsFinished());
    }
}

// Test 6: Random boundary cases (lengths around 55/56 bytes)
TEST_F(RandomRlpTest, RandomBoundaryLengths) {
    // Test strings with lengths around the boundary (53-58 bytes)
    for (size_t length = 53; length <= 58; ++length) {
        for (int iteration = 0; iteration < 10; ++iteration) {
            Bytes data = random_bytes(length);
            
            RlpEncoder encoder;
            encoder.add(data);
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto encoded = *encoded_result.value();
            
            RlpDecoder decoder(encoded);
            Bytes decoded;
            ASSERT_TRUE(decoder.read(decoded)) 
                << "Failed at length " << length 
                << " iteration " << iteration;
            EXPECT_EQ(data, decoded);
            EXPECT_TRUE(decoder.IsFinished());
        }
    }
    
    // Test lists with payload around boundary
    for (int payload_bytes = 53; payload_bytes <= 58; ++payload_bytes) {
        for (int iteration = 0; iteration < 10; ++iteration) {
            RlpEncoder encoder;
            encoder.BeginList();
            
            // Add elements to reach approximately payload_bytes
            int bytes_added = 0;
            std::vector<uint8_t> values;
            while (bytes_added < payload_bytes) {
                uint8_t value = random_integer<uint8_t>();
                values.push_back(value);
                encoder.add(value);
                bytes_added += 1; // Each uint8 takes 1 byte (assuming < 128)
            }
            
            encoder.EndList();
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto encoded = *encoded_result.value();
            
            RlpDecoder decoder(encoded);
            auto list_header = decoder.ReadListHeaderBytes();
            ASSERT_TRUE(list_header.has_value());
            
            for (uint8_t expected : values) {
                uint8_t decoded;
                ASSERT_TRUE(decoder.read(decoded));
                EXPECT_EQ(expected, decoded);
            }
            
            EXPECT_TRUE(decoder.IsFinished());
        }
    }
}

// Test 7: Random empty structures
TEST_F(RandomRlpTest, RandomEmptyStructures) {
    for (int iteration = 0; iteration < 50; ++iteration) {
        std::uniform_int_distribution<int> choice_dist(0, 3);
        int choice = choice_dist(rng);
        
        if (choice == 0) {
            // Empty string
            RlpEncoder encoder;
            encoder.add(Bytes{});
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto& encoded = *encoded_result.value();
            EXPECT_EQ(bytes_to_hex(encoded), "80");
            
            RlpDecoder decoder(encoded);
            Bytes decoded;
            ASSERT_TRUE(decoder.read(decoded));
            EXPECT_TRUE(decoded.empty());
        } else if (choice == 1) {
            // Empty list
            RlpEncoder encoder;
            encoder.BeginList();
            encoder.EndList();
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto& encoded = *encoded_result.value();
            EXPECT_EQ(bytes_to_hex(encoded), "c0");
            
            RlpDecoder decoder(encoded);
            auto list_header = decoder.ReadListHeaderBytes();
            ASSERT_TRUE(list_header.has_value());
            EXPECT_TRUE(decoder.IsFinished());
        } else if (choice == 2) {
            // List with empty strings
            RlpEncoder encoder;
            encoder.BeginList();
            int count = random_depth();
            for (int i = 0; i < count; ++i) {
                encoder.add(Bytes{});
            }
            encoder.EndList();
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto encoded = *encoded_result.value();
            
            RlpDecoder decoder(encoded);
            auto list_header = decoder.ReadListHeaderBytes();
            ASSERT_TRUE(list_header.has_value());
            for (int i = 0; i < count; ++i) {
                Bytes decoded;
                ASSERT_TRUE(decoder.read(decoded));
                EXPECT_TRUE(decoded.empty());
            }
            EXPECT_TRUE(decoder.IsFinished());
        } else {
            // Nested empty lists
            int depth = random_depth();
            RlpEncoder encoder;
            for (int i = 0; i < depth; ++i) {
                encoder.BeginList();
            }
            for (int i = 0; i < depth; ++i) {
                encoder.EndList();
            }
            auto encoded_result = encoder.GetBytes();
            ASSERT_TRUE(encoded_result);
            auto encoded = *encoded_result.value();
            
            RlpDecoder decoder(encoded);
            for (int i = 0; i < depth; ++i) {
                auto list_header = decoder.ReadListHeaderBytes();
                ASSERT_TRUE(list_header.has_value());
            }
            EXPECT_TRUE(decoder.IsFinished());
        }
    }
}

// Test 8: Random large structures (stress test)
TEST_F(RandomRlpTest, RandomLargeStructures) {
    for (int iteration = 0; iteration < 10; ++iteration) {
        RlpEncoder encoder;
        encoder.BeginList();
        
        // Create a large list with 1000 elements
        std::vector<uint16_t> values;
        for (int i = 0; i < 1000; ++i) {
            uint16_t value = random_integer<uint16_t>();
            values.push_back(value);
            encoder.add(value);
        }
        
        encoder.EndList();
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Verify encoding is large
        EXPECT_GT(encoded.size(), 2000); // Should be > 2000 bytes
        
        // Decode and verify
        RlpDecoder decoder(encoded);
        auto list_header = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(list_header.has_value());
        
        for (int i = 0; i < 1000; ++i) {
            uint16_t decoded;
            ASSERT_TRUE(decoder.read(decoded)) << "Failed at element " << i;
            EXPECT_EQ(values[i], decoded);
        }
        
        EXPECT_TRUE(decoder.IsFinished());
    }
}

// Test 9: Random complex nested structures
TEST_F(RandomRlpTest, RandomComplexNested) {
    for (int iteration = 0; iteration < 20; ++iteration) {
        RlpEncoder encoder;
        
        // Create a structure like: [[a, b], [c, [d, e]], f]
        encoder.BeginList();
        
        // First sublist [a, b]
        encoder.BeginList();
        uint8_t a = random_integer<uint8_t>();
        uint8_t b = random_integer<uint8_t>();
        encoder.add(a);
        encoder.add(b);
        encoder.EndList();
        
        // Second sublist [c, [d, e]]
        encoder.BeginList();
        uint8_t c = random_integer<uint8_t>();
        encoder.add(c);
        encoder.BeginList();
        uint8_t d = random_integer<uint8_t>();
        uint8_t e = random_integer<uint8_t>();
        encoder.add(d);
        encoder.add(e);
        encoder.EndList();
        encoder.EndList();
        
        // Single element f
        uint8_t f = random_integer<uint8_t>();
        encoder.add(f);
        
        encoder.EndList();
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Decode and verify structure
        RlpDecoder decoder(encoded);
        
        // Outer list
        auto outer_list = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(outer_list.has_value());
        
        // First sublist [a, b]
        auto list1 = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(list1.has_value());
        uint8_t decoded_a, decoded_b;
        ASSERT_TRUE(decoder.read(decoded_a));
        ASSERT_TRUE(decoder.read(decoded_b));
        EXPECT_EQ(a, decoded_a);
        EXPECT_EQ(b, decoded_b);
        
        // Second sublist [c, [d, e]]
        auto list2 = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(list2.has_value());
        uint8_t decoded_c;
        ASSERT_TRUE(decoder.read(decoded_c));
        EXPECT_EQ(c, decoded_c);
        
        auto list3 = decoder.ReadListHeaderBytes();
        ASSERT_TRUE(list3.has_value());
        uint8_t decoded_d, decoded_e;
        ASSERT_TRUE(decoder.read(decoded_d));
        ASSERT_TRUE(decoder.read(decoded_e));
        EXPECT_EQ(d, decoded_d);
        EXPECT_EQ(e, decoded_e);
        
        // Single element f
        uint8_t decoded_f;
        ASSERT_TRUE(decoder.read(decoded_f));
        EXPECT_EQ(f, decoded_f);
        
        EXPECT_TRUE(decoder.IsFinished());
    }
}

// Test 10: Random arrays
TEST_F(RandomRlpTest, RandomArrays) {
    for (int iteration = 0; iteration < 50; ++iteration) {
        // Random array size (1-32)
        std::uniform_int_distribution<size_t> size_dist(1, 32);
        size_t array_size = size_dist(rng);
        
        // Generate random array
        std::vector<uint8_t> array_data(array_size);
        for (size_t i = 0; i < array_size; ++i) {
            array_data[i] = random_integer<uint8_t>();
        }
        
        // Encode as fixed-size array
        RlpEncoder encoder;
        Bytes bytes_data(array_data.begin(), array_data.end());
        encoder.add(bytes_data);
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Decode and verify
        RlpDecoder decoder(encoded);
        Bytes decoded;
        ASSERT_TRUE(decoder.read(decoded));
        EXPECT_EQ(bytes_data, decoded);
        EXPECT_TRUE(decoder.IsFinished());
    }
}

// Google Test main entry point
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
