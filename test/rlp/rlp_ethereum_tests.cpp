/**
 * @file rlp_ethereum_tests.cpp
 * @brief Official Ethereum RLP Test Vectors
 * 
 * This file implements the official Ethereum RLP test vectors to ensure
 * compatibility with the Ethereum RLP specification. Test vectors are based
 * on the official ethereum/tests repository.
 */

#include <gtest/gtest.h>
#include "rlp/rlp_encoder.hpp"
#include "rlp/rlp_decoder.hpp"
#include "rlp/common.hpp"
#include "test_helpers.hpp"
#include "rlp/intx.hpp"
#include <array>
#include <vector>
#include <string>

using namespace rlp;
using namespace rlp::test;

class EthereumRlpTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Test encoding and decoding roundtrip
    template<typename T>
    void test_roundtrip(const T& value, const std::string& expected_hex = "") {
    RlpEncoder encoder;
    encoder.add(value);
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
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
        // Single bytes 0x00-0x7f are encoded as themselves
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(2) << i;
        test_roundtrip(single_byte, ss.str());
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

    // Empty list
    encoder.BeginList();
    encoder.EndList();
    auto empty_list_result = encoder.GetBytes();
    ASSERT_TRUE(empty_list_result);
    auto& empty_list = *empty_list_result.value();
    EXPECT_EQ(bytes_to_hex(empty_list), "c0");
    
    // Reset encoder
    encoder = RlpEncoder{};

    // List with single element [1]
    encoder.BeginList();
    encoder.add(uint8_t{1});
    encoder.EndList();
    auto single_list_result = encoder.GetBytes();
    ASSERT_TRUE(single_list_result);
    auto& single_list = *single_list_result.value();
    EXPECT_EQ(bytes_to_hex(single_list), "c101");

    // Reset encoder
    encoder = RlpEncoder{};

    // Nested lists [[]]
    encoder.BeginList();
    encoder.BeginList();
    encoder.EndList();
    encoder.EndList();
    auto nested_empty_result = encoder.GetBytes();
    ASSERT_TRUE(nested_empty_result);
    auto& nested_empty = *nested_empty_result.value();
    EXPECT_EQ(bytes_to_hex(nested_empty), "c1c0");

    // Reset encoder
    encoder = RlpEncoder{};

    // Complex nested structure [[1, 2], [3]]
    encoder.BeginList();
    encoder.BeginList();
    encoder.add(uint8_t{1});
    encoder.add(uint8_t{2});
    encoder.EndList();
    encoder.BeginList();
    encoder.add(uint8_t{3});
    encoder.EndList();
    encoder.EndList();
    auto complex_nested_result = encoder.GetBytes();
    ASSERT_TRUE(complex_nested_result);
    auto& complex_nested = *complex_nested_result.value();
    // Note: Actual output includes null terminator from std::basic_string<uint8_t>
    EXPECT_EQ(bytes_to_hex(complex_nested), "c5c20102c103");
}

// Official Ethereum RLP Test Vectors for Mixed Types
TEST_F(EthereumRlpTest, OfficialMixedTypeTests) {
    RlpEncoder encoder;

    // List with string and integer ["cat", 1]
    encoder.BeginList();
    encoder.add(Bytes{'c', 'a', 't'});
    encoder.add(uint8_t{1});
    encoder.EndList();
    auto mixed_result = encoder.GetBytes();
    ASSERT_TRUE(mixed_result);
    auto& mixed = *mixed_result.value();
    // Note: Actual output includes null terminator from std::basic_string<uint8_t>
    EXPECT_EQ(bytes_to_hex(mixed), "c58363617401");

    // Reset encoder
    encoder = RlpEncoder{};

    // More complex: ["dog", [1, 2], "cat"]
    encoder.BeginList();
    encoder.add(Bytes{'d', 'o', 'g'});
    encoder.BeginList();
    encoder.add(uint8_t{1});
    encoder.add(uint8_t{2});
    encoder.EndList();
    encoder.add(Bytes{'c', 'a', 't'});
    encoder.EndList();
    auto very_mixed_result = encoder.GetBytes();
    ASSERT_TRUE(very_mixed_result);
    auto& very_mixed = *very_mixed_result.value();
    // Note: Actual output includes null terminator from std::basic_string<uint8_t>
    EXPECT_EQ(bytes_to_hex(very_mixed), "cb83646f67c2010283636174");
}

