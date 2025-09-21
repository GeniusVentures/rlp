/**
 * @file rlp_ethereum_tests.cpp
 * @brief Official Ethereum RLP Test Vectors
 * 
 * This file implements the official Ethereum RLP test vectors to ensure
 * compatibility with the Ethereum RLP specification. Test vectors are based
 * on the official ethereum/tests repository.
 */

#include <gtest/gtest.h>
#include "rlp_encoder.hpp"
#include "rlp_decoder.hpp"
#include "common.hpp"
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

using namespace rlp;

class EthereumRlpTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Helper function to convert hex string to bytes
    Bytes hex_to_bytes(const std::string& hex) {
        Bytes result;
        if (hex.length() % 2 != 0) {
            throw std::invalid_argument("Hex string must have even length");
        }
        
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byte_string = hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::stoul(byte_string, nullptr, 16));
            result.push_back(byte);
        }
        return result;
    }

    // Helper function to convert bytes to hex string
    std::string bytes_to_hex(const Bytes& bytes) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (uint8_t byte : bytes) {
            ss << std::setw(2) << static_cast<unsigned>(byte);
        }
        return ss.str();
    }

    // Test encoding and decoding roundtrip
    template<typename T>
    void test_roundtrip(const T& value, const std::string& expected_hex = "") {
    RlpEncoder encoder;
    encoder.add(value);
        auto encoded = encoder.get_bytes();
        
        if (!expected_hex.empty()) {
            EXPECT_EQ(bytes_to_hex(encoded), expected_hex) 
                << "Encoding doesn't match expected hex";
        }

        RlpDecoder decoder(encoded);
        T decoded_value;
        auto result = decoder.read(decoded_value);
        EXPECT_TRUE(result) << "Decoding failed";
        EXPECT_EQ(value, decoded_value) << "Roundtrip failed";
    }
};

// Official Ethereum RLP Test Vectors for Strings
TEST_F(EthereumRlpTest, OfficialStringTests) {
    // Empty string
    test_roundtrip(Bytes{}, "80");
    
    // Single byte strings (0x00 to 0x7f are encoded as themselves)
    for (int i = 0; i < 128; ++i) {
        Bytes single_byte = {static_cast<uint8_t>(i)};
        if (i == 0) {
            // Zero byte is encoded as 0x80 (empty string)
            test_roundtrip(single_byte, "00");
        } else {
            // Single bytes 0x01-0x7f are encoded as themselves
            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(2) << i;
            test_roundtrip(single_byte, ss.str());
        }
    }

    // Single byte >= 0x80 should be encoded with length prefix
    test_roundtrip(Bytes{0x80}, "8180");
    test_roundtrip(Bytes{0xff}, "81ff");

    // Short strings (length 2-55)
    test_roundtrip(hex_to_bytes("0102"), "820102");
    test_roundtrip(hex_to_bytes("010203"), "83010203");
    
    // String "dog"
    test_roundtrip(Bytes{'d', 'o', 'g'}, "83646f67");
    
    // Longer string requiring length-of-length encoding
    std::string long_str_55(55, 'a'); // 55 'a' characters
    Bytes long_bytes_55(long_str_55.begin(), long_str_55.end());
    test_roundtrip(long_bytes_55); // Don't check exact hex for very long strings

    std::string long_str_56(56, 'b'); // 56 'b' characters  
    Bytes long_bytes_56(long_str_56.begin(), long_str_56.end());
    test_roundtrip(long_bytes_56);
}

// Official Ethereum RLP Test Vectors for Integers
TEST_F(EthereumRlpTest, OfficialIntegerTests) {
    // Integer 0
    test_roundtrip(uint8_t{0}, "80"); // Empty string encoding for zero
    test_roundtrip(uint32_t{0}, "80");
    test_roundtrip(uint64_t{0}, "80");

    // Small integers (1-127) - encoded as single bytes
    test_roundtrip(uint8_t{1}, "01");
    test_roundtrip(uint8_t{15}, "0f");
    test_roundtrip(uint8_t{127}, "7f");

    // Integers >= 128 - require string encoding
    test_roundtrip(uint8_t{128}, "8180");
    test_roundtrip(uint8_t{255}, "81ff");

    // Multi-byte integers
    test_roundtrip(uint16_t{256}, "820100");      // 0x0100
    test_roundtrip(uint16_t{1024}, "820400");     // 0x0400
    test_roundtrip(uint32_t{1000000}, "830f4240"); // 0x0f4240

    // Large integers
    test_roundtrip(uint64_t{0x123456789abcdef0ULL}, "88123456789abcdef0");
}

