#include <gtest/gtest.h>
#include <rlp_encoder.hpp>
#include <rlp_decoder.hpp>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <limits>
#include <gtest/gtest.h>
using namespace rlp;

// Helper to convert hex string to Bytes
rlp::Bytes from_hex(std::string_view hex) {
    if (hex.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have even length");
    }
    rlp::Bytes bytes;
    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = std::string(hex.substr(i, 2));
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Helper to convert Bytes to hex string
std::string to_hex(rlp::ByteView bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : bytes) {
        ss << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
}

// ===================================================================
// COMPREHENSIVE ERROR CONDITION TESTS
// ===================================================================

class RlpErrorConditionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize random number generator with fixed seed for reproducibility
        rng_.seed(12345);
    }
    std::mt19937 rng_;
};

TEST_F(RlpErrorConditionsTest, DecoderMalformedHeaders) {
    // Test all possible malformed header scenarios
    
    // Invalid string length encodings
    rlp::Bytes tmp;
    EXPECT_FALSE(RlpDecoder(from_hex("b8")).read(tmp)); // Short string, missing length byte
    EXPECT_FALSE(RlpDecoder(from_hex("b90000")).read(tmp)); // Long string, length too short
    EXPECT_FALSE(RlpDecoder(from_hex("ba")).read(tmp)); // Long string, missing length bytes

    // Invalid list length encodings
    EXPECT_FALSE(RlpDecoder(from_hex("f8")).read(tmp)); // Short list, missing length byte
    EXPECT_FALSE(RlpDecoder(from_hex("f90000")).read(tmp)); // Long list, length too short
    EXPECT_FALSE(RlpDecoder(from_hex("fa")).read(tmp)); // Long list, missing length bytes

    // Reserved bytes (0xf9-0xff are always invalid as single-byte headers)
    for (uint32_t byte = 0xf9; byte <= 0xff; ++byte) {
        rlp::Bytes data = {static_cast<uint8_t>(byte)};
        EXPECT_FALSE( RlpDecoder( data ).read( tmp ));
    }
}

TEST_F(RlpErrorConditionsTest, DecoderTruncatedData) {
    // Test truncated data in various scenarios
    
    // Truncated string data
    rlp::Bytes tmp;
    EXPECT_FALSE(RlpDecoder(from_hex("85123456")).read(tmp)); // Claims 5 bytes, only has 2
    EXPECT_FALSE(RlpDecoder(from_hex("b90005123456")).read(tmp)); // Claims 5 bytes via long encoding

    // Truncated list data
    EXPECT_FALSE(RlpDecoder(from_hex("c5123456")).read(tmp)); // Claims 5 bytes, only has 2
    EXPECT_FALSE(RlpDecoder(from_hex("f90005123456")).read(tmp)); // Claims 5 bytes via long encoding

    // Truncated nested structures
    EXPECT_FALSE(RlpDecoder(from_hex("c3c1")).read(tmp)); // List containing truncated sublist
    EXPECT_FALSE(RlpDecoder(from_hex("c382")).read(tmp)); // List containing truncated string
}

TEST_F(RlpErrorConditionsTest, DecoderNonCanonicalEncoding) {
    // Test non-canonical encodings that should be rejected
    
    // Single bytes that should not be string-encoded
    // According to RLP spec, bytes < 0x80 should be encoded directly, not as strings
    // The decoder should reject non-canonical encodings like {0x81, i} for i < 0x80
    for (uint8_t i = 0; i < 0x80; ++i) {
        rlp::Bytes non_canonical = {0x81, i}; // Single byte encoded as string (non-canonical)
        RlpDecoder decoder(non_canonical);
        rlp::Bytes result;
        EXPECT_FALSE(decoder.read(result)); // Should reject non-canonical encoding
        
        // The canonical encoding should just be the byte itself
        rlp::Bytes canonical = {i};
        RlpDecoder canonical_decoder(canonical);
        rlp::Bytes canonical_result;
        EXPECT_TRUE(canonical_decoder.read(canonical_result));
        EXPECT_EQ(canonical_result.size(), 1);
        EXPECT_EQ(canonical_result[0], i);
    }

    // Strings that could be encoded shorter
    rlp::Bytes result;
    EXPECT_FALSE(RlpDecoder(from_hex("b800")).read(result)); // Long encoding for empty string (non-canonical and invalid)

    // Leading zeros in length encoding
    EXPECT_FALSE(RlpDecoder(from_hex("b90000")).read(result)); // Length with leading zero
    EXPECT_FALSE(RlpDecoder(from_hex("f90000")).read(result)); // Length with leading zero
}