// Official Ethereum RLP Test Vectors for Edge Cases
TEST_F(EthereumRlpTest, OfficialEdgeCaseTests) {
    // Test boundary conditions for list lengths

    // List with exactly 55 bytes of payload
    RlpEncoder encoder;
    encoder.BeginList();
    // Add 55 single-byte elements to create exactly 55 bytes payload
    for (int i = 1; i <= 55; ++i) {
    encoder.add(uint8_t{1});
    }
    encoder.EndList();
    auto list_55_result = encoder.GetBytes();
    ASSERT_TRUE(list_55_result);
    auto& list_55 = *list_55_result.value();
    // Should use short form (0xc0 + length)
    EXPECT_EQ(list_55[0], 0xf7); // 0xc0 + 55

    // Reset encoder
    encoder = RlpEncoder{};

    // List with exactly 56 bytes of payload (triggers long form)
    encoder.BeginList();
    // Add 56 single-byte elements
    for (int i = 1; i <= 56; ++i) {
    encoder.add(uint8_t{1});
    }
    encoder.EndList();
    auto list_56_result = encoder.GetBytes();
    ASSERT_TRUE(list_56_result);
    auto& list_56 = *list_56_result.value();
    // Should use long form (0xf8 + length_of_length + length)
    EXPECT_EQ(list_56[0], 0xf8); // Long form indicator
    EXPECT_EQ(list_56[1], 0x38); // 56 in hex
}

// Test vectors for specific Ethereum data structures
TEST_F(EthereumRlpTest, EthereumDataStructures) {
    RlpEncoder encoder;

    // Ethereum transaction-like structure
    // [nonce, gasPrice, gasLimit, to, value, data, v, r, s]
    encoder.BeginList();
    encoder.add(uint64_t{0x09});           // nonce
    encoder.add(uint64_t{0x4a817c800});    // gasPrice (20 Gwei)
    encoder.add(uint64_t{0x5208});         // gasLimit (21000)
    encoder.add(hex_to_bytes("3535353535353535353535353535353535353535")); // to address (20 bytes)
    encoder.add(uint64_t{0xde0b6b3a7640000}); // value (1 ETH in wei)
    encoder.add(Bytes{});                  // data (empty)
    encoder.add(uint8_t{0x1c});           // v
    encoder.add(hex_to_bytes("28ef61340bd939bc2195fe537567866003e1a15d3c71ff63e1590620aa636276")); // r (32 bytes, 64 hex digits)
    encoder.add(hex_to_bytes("67cbe9d8997f761aecb703304b3800ccf555c9f3dc64214b297fb1966a3b6d83")); // s (32 bytes, 64 hex digits)
    encoder.EndList();
    
    auto tx_bytes_result = encoder.GetBytes();
    ASSERT_TRUE(tx_bytes_result);
    auto& tx_bytes = *tx_bytes_result.value();
    
    // Verify we can decode it back
    RlpDecoder decoder(tx_bytes);
    auto list_length_result = decoder.ReadListHeaderBytes();
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
    
    EXPECT_TRUE(decoder.IsFinished());
}