// Official Ethereum RLP Test Vectors for Lists
TEST_F(EthereumRlpTest, OfficialListTests) {
    RlpEncoder encoder;
    RlpDecoder decoder(Bytes{});

    // Empty list
    encoder.begin_list();
    encoder.end_list();
    auto empty_list = encoder.get_bytes();
    EXPECT_EQ(bytes_to_hex(empty_list), "c0");
    
    // Reset encoder
    encoder = RlpEncoder{};

    // List with single element [1]
    encoder.begin_list();
    encoder.encode(uint8_t{1});
    encoder.end_list();
    auto single_element = encoder.get_bytes();
    EXPECT_EQ(bytes_to_hex(single_element), "c101");

    // Reset encoder
    encoder = RlpEncoder{};

    // List [1, 2, 3]
    encoder.begin_list();
    encoder.encode(uint8_t{1});
    encoder.encode(uint8_t{2});
    encoder.encode(uint8_t{3});
    encoder.end_list();
    auto three_elements = encoder.get_bytes();
    EXPECT_EQ(bytes_to_hex(three_elements), "c3010203");

    // Reset encoder
    encoder = RlpEncoder{};

    // Nested lists [[]]
    encoder.begin_list();
    encoder.begin_list();
    encoder.end_list();
    encoder.end_list();
    auto nested_empty = encoder.get_bytes();
    EXPECT_EQ(bytes_to_hex(nested_empty), "c1c0");

    // Reset encoder
    encoder = RlpEncoder{};

    // Complex nested structure [[1, 2], [3]]
    encoder.begin_list();
    encoder.begin_list();
    encoder.encode(uint8_t{1});
    encoder.encode(uint8_t{2});
    encoder.end_list();
    encoder.begin_list();
    encoder.encode(uint8_t{3});
    encoder.end_list();
    encoder.end_list();
    auto complex_nested = encoder.get_bytes();
    EXPECT_EQ(bytes_to_hex(complex_nested), "c4c20102c103");
}

// Official Ethereum RLP Test Vectors for Mixed Types
TEST_F(EthereumRlpTest, OfficialMixedTypeTests) {
    RlpEncoder encoder;

    // List with string and integer ["cat", 1]
    encoder.begin_list();
    encoder.encode(Bytes{'c', 'a', 't'});
    encoder.encode(uint8_t{1});
    encoder.end_list();
    auto mixed = encoder.get_bytes();
    EXPECT_EQ(bytes_to_hex(mixed), "c483636174" "01");

    // Reset encoder
    encoder = RlpEncoder{};

    // More complex: ["dog", [1, 2], "cat"]
    encoder.begin_list();
    encoder.encode(Bytes{'d', 'o', 'g'});
    encoder.begin_list();
    encoder.encode(uint8_t{1});
    encoder.encode(uint8_t{2});
    encoder.end_list();
    encoder.encode(Bytes{'c', 'a', 't'});
    encoder.end_list();
    auto very_mixed = encoder.get_bytes();
    EXPECT_EQ(bytes_to_hex(very_mixed), "ca83646f67c2010283636174");
}

// Official Ethereum RLP Test Vectors for Edge Cases
TEST_F(EthereumRlpTest, OfficialEdgeCaseTests) {
    // Test boundary conditions for list lengths

    // List with exactly 55 bytes of payload
    RlpEncoder encoder;
    encoder.begin_list();
    // Add 55 single-byte elements to create exactly 55 bytes payload
    for (int i = 1; i <= 55; ++i) {
        encoder.encode(uint8_t{1});
    }
    encoder.end_list();
    auto list_55 = encoder.get_bytes();
    // Should use short form (0xc0 + length)
    EXPECT_EQ(list_55[0], 0xf7); // 0xc0 + 55

    // Reset encoder
    encoder = RlpEncoder{};

    // List with exactly 56 bytes of payload (triggers long form)
    encoder.begin_list();
    // Add 56 single-byte elements
    for (int i = 1; i <= 56; ++i) {
        encoder.encode(uint8_t{1});
    }
    encoder.end_list();
    auto list_56 = encoder.get_bytes();
    // Should use long form (0xf8 + length_of_length + length)
    EXPECT_EQ(list_56[0], 0xf8); // Long form indicator
    EXPECT_EQ(list_56[1], 0x38); // 56 in hex
}

