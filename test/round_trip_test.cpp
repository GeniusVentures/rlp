#include "../include/rlp_encoder.hpp"
#include "../include/rlp_decoder.hpp"
#include <gtest/gtest.h>
#include <cstdint>

namespace {

// Test round-trip encoding/decoding for template methods
TEST(RoundTripTest, TemplateIntegralTypes) {
    // Test uint8_t
    {
        rlp::RlpEncoder encoder;
        uint8_t original = 255;
        encoder.add(original);
        rlp::Bytes encoded = encoder.get_bytes();
        
        rlp::RlpDecoder decoder(encoded);
        uint8_t decoded;
        ASSERT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, original);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    // Test uint16_t
    {
        rlp::RlpEncoder encoder;
        uint16_t original = 65535;
        encoder.add(original);
        rlp::Bytes encoded = encoder.get_bytes();
        
        rlp::RlpDecoder decoder(encoded);
        uint16_t decoded;
        ASSERT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, original);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    // Test uint32_t
    {
        rlp::RlpEncoder encoder;
        uint32_t original = 4294967295U;
        encoder.add(original);
        rlp::Bytes encoded = encoder.get_bytes();
        
        rlp::RlpDecoder decoder(encoded);
        uint32_t decoded;
        ASSERT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, original);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    // Test uint64_t
    {
        rlp::RlpEncoder encoder;
        uint64_t original = 18446744073709551615ULL;
        encoder.add(original);
        rlp::Bytes encoded = encoder.get_bytes();
        
        rlp::RlpDecoder decoder(encoded);
        uint64_t decoded;
        ASSERT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, original);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    // Test bool
    {
        rlp::RlpEncoder encoder;
        bool original = true;
        encoder.add(original);
        rlp::Bytes encoded = encoder.get_bytes();
        
        rlp::RlpDecoder decoder(encoded);
        bool decoded;
        ASSERT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, original);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    // Test bool false
    {
        rlp::RlpEncoder encoder;
        bool original = false;
        encoder.add(original);
        rlp::Bytes encoded = encoder.get_bytes();
        
        rlp::RlpDecoder decoder(encoded);
        bool decoded;
        ASSERT_TRUE(decoder.read(decoded));
        EXPECT_EQ(decoded, original);
        EXPECT_TRUE(decoder.is_finished());
    }
}

TEST(RoundTripTest, TemplateUint256) {
    rlp::RlpEncoder encoder;
    intx::uint256 original = intx::from_string<intx::uint256>("0x123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0");
    encoder.add(original);
    rlp::Bytes encoded = encoder.get_bytes();
    
    rlp::RlpDecoder decoder(encoded);
    intx::uint256 decoded;
    ASSERT_TRUE(decoder.read(decoded));
    EXPECT_EQ(decoded, original);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RoundTripTest, TemplateSequentialInList) {
    rlp::RlpEncoder encoder;
    encoder.begin_list();
    encoder.add(static_cast<uint8_t>(42));
    encoder.add(static_cast<uint16_t>(1337));
    encoder.add(static_cast<uint32_t>(0xDEADBEEF));
    encoder.add(true);
    encoder.add(false);
    encoder.end_list();
    rlp::Bytes encoded = encoder.get_bytes();
    
    rlp::RlpDecoder decoder(encoded);
    auto list_len = decoder.read_list_header();
    ASSERT_TRUE(list_len);
    
    uint8_t val1;
    ASSERT_TRUE(decoder.read(val1));
    EXPECT_EQ(val1, 42);
    
    uint16_t val2;
    ASSERT_TRUE(decoder.read(val2));
    EXPECT_EQ(val2, 1337);
    
    uint32_t val3;
    ASSERT_TRUE(decoder.read(val3));
    EXPECT_EQ(val3, 0xDEADBEEF);
    
    bool val4;
    ASSERT_TRUE(decoder.read(val4));
    EXPECT_EQ(val4, true);
    
    bool val5;
    ASSERT_TRUE(decoder.read(val5));
    EXPECT_EQ(val5, false);
    
    EXPECT_TRUE(decoder.is_finished());
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}