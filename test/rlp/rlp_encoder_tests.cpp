#include <gtest/gtest.h>
#include <rlp/rlp_encoder.hpp>
#include "test_helpers.hpp"
#include <vector>
#include <array>

using namespace rlp;
using namespace rlp::test;

// --- Encoder Tests ---

TEST(RlpEncoder, EncodeEmptyString) {
    rlp::RlpEncoder encoder;
    encoder.add(rlp::ByteView{});
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "80");
}

TEST(RlpEncoder, EncodeSingleByteLiteral) {
    rlp::RlpEncoder encoder;
    encoder.add(from_hex("7b")); // 0x7B < 0x80
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "7b");
}

TEST(RlpEncoder, EncodeSingleByteString) {
    rlp::RlpEncoder encoder;
    encoder.add(from_hex("80")); // 0x80 >= 0x80
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "8180"); // length=1 -> 0x81 prefix
}

TEST(RlpEncoder, EncodeShortString) {
    rlp::RlpEncoder encoder;
    encoder.add(from_hex("abba")); // length=2
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "82abba"); // length=2 -> 0x82 prefix
}

TEST(RlpEncoder, EncodeLongString) {
    rlp::RlpEncoder encoder;
    std::string long_str(60, 'a'); // length=60 > 55
    rlp::Bytes long_bytes(reinterpret_cast<const uint8_t*>(long_str.data()), long_str.size());
    encoder.add(long_bytes);
    std::string expected_hex = "b83c";
    for(int i = 0; i < 60; ++i) expected_hex += "61";
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), expected_hex);
}

TEST(RlpEncoder, EncodeUintZero) {
    rlp::RlpEncoder encoder;
    const uint64_t value = 0;
    encoder.add(value);
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "80"); // 0 encodes as empty string 0x80
}

TEST(RlpEncoder, EncodeUintSmall) {
    rlp::RlpEncoder encoder;
    const uint64_t value = 15;
    encoder.add(value); // 0x0F < 0x80
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "0f");
}

TEST(RlpEncoder, EncodeUintMedium) {
    rlp::RlpEncoder encoder;
    const uint64_t testVal = 0x400; // 1024
    encoder.add(testVal);
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "820400");
}

TEST(RlpEncoder, EncodeUintLarge) {
    rlp::RlpEncoder encoder;
    encoder.add(uint64_t{0xFFCCB5DDFFEE1483});
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "88ffccb5ddffee1483");
}

TEST(RlpEncoder, EncodeUint8Large) {
    rlp::RlpEncoder encoder;
    encoder.add(uint8_t{200}); // 200 = 0xC8 >= 0x80
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "81c8");
}

TEST(RlpEncoder, EncodeUint8Small) {
    rlp::RlpEncoder encoder;
    encoder.add(uint8_t{100}); // 100 = 0x64 < 0x80
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "64");
}

TEST(RlpEncoder, EncodeUint16Small) {
    rlp::RlpEncoder encoder;
    encoder.add(uint16_t{100}); // 100 < 128
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "64");
}

TEST(RlpEncoder, EncodeUint16Big) {
    rlp::RlpEncoder encoder;
    encoder.add(uint16_t{300}); // 300 = 0x012C
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "82012c");
}

TEST(RlpEncoder, EncodeUint32Small) {
    rlp::RlpEncoder encoder;
    encoder.add(uint32_t{100}); // 100 < 128
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "64");
}

TEST(RlpEncoder, EncodeUint32Big) {
    rlp::RlpEncoder encoder;
    encoder.add(uint32_t{70000}); // 70000 = 0x011170
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "83011170");
}

TEST(RlpEncoder, EncodeUint256Zero) {
    rlp::RlpEncoder encoder;
    encoder.add(intx::uint256{0});
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "80");
}

TEST(RlpEncoder, EncodeUint256Large) {
    rlp::RlpEncoder encoder;
    encoder.add(intx::from_string<intx::uint256>("0x10203E405060708090A0B0C0D0E0F2"));
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "8f10203e405060708090a0b0c0d0e0f2");
}

TEST(RlpEncoder, EncodeBoolTrue) {
    rlp::RlpEncoder encoder;
    encoder.add(true);
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "01"); // true encodes as 0x01
}

TEST(RlpEncoder, EncodeBoolFalse) {
    rlp::RlpEncoder encoder;
    encoder.add(false);
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "80"); // false encodes as 0x80 (empty string / zero)
}

TEST(RlpEncoder, EncodeEmptyList) {
    rlp::RlpEncoder encoder;
    encoder.BeginList();
    encoder.EndList();
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "c0");
}

TEST(RlpEncoder, EncodeSimpleList) {
    rlp::RlpEncoder encoder;
    encoder.BeginList();
    encoder.add(from_hex("aa")); // Encodes as 0x81aa
    encoder.add(from_hex("bb")); // Encodes as 0x81bb
    encoder.EndList();
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "c481aa81bb");
}

TEST(RlpEncoder, EncodeNestedList) {
    rlp::RlpEncoder encoder;
    encoder.BeginList(); // Outer list
    encoder.add(uint64_t{1}); // 0x01
    encoder.BeginList(); // Inner list
    encoder.add(uint64_t{2}); // 0x02
    encoder.add(uint64_t{3}); // 0x03
    encoder.EndList();   // Inner list payload=0x0203 (len 2), header=0xC2 -> C20203 (len 3)
    encoder.EndList();   // Outer list payload=0x01 + C20203 (len 1+3=4), header=0xC4 -> C401C20203
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "c401c20203");
}

TEST(RlpEncoder, EncodeVectorUint) {
    rlp::RlpEncoder encoder;
    std::vector<uint64_t> v = {0xBBCCB5, 0xFFC0B5};
    encoder.add(v);
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(to_hex(*result.value()), "c883bbccb583ffc0b5");
}

// --- Additional Edge Case Tests ---

TEST(RlpEncoder, EncodeUint256Max) {
    rlp::RlpEncoder encoder;
    intx::uint256 max_val = ~intx::uint256{0}; // Maximum uint256
    encoder.add(max_val);
    // This should encode as a long string
    auto result = encoder.GetBytes(); 
    ASSERT_TRUE(result); 
    EXPECT_TRUE(result.value()->size() > 1);
}

TEST(RlpEncoder, EncodeLargeString) {
    rlp::RlpEncoder encoder;
    std::string large_str(1000, 'x');
    rlp::Bytes large_bytes(reinterpret_cast<const uint8_t*>(large_str.data()), large_str.size());
    encoder.add(large_bytes);
    auto result = encoder.GetBytes(); ASSERT_TRUE(result); EXPECT_EQ(result.value()->size(), 1003); // 1000 payload + 3 header bytes
}

TEST(RlpEncoder, EncodeUnclosedListReturnsError) {
    rlp::RlpEncoder encoder;
    encoder.BeginList();
    encoder.add(uint64_t{1});
    // Don't call end_list()
    auto result = encoder.GetBytes();
    EXPECT_FALSE(result) << "Should fail with unclosed list";
    EXPECT_EQ(result.error(), rlp::EncodingError::kUnclosedList);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