TEST_F(RlpErrorConditionsTest, DecoderIntegerOverflow) {
    // Test integer overflow scenarios for all types
    
    // uint8_t overflow (> 255)
    {
        RlpEncoder encoder;
        encoder.add(uint16_t{256}); // Should succeed
        auto encoded = encoder.get_bytes();
        RlpDecoder decoder(encoded);
        uint8_t result;
        EXPECT_FALSE(decoder.read(result)); // Should fail due to overflow
    }
    
    // uint16_t overflow (> 65535)
    {
        RlpEncoder encoder;
        encoder.add(uint32_t{65536}); // Should succeed
        auto encoded = encoder.get_bytes();
        RlpDecoder decoder(encoded);
        uint16_t result;
        EXPECT_FALSE(decoder.read(result)); // Should fail due to overflow
    }
    
    // uint32_t overflow (> 4294967295)
    {
        RlpEncoder encoder;
        encoder.add(uint64_t{4294967296ULL}); // Should succeed
        auto encoded = encoder.get_bytes();
        RlpDecoder decoder(encoded);
        uint32_t result;
        EXPECT_FALSE(decoder.read(result)); // Should fail due to overflow
    }
    
    // uint256 overflow (> 2^256 - 1) - test with oversized data
    {
        rlp::Bytes oversized_data = {0xa1, 0x01}; // 33 bytes of data
        for (int i = 0; i < 33; ++i) {
            oversized_data.push_back(0xff);
        }
        RlpDecoder decoder(oversized_data);
        intx::uint256 result;
        EXPECT_FALSE(decoder.read(result)); // Should fail due to size limit
    }
}

TEST_F(RlpErrorConditionsTest, DecoderTypeErrors) {
    // Test type mismatches
    
    // Try to read list as string
    {
        RlpEncoder encoder;
        encoder.begin_list();
        encoder.add(uint8_t{42}); // Should succeed
        encoder.end_list();
        auto encoded = encoder.get_bytes();

        RlpDecoder decoder(encoded);
        rlp::Bytes str;
        EXPECT_FALSE(decoder.read(str));
    }

    // Try to read string as list
    {
        RlpEncoder encoder;
        encoder.add(rlp::ByteView{reinterpret_cast<const uint8_t*>("hello"), 5}); // Should succeed
        auto encoded = encoder.get_bytes();

        RlpDecoder decoder(encoded);
        rlp::Bytes list;
        EXPECT_FALSE(decoder.read_list_header()); // Should not be a list
    }

    // Try to read string as integer
    {
        rlp::Bytes string_data = from_hex("8568656c6c6f"); // "hello" as hex string (even length)
        RlpDecoder decoder(string_data);
        uint32_t result;
        EXPECT_FALSE(decoder.read(result));
    }
}

TEST_F(RlpErrorConditionsTest, EncoderEdgeCases) {
    // Test encoder edge cases and error conditions
    
    // Unclosed list should throw
    {
        RlpEncoder encoder;
        encoder.begin_list();
        encoder.add(uint8_t{42}); // Should succeed
        // Don't call end_list()
        EXPECT_THROW({
            [[maybe_unused]] auto bytes = encoder.get_bytes();
        }, std::logic_error);
    }
    
    // Multiple end_list calls should throw
    {
        RlpEncoder encoder;
        encoder.begin_list();
        encoder.add(uint8_t{42}); // Should succeed
        encoder.end_list();
        EXPECT_THROW(encoder.end_list(), std::logic_error);
    }
    
    // Empty encoder should work
    {
        RlpEncoder encoder;
        auto bytes = encoder.get_bytes();
        EXPECT_TRUE(bytes.empty());
    }
}

// ===================================================================
// BOUNDARY VALUE TESTS
// ===================================================================