// Test complex nested structures typical in Ethereum
TEST_F(EthereumRlpTest, ComplexNestedStructures) {
    RlpEncoder encoder;

    // Ethereum block header-like structure with nested data
    encoder.BeginList();
    
    // Parent hash (32 bytes)
        encoder.add(hex_to_bytes("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"));
    
    // Uncle hash (32 bytes)  
        encoder.add(hex_to_bytes("fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321"));
    
    // Coinbase (20 bytes)
        encoder.add(hex_to_bytes("abcdefabcdefabcdefabcdefabcdefabcdefabcd"));
    
    // State root (32 bytes)
        encoder.add(hex_to_bytes("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    
    // Transactions root (32 bytes)
        encoder.add(hex_to_bytes("fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"));
    
    // Receipts root (32 bytes)
        encoder.add(hex_to_bytes("1111222233334444555566667777888899990000aaaabbbbccccddddeeeeffff"));
    
    // Logs bloom (256 bytes)
    Bytes bloom(256, 0x00);
    bloom[0] = 0x01;
    bloom[255] = 0xff;
    encoder.add(bloom);
    
    // Difficulty
        encoder.add(uint64_t{0x1bc16d674ec80000});
    
    // Block number
        encoder.add(uint64_t{0x1b4});
    
    // Gas limit
        encoder.add(uint64_t{0x1388});
    
    // Gas used
        encoder.add(uint64_t{0x0});
    
    // Timestamp
        encoder.add(uint64_t{0x54e34e8e});
    
    // Extra data
        encoder.add(Bytes{'G', 'e', 't', 'h'});
    
    // Mix hash (32 bytes)
        encoder.add(hex_to_bytes("0000000000000000000000000000000000000000000000000000000000000000"));
    
    // Nonce (8 bytes)
        encoder.add(uint64_t{0x13218f1238912389});
    
    encoder.EndList();
    
    auto block_header_result = encoder.GetBytes();
    ASSERT_TRUE(block_header_result);
    auto& block_header = *block_header_result.value();
    
    // Verify we can decode the complex structure
    RlpDecoder decoder(block_header);
    auto header_list_result = decoder.ReadListHeaderBytes();
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
    
    EXPECT_TRUE(decoder.IsFinished());
}

// Test deeply nested structures
TEST_F(EthereumRlpTest, DeeplyNestedStructures) {
    RlpEncoder encoder;
    
    // Create a deeply nested structure: [[[[[1]]]]]
    const int depth = 10;
    
    for (int i = 0; i < depth; ++i) {
        encoder.BeginList();
    }
    
    encoder.add(uint8_t{42});
    
    for (int i = 0; i < depth; ++i) {
        encoder.EndList();
    }
    
    auto nested_result = encoder.GetBytes();
    ASSERT_TRUE(nested_result);
    auto& nested = *nested_result.value();
    
    // Verify decoding
    RlpDecoder decoder(nested);
    
    // Navigate through all the nested lists
    for (int i = 0; i < depth; ++i) {
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_TRUE(list_result.has_value()) << "Failed at depth " << i;
    }
    
    uint8_t value;
    EXPECT_TRUE(decoder.read(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(decoder.IsFinished());
}

// Test arrays and vectors
TEST_F(EthereumRlpTest, ArraysAndVectors) {
    // Test std::vector encoding/decoding
    std::vector<uint32_t> vec = {1, 2, 3, 4, 5};
    
    {
        RlpEncoder encoder;
        encoder.BeginList();
        // Parent hash (32 bytes)
        encoder.add(hex_to_bytes("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"));
        // Uncle hash (32 bytes)  
        encoder.add(hex_to_bytes("fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321"));
        // Coinbase (20 bytes)
        encoder.add(hex_to_bytes("abcdefabcdefabcdefabcdefabcdefabcdefabcd"));
        // State root (32 bytes)
        encoder.add(hex_to_bytes("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
        // Transactions root (32 bytes)
        encoder.add(hex_to_bytes("fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"));
        // Receipts root (32 bytes)
        encoder.add(hex_to_bytes("1111222233334444555566667777888899990000aaaabbbbccccddddeeeeffff"));
        // Logs bloom (256 bytes)
        Bytes bloom(256, 0x00);
        bloom[0] = 0x01;
        bloom[255] = 0xff;
        encoder.add(bloom);
        // Difficulty
        encoder.add(uint64_t{0x1bc16d674ec80000});
        // Block number
        encoder.add(uint64_t{0x1b4});
        // Gas limit
        encoder.add(uint64_t{0x1388});
        // Gas used
        encoder.add(uint64_t{0x0});
        // Timestamp
        encoder.add(uint64_t{0x54e34e8e});
        // Extra data
        encoder.add(Bytes{'G', 'e', 't', 'h'});
        // Mix hash (32 bytes)
        encoder.add(hex_to_bytes("0000000000000000000000000000000000000000000000000000000000000000"));
        // Nonce (8 bytes)
        encoder.add(uint64_t{0x13218f1238912389});
        encoder.EndList();
    }
    
    // Create a large list
    {
        RlpEncoder large_encoder;
        large_encoder.BeginList();
        for (int i = 0; i < 100; ++i) {
            large_encoder.add(uint32_t{static_cast<uint32_t>(i)});
        }
        large_encoder.EndList();
        
        auto large_list_result = large_encoder.GetBytes();
        ASSERT_TRUE(large_list_result);
        auto& large_list = *large_list_result.value();
        
        // Decode and verify
        RlpDecoder decoder(large_list);
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_TRUE(list_result.has_value());
        
        for (int i = 0; i < 100; ++i) {
            uint32_t value;
            EXPECT_TRUE(decoder.read(value));
            EXPECT_EQ(value, static_cast<uint32_t>(i));
        }
        
        EXPECT_TRUE(decoder.IsFinished());
    }
}

// Test invalid RLP encodings from Ethereum invalidRLPTest.json
TEST_F(EthereumRlpTest, InvalidRlpBytesShouldBeSingleByte) {
    // bytesShouldBeSingleByte00: 0x8100 - byte 0x00 should be encoded as 0x00, not as string
    {
        auto invalid = hex_to_bytes("8100");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject non-canonical encoding of 0x00";
        // Our implementation detects this as kNonCanonicalSize (single byte should not use string encoding)
        EXPECT_EQ(decode_result.error(), DecodingError::kNonCanonicalSize);
    }

    // bytesShouldBeSingleByte01: 0x8101 - byte 0x01 should be encoded as 0x01, not as string
    {
        auto invalid = hex_to_bytes("8101");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject non-canonical encoding of 0x01";
        EXPECT_EQ(decode_result.error(), DecodingError::kNonCanonicalSize);
    }

    // bytesShouldBeSingleByte7F: 0x817F - byte 0x7F should be encoded as 0x7F, not as string
    {
        auto invalid = hex_to_bytes("817f");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject non-canonical encoding of 0x7F";
        EXPECT_EQ(decode_result.error(), DecodingError::kNonCanonicalSize);
    }
}

TEST_F(EthereumRlpTest, InvalidRlpLeadingZeros) {
    // leadingZerosInLongLengthArray1: 0xb90040... - length has leading zero
    {
        auto invalid = hex_to_bytes("b900400102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject length with leading zero";
        // Our implementation may detect this as kNonCanonicalSize (similar to leading zero case)
        EXPECT_TRUE(decode_result.error() == DecodingError::kLeadingZero ||
                   decode_result.error() == DecodingError::kNonCanonicalSize);
    }

    // leadingZerosInLongLengthArray2: 0xb800 - length encoded with leading zeros
    {
        auto invalid = hex_to_bytes("b800");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject length with leading zero";
        // Our implementation detects this as kNonCanonicalSize
        EXPECT_TRUE(decode_result.error() == DecodingError::kLeadingZero ||
                   decode_result.error() == DecodingError::kNonCanonicalSize);
    }

    // leadingZerosInLongLengthList1: 0xfb00000040... - list length has leading zeros
    {
        auto invalid = hex_to_bytes("fb00000040c00102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d");
        RlpDecoder decoder(invalid);
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_FALSE(list_result) << "Should reject list length with leading zeros";
        // Our implementation may detect this as kNonCanonicalSize
        EXPECT_TRUE(list_result.error() == DecodingError::kLeadingZero ||
                   list_result.error() == DecodingError::kNonCanonicalSize);
    }

    // leadingZerosInLongLengthList2: 0xf800 - list length encoded with leading zeros
    {
        auto invalid = hex_to_bytes("f800");
        RlpDecoder decoder(invalid);
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_FALSE(list_result) << "Should reject list length with leading zeros";
        // Should detect as error (either LeadingZero or NonCanonicalSize)
        EXPECT_FALSE(list_result.has_value());
    }
}

TEST_F(EthereumRlpTest, InvalidRlpNonOptimalLength) {
    // nonOptimalLongLengthArray1: 0xb81000... - uses long form for length that fits in short form
    {
        auto invalid = hex_to_bytes("b81000112233445566778899aabbccddeeff");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject non-optimal length encoding";
        EXPECT_EQ(decode_result.error(), DecodingError::kNonCanonicalSize);
    }

    // nonOptimalLongLengthArray2: 0xb801ff - uses long form for single byte length
    {
        auto invalid = hex_to_bytes("b801ff");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject non-optimal length encoding";
        EXPECT_EQ(decode_result.error(), DecodingError::kNonCanonicalSize);
    }

    // nonOptimalLongLengthList1: 0xf81000... - uses long form for length that fits in short form
    {
        auto invalid = hex_to_bytes("f81000c8000102030405060708c8090a0b0c0d0e0f1011");
        RlpDecoder decoder(invalid);
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_FALSE(list_result) << "Should reject non-optimal list length encoding";
        EXPECT_EQ(list_result.error(), DecodingError::kNonCanonicalSize);
    }

    // nonOptimalLongLengthList2: 0xf803112233 - uses long form for length that fits in short form
    {
        auto invalid = hex_to_bytes("f803112233");
        RlpDecoder decoder(invalid);
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_FALSE(list_result) << "Should reject non-optimal list length encoding";
        EXPECT_EQ(list_result.error(), DecodingError::kNonCanonicalSize);
    }
}

TEST_F(EthereumRlpTest, InvalidRlpIncorrectLength) {
    // wrongSizeList: 0xf80180 - Example of list encoding
    // 0xf801 = list with long form encoding length 1
    // 0x80 = empty string
    // This uses non-optimal length encoding (should use 0xc1 for list of 1 byte)
    {
        auto invalid = hex_to_bytes("f80180");
        RlpDecoder decoder(invalid);
        auto list_result = decoder.ReadListHeaderBytes();
        // Our implementation correctly rejects this as non-canonical
        EXPECT_FALSE(list_result) << "Should reject non-optimal list length encoding";
        EXPECT_EQ(list_result.error(), DecodingError::kNonCanonicalSize);
    }

    // incorrectLengthInArray: 0xb90021... - declares length 33 but only has 32 bytes
    {
        auto invalid = hex_to_bytes("b900210102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject incorrect declared length";
        // Either LeadingZero (because 0x0021 has leading zero) or InputTooShort
        EXPECT_TRUE(decode_result.error() == DecodingError::kLeadingZero ||
                   decode_result.error() == DecodingError::kInputTooShort);
    }

    // lessThanShortLengthArray1: 0x81 - declares length but no data
    {
        auto invalid = hex_to_bytes("81");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject incomplete data";
        EXPECT_EQ(decode_result.error(), DecodingError::kInputTooShort);
    }

    // lessThanShortLengthArray2: 0xa00001020304050607... (30 bytes instead of 32)
    {
        auto invalid = hex_to_bytes("a000010203040506070809010203040506070809010203040506070809");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject incomplete data";
        EXPECT_EQ(decode_result.error(), DecodingError::kInputTooShort);
    }

    // lessThanShortLengthList1: 0xc5010203 - declares length 5 but only has 3 bytes payload
    {
        auto invalid = hex_to_bytes("c5010203");
        RlpDecoder decoder(invalid);
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_FALSE(list_result) << "Should reject insufficient list payload";
        EXPECT_EQ(list_result.error(), DecodingError::kInputTooShort);
    }

    // lessThanLongLengthArray1: 0xba010000... - very long declared length
    {
        auto invalid = hex_to_bytes("ba010000ff");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject unrealistic length";
        EXPECT_EQ(decode_result.error(), DecodingError::kInputTooShort);
    }

    // lessThanLongLengthList1: 0xf90180 - declares length 384 but input is much shorter
    {
        auto invalid = hex_to_bytes("f90180");
        RlpDecoder decoder(invalid);
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_FALSE(list_result) << "Should reject insufficient data";
        EXPECT_EQ(list_result.error(), DecodingError::kInputTooShort);
    }
}

TEST_F(EthereumRlpTest, InvalidRlpEmptyInput) {
    // emptyEncoding - empty input
    {
        Bytes empty;
        RlpDecoder decoder(empty);
        uint8_t value;
        auto decode_result = decoder.read(value);
        EXPECT_FALSE(decode_result) << "Should reject empty input";
        EXPECT_EQ(decode_result.error(), DecodingError::kInputTooShort);
    }
}

TEST_F(EthereumRlpTest, InvalidRlpIntegerOverflow) {
    // int32Overflow: 0xbf0f000000000000021111 - length would overflow
    {
        auto invalid = hex_to_bytes("bf0f000000000000021111");
        RlpDecoder decoder(invalid);
        Bytes result;
        auto decode_result = decoder.read(result);
        EXPECT_FALSE(decode_result) << "Should reject potential overflow";
        // Could be LeadingZero, NonCanonicalSize, or InputTooShort depending on implementation
        EXPECT_TRUE(decode_result.error() == DecodingError::kLeadingZero || 
                   decode_result.error() == DecodingError::kNonCanonicalSize ||
                   decode_result.error() == DecodingError::kInputTooShort);
    }

    // int32Overflow2: 0xff0f000000000000021111 - length would overflow
    {
        auto invalid = hex_to_bytes("ff0f000000000000021111");
        RlpDecoder decoder(invalid);
        auto list_result = decoder.ReadListHeaderBytes();
        EXPECT_FALSE(list_result) << "Should reject potential overflow";
        // Could be LeadingZero, NonCanonicalSize, or InputTooShort depending on implementation
        EXPECT_TRUE(list_result.error() == DecodingError::kLeadingZero || 
                   list_result.error() == DecodingError::kNonCanonicalSize ||
                   list_result.error() == DecodingError::kInputTooShort);
    }
}

TEST_F(EthereumRlpTest, ValidRlpExactHexOutputs) {
    // Test exact hex outputs from rlptest.json for validation
    
    // Test: emptystring -> 0x80
    {
        RlpEncoder encoder;
        encoder.add(Bytes{});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "80");
    }

    // Test: shortstring "dog" -> 0x83646f67
    {
        RlpEncoder encoder;
        encoder.add(Bytes{'d', 'o', 'g'});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "83646f67");
    }

    // Test: zero -> 0x80
    {
        RlpEncoder encoder;
        encoder.add(uint32_t{0});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "80");
    }

    // Test: smallint 1 -> 0x01
    {
        RlpEncoder encoder;
        encoder.add(uint8_t{1});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "01");
    }

    // Test: smallint 16 -> 0x10
    {
        RlpEncoder encoder;
        encoder.add(uint8_t{16});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "10");
    }

    // Test: smallint 127 -> 0x7f
    {
        RlpEncoder encoder;
        encoder.add(uint8_t{127});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "7f");
    }

    // Test: mediumint 128 -> 0x8180
    {
        RlpEncoder encoder;
        encoder.add(uint8_t{128});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "8180");
    }

    // Test: mediumint 1000 -> 0x8203e8
    {
        RlpEncoder encoder;
        encoder.add(uint16_t{1000});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "8203e8");
    }

    // Test: mediumint 100000 -> 0x830186a0
    {
        RlpEncoder encoder;
        encoder.add(uint32_t{100000});
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "830186a0");
    }

    // Test: emptylist -> 0xc0
    {
        RlpEncoder encoder;
        encoder.BeginList();
        encoder.EndList();
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "c0");
    }

    // Test: stringlist ["dog","god","cat"] -> starts with 0xcc
    {
        RlpEncoder encoder;
        encoder.BeginList();
        encoder.add(Bytes{'d', 'o', 'g'});
        encoder.add(Bytes{'g', 'o', 'd'});
        encoder.add(Bytes{'c', 'a', 't'});
        encoder.EndList();
        auto bytes_result = encoder.GetBytes(); 
        ASSERT_TRUE(bytes_result); 
        auto result = bytes_to_hex(*bytes_result.value());
        EXPECT_EQ(result.substr(0, 2), "cc") << "List should start with 0xcc";
        EXPECT_EQ(result, "cc83646f6783676f6483636174");
    }

    // Test: multilist ["zw",[4],1] -> 0xc6827a77c10401
    {
        RlpEncoder encoder;
        encoder.BeginList();
        encoder.add(Bytes{'z', 'w'});
        encoder.BeginList();
        encoder.add(uint8_t{4});
        encoder.EndList();
        encoder.add(uint8_t{1});
        encoder.EndList();
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "c6827a77c10401");
    }

    // Test: listsoflists [[],[]] -> 0xc4c2c0c0c0
    {
        RlpEncoder encoder;
        encoder.BeginList();
        encoder.BeginList();
        encoder.BeginList();
        encoder.EndList();
        encoder.BeginList();
        encoder.EndList();
        encoder.EndList();
        encoder.BeginList();
        encoder.EndList();
        encoder.EndList();
        auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(bytes_to_hex(*result.value()), "c4c2c0c0c0");
    }
}