// Test vectors for specific Ethereum data structures
TEST_F(EthereumRlpTest, EthereumDataStructures) {
    RlpEncoder encoder;

    // Ethereum transaction-like structure
    // [nonce, gasPrice, gasLimit, to, value, data, v, r, s]
    encoder.begin_list();
    encoder.encode(uint64_t{0x09});           // nonce
    encoder.encode(uint64_t{0x4a817c800});    // gasPrice (20 Gwei)
    encoder.encode(uint64_t{0x5208});         // gasLimit (21000)
    encoder.encode(hex_to_bytes("3535353535353535353535353535353535353535")); // to address (20 bytes)
    encoder.encode(uint64_t{0xde0b6b3a7640000}); // value (1 ETH in wei)
    encoder.encode(Bytes{});                  // data (empty)
    encoder.encode(uint8_t{0x1c});           // v
    encoder.encode(hex_to_bytes("2fe7c4a137b47229e5fd0000b5d42d8fe4ce4d8e8ed2e1b8f8e6b1d4c8e9f1f18")); // r (32 bytes)
    encoder.encode(hex_to_bytes("7d8e2e1b8f8e6b1d4c8e9f1f182fe7c4a137b47229e5fd0000b5d42d8fe4ce4d8")); // s (32 bytes)
    encoder.end_list();
    
    auto tx_bytes = encoder.get_bytes();
    
    // Verify we can decode it back
    RlpDecoder decoder(tx_bytes);
    auto list_length_result = decoder.read_list_header();
    EXPECT_TRUE(list_length_result.has_value()) << "Failed to decode transaction list header";

    // Decode each field
    uint64_t nonce, gas_price, gas_limit, value;
    Bytes to_addr, data, r_bytes, s_bytes;
    uint8_t v;

    EXPECT_TRUE(decoder.read(nonce));
    EXPECT_EQ(nonce, 0x09);
    
    EXPECT_TRUE(decoder.read(gas_price));
    EXPECT_EQ(gas_price, 0x4a817c800);
    
    EXPECT_TRUE(decoder.read(gas_limit));
    EXPECT_EQ(gas_limit, 0x5208);
    
    EXPECT_TRUE(decoder.read(to_addr));
    EXPECT_EQ(to_addr.size(), 20);
    
    EXPECT_TRUE(decoder.read(value));
    EXPECT_EQ(value, 0xde0b6b3a7640000);
    
    EXPECT_TRUE(decoder.read(data));
    EXPECT_TRUE(data.empty());
    
    EXPECT_TRUE(decoder.read(v));
    EXPECT_EQ(v, 0x1c);
    
    EXPECT_TRUE(decoder.read(r_bytes));
    EXPECT_EQ(r_bytes.size(), 32);
    
    EXPECT_TRUE(decoder.read(s_bytes));
    EXPECT_EQ(s_bytes.size(), 32);
    
    EXPECT_TRUE(decoder.is_finished());
}