TEST_F(RlpErrorConditionsTest, BoundaryValues) {
    // Test boundary values for all integer types
    
    // uint8_t boundaries
    for (uint8_t val : {uint8_t(0), uint8_t(1), uint8_t(127), uint8_t(128), uint8_t(254), uint8_t(255)}) {
        RlpEncoder encoder;
        encoder.add(val);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        uint8_t result;
        ASSERT_TRUE(decoder.read(result)) << "Failed for uint8_t value: " << static_cast<int>(val);
        EXPECT_EQ(result, val);
    }
    
    // uint16_t boundaries
    for (uint16_t val : {uint16_t(0), uint16_t(1), uint16_t(255), uint16_t(256), uint16_t(32767), uint16_t(32768), uint16_t(65534), uint16_t(65535)}) {
        RlpEncoder encoder;
        encoder.add(val);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        uint16_t result;
        ASSERT_TRUE(decoder.read(result)) << "Failed for uint16_t value: " << val;
        EXPECT_EQ(result, val);
    }
    
    // uint32_t boundaries
    for (uint32_t val : {uint32_t(0), uint32_t(1), uint32_t(65535), uint32_t(65536), uint32_t(2147483647U), uint32_t(2147483648U), uint32_t(4294967294U), uint32_t(4294967295U)}) {
        RlpEncoder encoder;
        encoder.add(val);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        uint32_t result;
        ASSERT_TRUE(decoder.read(result)) << "Failed for uint32_t value: " << val;
        EXPECT_EQ(result, val);
    }
    
    // uint64_t boundaries
    for (uint64_t val : {uint64_t(0ULL), uint64_t(1ULL), uint64_t(4294967295ULL), uint64_t(4294967296ULL),
                         uint64_t(9223372036854775807ULL), uint64_t(9223372036854775808ULL),
                         uint64_t(18446744073709551614ULL), uint64_t(18446744073709551615ULL)}) {
        RlpEncoder encoder;
        encoder.add(val);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        uint64_t result;
        ASSERT_TRUE(decoder.read(result)) << "Failed for uint64_t value: " << val;
        EXPECT_EQ(result, val);
    }
}

// ===================================================================
// STRESS TESTS
// ===================================================================

TEST_F(RlpErrorConditionsTest, DeepNesting) {
    // Test deeply nested structures
    const int max_depth = 100;
    
    RlpEncoder encoder;
    for (int i = 0; i < max_depth; ++i) {
        encoder.begin_list();
    }
    encoder.add(uint8_t{42});
    for (int i = 0; i < max_depth; ++i) {
        encoder.end_list();
    }
    
    auto encoded = encoder.get_bytes();
    RlpDecoder decoder(encoded);
    
    // Navigate to the deeply nested value
    for (int i = 0; i < max_depth; ++i) {
        auto list_header = decoder.read_list_header();
        ASSERT_TRUE(list_header.has_value()) << "Failed at nesting level: " << i;
    }
    uint8_t result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result, 42);
}

TEST_F(RlpErrorConditionsTest, LargeDataStructures) {
    // Test large string
    const size_t large_size = 100000;
    std::vector<uint8_t> large_data(large_size, 0xAB);
    
    RlpEncoder encoder;
    encoder.add(rlp::ByteView{large_data.data(), large_data.size()});
    auto encoded = encoder.get_bytes();
    
    RlpDecoder decoder(encoded);
    rlp::Bytes result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result.size(), large_size);
    EXPECT_TRUE(std::all_of(result.begin(), result.end(), [](uint8_t b) { return b == 0xAB; }));
}

TEST_F(RlpErrorConditionsTest, EmptyStructures) {
    // Test empty strings and lists at various nesting levels
    
    // Empty string
    {
        RlpEncoder encoder;
        encoder.add(rlp::ByteView{});
        auto encoded = encoder.get_bytes();

        RlpDecoder decoder(encoded);
        rlp::Bytes result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_TRUE(result.empty());
    }

    // Empty list
    {
        RlpEncoder encoder;
        encoder.begin_list();
        encoder.end_list();
        auto encoded = encoder.get_bytes();

        RlpDecoder decoder(encoded);
        auto list_header = decoder.read_list_header();
        ASSERT_TRUE(list_header.has_value());
    EXPECT_EQ(list_header.value(), 0);
    }

    // List of empty strings
    {
        RlpEncoder encoder;
        encoder.begin_list();
        for (int i = 0; i < 10; ++i) {
            encoder.add(rlp::ByteView{});
        }
        encoder.end_list();
        auto encoded = encoder.get_bytes();

        RlpDecoder decoder(encoded);
        auto list_header = decoder.read_list_header();
        ASSERT_TRUE(list_header.has_value());
    EXPECT_EQ(list_header.value(), 10u);
    for (size_t i = 0; i < list_header.value(); ++i) {
            rlp::Bytes str;
            ASSERT_TRUE(decoder.read(str)) << "Failed at index: " << i;
            EXPECT_TRUE(str.empty());
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}