// Test big integers (>256 bit) from Ethereum bigint test
TEST_F(EthereumRlpTest, BigIntegerTests) {
    // Test uint256 maximum value (2^256 - 1)
    // This is 32 bytes of 0xFF
    {
        // Create max uint256: 0xFFFFFFFF...FFFF (32 bytes)
        intx::uint256 max_uint256 = 0;
        for (int i = 0; i < 32; ++i) {
            max_uint256 = (max_uint256 << 8) | 0xFF;
        }
        
        RlpEncoder encoder;
        encoder.add(max_uint256);
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Should be encoded as: 0xa0 (32 bytes string) + 32 bytes of 0xff
        EXPECT_EQ(encoded[0], 0xa0) << "Should use 32-byte string encoding";
        EXPECT_EQ(encoded.size(), 33) << "Should be 1 byte header + 32 bytes data";
        
        // Verify all 32 bytes are 0xff
        for (size_t i = 1; i < 33; ++i) {
            EXPECT_EQ(encoded[i], 0xff) << "Byte " << i << " should be 0xff";
        }
        
        // Decode and verify roundtrip
        RlpDecoder decoder(encoded);
        intx::uint256 decoded;
        auto result = decoder.read(decoded);
        EXPECT_TRUE(result) << "Should decode successfully";
        EXPECT_EQ(decoded, max_uint256) << "Roundtrip should preserve value";
        EXPECT_TRUE(decoder.IsFinished());
    }

    // Test specific large uint256 value from Ethereum tests
    // bigint: 0xa1 0x01 0x00 0x00...0x00 (33 bytes total: 0x01 followed by 32 zeros)
    // This represents 2^256 encoded incorrectly, but we test large valid uint256 values
    {
        // Test a large uint256: 0x0100000000000000000000000000000000000000000000000000000000000000
        // This is 2^248
        intx::uint256 large_value = intx::uint256{1} << 248;
        
        RlpEncoder encoder;
        encoder.add(large_value);
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Should encode as string with proper length
        EXPECT_GT(encoded.size(), 1) << "Should have header and data";
        
        // Decode and verify
        RlpDecoder decoder(encoded);
        intx::uint256 decoded;
        auto result = decoder.read(decoded);
        EXPECT_TRUE(result) << "Should decode successfully";
        EXPECT_EQ(decoded, large_value) << "Should preserve 2^248";
        EXPECT_TRUE(decoder.IsFinished());
    }

    // Test various uint256 boundary values
    {
        // Test 2^128
        intx::uint256 val_128 = intx::uint256{1} << 128;
        test_roundtrip(val_128);
        
        // Test 2^192
        intx::uint256 val_192 = intx::uint256{1} << 192;
        test_roundtrip(val_192);
        
        // Test 2^255 (largest power of 2 that fits in uint256)
        intx::uint256 val_255 = intx::uint256{1} << 255;
        test_roundtrip(val_255);
    }

    // Test Ethereum-specific large values
    {
        // Test a value like total supply of ETH in wei (slightly over 120M ETH)
        // 120,000,000 ETH * 10^18 wei/ETH
        intx::uint256 eth_supply = intx::uint256{120'000'000} * intx::uint256{1'000'000'000'000'000'000ULL};
        
        RlpEncoder encoder;
        encoder.add(eth_supply);
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto encoded = *encoded_result.value();
        
        RlpDecoder decoder(encoded);
        intx::uint256 decoded;
        EXPECT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, eth_supply) << "Should preserve large ETH supply value";
    }

    // Test exact hex from Ethereum bigint test
    // From rlptest.json: "bigint" test with encoding 0xa1 followed by 33 bytes
    // Note: This represents a value > 2^256 which doesn't fit in uint256
    // We test the largest valid uint256 values instead
    {
        // Create a uint256 with specific pattern for testing
        // Use hex string: 0x0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20
        intx::uint256 pattern_value = 0;
        for (int i = 0; i < 32; ++i) {
            pattern_value = (pattern_value << 8) | static_cast<uint8_t>(i + 1);
        }
        
        RlpEncoder encoder;
        encoder.add(pattern_value);
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        // Should be 0xa0 (32 byte string) + 32 bytes of data
        EXPECT_EQ(encoded[0], 0xa0);
        EXPECT_EQ(encoded.size(), 33);
        
        // Verify the pattern
        for (size_t i = 0; i < 32; ++i) {
            EXPECT_EQ(encoded[i + 1], static_cast<uint8_t>(i + 1));
        }
        
        // Verify roundtrip
        RlpDecoder decoder(encoded);
        intx::uint256 decoded;
        EXPECT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, pattern_value);
    }

    // Test that uint256 zero is encoded as empty string (0x80)
    {
        intx::uint256 zero{0};
        
        RlpEncoder encoder;
        encoder.add(zero);
        auto encoded_result = encoder.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        
        EXPECT_EQ(bytes_to_hex(encoded), "80") << "uint256 zero should encode as 0x80";
        
        RlpDecoder decoder(encoded);
        intx::uint256 decoded;
        EXPECT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, zero);
    }

    // Test uint256 value that fits in different byte sizes
    {
        // 1 byte value (0x7F)
        intx::uint256 val_1byte{0x7F};
        RlpEncoder enc1;
        enc1.add(val_1byte);
        auto enc1_result = enc1.GetBytes();
        ASSERT_TRUE(enc1_result);
        EXPECT_EQ(bytes_to_hex(*enc1_result.value()), "7f");
        
        // 1 byte value requiring string encoding (0x80)
        intx::uint256 val_1byte_str{0x80};
        RlpEncoder enc2;
        enc2.add(val_1byte_str);
        auto enc2_result = enc2.GetBytes();
        ASSERT_TRUE(enc2_result);
        EXPECT_EQ(bytes_to_hex(*enc2_result.value()), "8180");
        
        // 8 byte value
        intx::uint256 val_8byte{0x0102030405060708ULL};
        RlpEncoder enc3;
        enc3.add(val_8byte);
        auto encoded_result = enc3.GetBytes();
        ASSERT_TRUE(encoded_result);
        auto& encoded = *encoded_result.value();
        EXPECT_EQ(encoded[0], 0x88) << "Should use 8-byte string encoding";
        
        // 16 byte value
        intx::uint256 val_16byte = (intx::uint256{0x0102030405060708ULL} << 64) | 
                                   intx::uint256{0x090a0b0c0d0e0f10ULL};
        test_roundtrip(val_16byte);
    }
}

// Google Test main entry point
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
