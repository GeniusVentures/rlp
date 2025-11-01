#include <gtest/gtest.h>
#include <rlp/rlp_encoder.hpp>
#include <rlp/rlp_decoder.hpp>
#include <rlp/common.hpp>
#include <type_traits>

namespace rlp {

// Test class for type safety validation
class RLPTypeSafetyTests : public ::testing::Test {
protected:
    using Encoder = rlp::RLPEncoder;
    using Decoder = rlp::RLPDecoder;
};

// ============================================================================
// Compile-time Type Trait Tests
// ============================================================================

// Test: Verify encodable types are correctly identified
TEST_F(RLPTypeSafetyTests, EncodableTypesIdentified) {
    // Unsigned integral types should be encodable
    EXPECT_TRUE(is_rlp_encodable_v<uint8_t>);
    EXPECT_TRUE(is_rlp_encodable_v<uint16_t>);
    EXPECT_TRUE(is_rlp_encodable_v<uint32_t>);
    EXPECT_TRUE(is_rlp_encodable_v<uint64_t>);
    
    // bool should be encodable
    EXPECT_TRUE(is_rlp_encodable_v<bool>);
    
    // uint256 should be encodable
    EXPECT_TRUE(is_rlp_encodable_v<intx::uint256>);
    
    // Bytes and ByteView should be encodable
    EXPECT_TRUE(is_rlp_encodable_v<Bytes>);
    EXPECT_TRUE(is_rlp_encodable_v<ByteView>);
}

// Test: Verify decodable types are correctly identified
TEST_F(RLPTypeSafetyTests, DecodableTypesIdentified) {
    // Unsigned integral types should be decodable
    EXPECT_TRUE(is_rlp_decodable_v<uint8_t>);
    EXPECT_TRUE(is_rlp_decodable_v<uint16_t>);
    EXPECT_TRUE(is_rlp_decodable_v<uint32_t>);
    EXPECT_TRUE(is_rlp_decodable_v<uint64_t>);
    
    // bool should be decodable
    EXPECT_TRUE(is_rlp_decodable_v<bool>);
    
    // uint256 should be decodable
    EXPECT_TRUE(is_rlp_decodable_v<intx::uint256>);
    
    // Bytes should be decodable
    EXPECT_TRUE(is_rlp_decodable_v<Bytes>);
    
    // ByteView should NOT be directly decodable (can't write to const view)
    EXPECT_FALSE(is_rlp_decodable_v<ByteView>);
}

// Test: Verify invalid types are rejected
TEST_F(RLPTypeSafetyTests, InvalidTypesRejected) {
    // Signed integral types should NOT be encodable
    EXPECT_FALSE(is_rlp_encodable_v<int8_t>);
    EXPECT_FALSE(is_rlp_encodable_v<int16_t>);
    EXPECT_FALSE(is_rlp_encodable_v<int32_t>);
    EXPECT_FALSE(is_rlp_encodable_v<int64_t>);
    
    // Floating point types should NOT be encodable
    EXPECT_FALSE(is_rlp_encodable_v<float>);
    EXPECT_FALSE(is_rlp_encodable_v<double>);
    
    // Custom types should NOT be encodable
    struct CustomType { int x; };
    EXPECT_FALSE(is_rlp_encodable_v<CustomType>);
    
    // Pointers should NOT be encodable
    EXPECT_FALSE(is_rlp_encodable_v<uint8_t*>);
    EXPECT_FALSE(is_rlp_encodable_v<const char*>);
}

// ============================================================================
// Runtime Encoding/Decoding Tests
// ============================================================================

// Test: Encode and decode uint8_t
TEST_F(RLPTypeSafetyTests, EncodeDecodeUint8) {
    Encoder encoder;
    encoder.add(uint8_t(42));
    ByteView encoded = encoder.encoded();
    
    Decoder decoder(encoded);
    uint8_t value = 0;
    EXPECT_TRUE(decoder.read(value));
    EXPECT_EQ(value, uint8_t(42));
}

// Test: Encode and decode uint64_t
TEST_F(RLPTypeSafetyTests, EncodeDecodeUint64) {
    Encoder encoder;
    encoder.add(uint64_t(0x123456789ABCDEF0));
    ByteView encoded = encoder.encoded();
    
    Decoder decoder(encoded);
    uint64_t value = 0;
    EXPECT_TRUE(decoder.read(value));
    EXPECT_EQ(value, uint64_t(0x123456789ABCDEF0));
}

// Test: Encode and decode bool
TEST_F(RLPTypeSafetyTests, EncodeDecodeBool) {
    {
        Encoder encoder;
        encoder.add(true);
        ByteView encoded = encoder.encoded();
        
        Decoder decoder(encoded);
        bool value = false;
        EXPECT_TRUE(decoder.read(value));
        EXPECT_TRUE(value);
    }
    
    {
        Encoder encoder;
        encoder.add(false);
        ByteView encoded = encoder.encoded();
        
        Decoder decoder(encoded);
        bool value = true;
        EXPECT_TRUE(decoder.read(value));
        EXPECT_FALSE(value);
    }
}

// Test: Encode and decode uint256
TEST_F(RLPTypeSafetyTests, EncodeDecodeUint256) {
    Encoder encoder;
    intx::uint256 value256 = intx::uint256(0xDEADBEEFCAFEBABEUL) << 64;
    encoder.add(value256);
    ByteView encoded = encoder.encoded();
    
    Decoder decoder(encoded);
    intx::uint256 decoded = 0;
    EXPECT_TRUE(decoder.read(decoded));
    EXPECT_EQ(decoded, value256);
}

// Test: Encode and decode Bytes
TEST_F(RLPTypeSafetyTests, EncodeDecodeBytes) {
    Encoder encoder;
    Bytes data = {0x01, 0x02, 0x03, 0x04, 0x05};
    encoder.add(data);
    ByteView encoded = encoder.encoded();
    
    Decoder decoder(encoded);
    Bytes decoded;
    EXPECT_TRUE(decoder.read(decoded));
    EXPECT_EQ(decoded, data);
}

// Test: Encode and decode ByteView (for encoding only)
TEST_F(RLPTypeSafetyTests, EncodeByteView) {
    Encoder encoder;
    ByteView data{reinterpret_cast<const uint8_t*>("\x01\x02\x03\x04\x05"), 5};
    encoder.add(data);
    ByteView encoded = encoder.encoded();
    
    // Verify we can decode the same data back
    Decoder decoder(encoded);
    Bytes decoded;
    EXPECT_TRUE(decoder.read(decoded));
    EXPECT_EQ(decoded[0], uint8_t(0x01));
    EXPECT_EQ(decoded[1], uint8_t(0x02));
    EXPECT_EQ(decoded[2], uint8_t(0x03));
}

// Test: Encode and decode vector of uint8_t
TEST_F(RLPTypeSafetyTests, EncodeDecodeVectorUint8) {
    Encoder encoder;
    std::vector<uint8_t> original{10, 20, 30, 40, 50};
    for (auto v : original) {
        encoder.add(v);
    }
    ByteView encoded = encoder.encoded();
    
    Decoder decoder(encoded);
    std::vector<uint8_t> decoded;
    uint8_t value = 0;
    while (decoder.read(value)) {
        decoded.push_back(value);
    }
    EXPECT_EQ(decoded, original);
}

// Test: Encode and decode vector of uint32_t
TEST_F(RLPTypeSafetyTests, EncodeDecodeVectorUint32) {
    Encoder encoder;
    std::vector<uint32_t> original{0x11223344, 0x55667788, 0x99AABBCC};
    for (auto v : original) {
        encoder.add(v);
    }
    ByteView encoded = encoder.encoded();
    
    Decoder decoder(encoded);
    std::vector<uint32_t> decoded;
    uint32_t value = 0;
    while (decoder.read(value)) {
        decoded.push_back(value);
    }
    EXPECT_EQ(decoded, original);
}

// Test: Encode and decode multiple types in sequence
TEST_F(RLPTypeSafetyTests, EncodeDecodeMultipleTypes) {
    Encoder encoder;
    uint8_t u8 = 42;
    uint32_t u32 = 0xDEADBEEF;
    bool b = true;
    Bytes data{0xAA, 0xBB, 0xCC};
    
    encoder.add(u8);
    encoder.add(u32);
    encoder.add(b);
    encoder.add(data);
    ByteView encoded = encoder.encoded();
    
    Decoder decoder(encoded);
    uint8_t du8 = 0;
    uint32_t du32 = 0;
    bool db = false;
    Bytes ddata;
    
    EXPECT_TRUE(decoder.read(du8));
    EXPECT_TRUE(decoder.read(du32));
    EXPECT_TRUE(decoder.read(db));
    EXPECT_TRUE(decoder.read(ddata));
    
    EXPECT_EQ(du8, u8);
    EXPECT_EQ(du32, u32);
    EXPECT_EQ(db, b);
    EXPECT_EQ(ddata, data);
}

// Test: Use static read method with valid type
TEST_F(RLPTypeSafetyTests, StaticReadWithValidType) {
    Encoder encoder;
    encoder.add(uint32_t(0x12345678));
    ByteView encoded = encoder.encoded();
    
    ByteView remaining = encoded;
    auto result = Decoder::read<uint32_t>(remaining);
    EXPECT_TRUE(result);
    EXPECT_EQ(result.value(), uint32_t(0x12345678));
}

// Test: Use static read method with multiple values
TEST_F(RLPTypeSafetyTests, StaticReadMultipleValues) {
    Encoder encoder;
    encoder.add(uint16_t(100));
    encoder.add(uint16_t(200));
    encoder.add(uint16_t(300));
    ByteView encoded = encoder.encoded();
    
    ByteView remaining = encoded;
    auto r1 = Decoder::read<uint16_t>(remaining);
    EXPECT_TRUE(r1);
    EXPECT_EQ(r1.value(), uint16_t(100));
    
    auto r2 = Decoder::read<uint16_t>(remaining);
    EXPECT_TRUE(r2);
    EXPECT_EQ(r2.value(), uint16_t(200));
    
    auto r3 = Decoder::read<uint16_t>(remaining);
    EXPECT_TRUE(r3);
    EXPECT_EQ(r3.value(), uint16_t(300));
}

} // namespace rlp