// Test complex nested structures typical in Ethereum
TEST_F(EthereumRlpTest, ComplexNestedStructures) {
    RlpEncoder encoder;

    // Ethereum block header-like structure with nested data
    encoder.begin_list();
    
    // Parent hash (32 bytes)
    encoder.encode(hex_to_bytes("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"));
    
    // Uncle hash (32 bytes)  
    encoder.encode(hex_to_bytes("fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321"));
    
    // Coinbase (20 bytes)
    encoder.encode(hex_to_bytes("abcdefabcdefabcdefabcdefabcdefabcdefabcd"));
    
    // State root (32 bytes)
    encoder.encode(hex_to_bytes("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    
    // Transactions root (32 bytes)
    encoder.encode(hex_to_bytes("fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"));
    
    // Receipts root (32 bytes)
    encoder.encode(hex_to_bytes("1111222233334444555566667777888899990000aaaabbbbccccddddeeeeffff"));
    
    // Logs bloom (256 bytes)
    Bytes bloom(256, 0x00);
    bloom[0] = 0x01;
    bloom[255] = 0xff;
    encoder.encode(bloom);
    
    // Difficulty
    encoder.encode(uint64_t{0x1bc16d674ec80000});
    
    // Block number
    encoder.encode(uint64_t{0x1b4});
    
    // Gas limit
    encoder.encode(uint64_t{0x1388});
    
    // Gas used
    encoder.encode(uint64_t{0x0});
    
    // Timestamp
    encoder.encode(uint64_t{0x54e34e8e});
    
    // Extra data
    encoder.encode(Bytes{'G', 'e', 't', 'h'});
    
    // Mix hash (32 bytes)
    encoder.encode(hex_to_bytes("0000000000000000000000000000000000000000000000000000000000000000"));
    
    // Nonce (8 bytes)
    encoder.encode(uint64_t{0x13218f1238912389});
    
    encoder.end_list();
    
    auto block_header = encoder.get_bytes();
    
    // Verify we can decode the complex structure
    RlpDecoder decoder(block_header);
    auto header_list_result = decoder.read_list_header();
    EXPECT_TRUE(header_list_result.has_value());
    
    // Verify all fields can be read back
    Bytes parent_hash, uncle_hash, coinbase, state_root, tx_root, receipts_root;
    Bytes logs_bloom, mix_hash, extra_data;
    uint64_t difficulty, block_num, gas_limit, gas_used, timestamp, nonce;
    
    EXPECT_TRUE(decoder.read(parent_hash));
    EXPECT_EQ(parent_hash.size(), 32);
    
    EXPECT_TRUE(decoder.read(uncle_hash));
    EXPECT_EQ(uncle_hash.size(), 32);
    
    EXPECT_TRUE(decoder.read(coinbase));
    EXPECT_EQ(coinbase.size(), 20);
    
    EXPECT_TRUE(decoder.read(state_root));
    EXPECT_EQ(state_root.size(), 32);
    
    EXPECT_TRUE(decoder.read(tx_root));
    EXPECT_EQ(tx_root.size(), 32);
    
    EXPECT_TRUE(decoder.read(receipts_root));
    EXPECT_EQ(receipts_root.size(), 32);
    
    EXPECT_TRUE(decoder.read(logs_bloom));
    EXPECT_EQ(logs_bloom.size(), 256);
    EXPECT_EQ(logs_bloom[0], 0x01);
    EXPECT_EQ(logs_bloom[255], 0xff);
    
    EXPECT_TRUE(decoder.read(difficulty));
    EXPECT_TRUE(decoder.read(block_num));
    EXPECT_TRUE(decoder.read(gas_limit));
    EXPECT_TRUE(decoder.read(gas_used));
    EXPECT_TRUE(decoder.read(timestamp));
    
    EXPECT_TRUE(decoder.read(extra_data));
    EXPECT_EQ(extra_data.size(), 4);
    
    EXPECT_TRUE(decoder.read(mix_hash));
    EXPECT_EQ(mix_hash.size(), 32);
    
    EXPECT_TRUE(decoder.read(nonce));
    
    EXPECT_TRUE(decoder.is_finished());
}

// Test deeply nested structures
TEST_F(EthereumRlpTest, DeeplyNestedStructures) {
    RlpEncoder encoder;
    
    // Create a deeply nested structure: [[[[[1]]]]]
    const int depth = 10;
    
    for (int i = 0; i < depth; ++i) {
        encoder.begin_list();
    }
    
    encoder.encode(uint8_t{42});
    
    for (int i = 0; i < depth; ++i) {
        encoder.end_list();
    }
    
    auto nested = encoder.get_bytes();
    
    // Verify decoding
    RlpDecoder decoder(nested);
    
    // Navigate through all the nested lists
    for (int i = 0; i < depth; ++i) {
        auto list_result = decoder.read_list_header();
        EXPECT_TRUE(list_result.has_value()) << "Failed at depth " << i;
    }
    
    uint8_t value;
    EXPECT_TRUE(decoder.read(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(decoder.is_finished());
}

// Test arrays and vectors
TEST_F(EthereumRlpTest, ArraysAndVectors) {
    // Test std::vector encoding/decoding
    std::vector<uint32_t> vec = {1, 2, 3, 4, 5};
    
    RlpEncoder encoder;
    encoder.encode_vector(vec);
    auto encoded_vec = encoder.get_bytes();
    
    RlpDecoder decoder(encoded_vec);
    std::vector<uint32_t> decoded_vec;
    EXPECT_TRUE(decoder.read_vector(decoded_vec));
    EXPECT_EQ(vec, decoded_vec);
    
    // Test fixed-size array
    std::array<uint8_t, 4> arr = {0xde, 0xad, 0xbe, 0xef};
    
    encoder = RlpEncoder{};
    encoder.encode(arr);
    auto encoded_arr = encoder.get_bytes();
    
    decoder = RlpDecoder{encoded_arr};
    std::array<uint8_t, 4> decoded_arr;
    EXPECT_TRUE(decoder.read(decoded_arr));
    EXPECT_EQ(arr, decoded_arr);
}

// Test boolean encoding (Ethereum-specific: false=0x80, true=0x01)
TEST_F(EthereumRlpTest, BooleanEncoding) {
    // Test false
    test_roundtrip(false, "80"); // Empty string encoding
    
    // Test true  
    test_roundtrip(true, "01"); // Single byte 0x01
}

// Test large data structures
TEST_F(EthereumRlpTest, LargeDataStructures) {
    // Create a large byte array (larger than 55 bytes)
    Bytes large_data(1000, 0xaa);
    test_roundtrip(large_data);
    
    // Create a large list
    RlpEncoder encoder;
    encoder.begin_list();
    for (int i = 0; i < 100; ++i) {
        encoder.encode(uint32_t{static_cast<uint32_t>(i)});
    }
    encoder.end_list();
    
    auto large_list = encoder.get_bytes();
    
    // Decode and verify
    RlpDecoder decoder(large_list);
    auto list_result = decoder.read_list_header();
    EXPECT_TRUE(list_result.has_value());
    
    for (int i = 0; i < 100; ++i) {
        uint32_t value;
        EXPECT_TRUE(decoder.read(value));
        EXPECT_EQ(value, static_cast<uint32_t>(i));
    }
    
    EXPECT_TRUE(decoder.is_finished());
}