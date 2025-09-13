#include <gtest/gtest.h>
#include <rlp_encoder.hpp>
#include <vector>
#include <array>
#include <sstream>
#include <iomanip>

using namespace rlp;

// Helper to convert Bytes to hex string for comparison
std::string to_hex(rlp::ByteView bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : bytes) {
        ss << std::setw(2) << static_cast<int>(b);
    }
    return ss.str();
}

// Helper to convert hex string (without 0x) to Bytes
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

// --- Encoder Tests ---

TEST(RlpEncoder, EncodeEmptyString) {
    rlp::RlpEncoder encoder;
    encoder.add(rlp::ByteView{});
    EXPECT_EQ(to_hex(encoder.get_bytes()), "80");
}

TEST(RlpEncoder, EncodeSingleByteLiteral) {
    rlp::RlpEncoder encoder;
    encoder.add(from_hex("7b")); // 0x7B < 0x80
    EXPECT_EQ(to_hex(encoder.get_bytes()), "7b");
}

TEST(RlpEncoder, EncodeSingleByteString) {
    rlp::RlpEncoder encoder;
    encoder.add(from_hex("80")); // 0x80 >= 0x80
    EXPECT_EQ(to_hex(encoder.get_bytes()), "8180"); // length=1 -> 0x81 prefix
}

TEST(RlpEncoder, EncodeShortString) {
    rlp::RlpEncoder encoder;
    encoder.add(from_hex("abba")); // length=2
    EXPECT_EQ(to_hex(encoder.get_bytes()), "82abba"); // length=2 -> 0x82 prefix
}

TEST(RlpEncoder, EncodeLongString) {
    rlp::RlpEncoder encoder;
    std::string long_str(60, 'a'); // length=60 > 55
    rlp::Bytes long_bytes(reinterpret_cast<const uint8_t*>(long_str.data()), long_str.size());
    encoder.add(long_bytes);
    std::string expected_hex = "b83c";
    for(int i = 0; i < 60; ++i) expected_hex += "61";
    EXPECT_EQ(to_hex(encoder.get_bytes()), expected_hex);
}

TEST(RlpEncoder, EncodeUintZero) {
    rlp::RlpEncoder encoder;
    const uint64_t value = 0;
    encoder.add(value);
    EXPECT_EQ(to_hex(encoder.get_bytes()), "80"); // 0 encodes as empty string 0x80
}

TEST(RlpEncoder, EncodeUintSmall) {
    rlp::RlpEncoder encoder;
    const uint64_t value = 15;
    encoder.add(value); // 0x0F < 0x80
    EXPECT_EQ(to_hex(encoder.get_bytes()), "0f");
}

TEST(RlpEncoder, EncodeUintMedium) {
    rlp::RlpEncoder encoder;
    const uint64_t testVal = 0x400; // 1024
    encoder.add(testVal);
    EXPECT_EQ(to_hex(encoder.get_bytes()), "820400");
}

TEST(RlpEncoder, EncodeUintLarge) {
    rlp::RlpEncoder encoder;
    encoder.add(uint64_t{0xFFCCB5DDFFEE1483});
    EXPECT_EQ(to_hex(encoder.get_bytes()), "88ffccb5ddffee1483");
}

TEST(RlpEncoder, EncodeUint256Zero) {
    rlp::RlpEncoder encoder;
    encoder.add(intx::uint256{0});
    EXPECT_EQ(to_hex(encoder.get_bytes()), "80");
}

TEST(RlpEncoder, EncodeUint256Large) {
    rlp::RlpEncoder encoder;
    encoder.add(intx::from_string<intx::uint256>("0x10203E405060708090A0B0C0D0E0F2"));
    EXPECT_EQ(to_hex(encoder.get_bytes()), "8f10203e405060708090a0b0c0d0e0f2");
}

TEST(RlpEncoder, EncodeBoolTrue) {
    rlp::RlpEncoder encoder;
    encoder.add(true);
    EXPECT_EQ(to_hex(encoder.get_bytes()), "01"); // true encodes as 0x01
}

TEST(RlpEncoder, EncodeBoolFalse) {
    rlp::RlpEncoder encoder;
    encoder.add(false);
    EXPECT_EQ(to_hex(encoder.get_bytes()), "80"); // false encodes as 0x80 (empty string / zero)
}

TEST(RlpEncoder, EncodeEmptyList) {
    rlp::RlpEncoder encoder;
    encoder.begin_list();
    encoder.end_list();
    EXPECT_EQ(to_hex(encoder.get_bytes()), "c0");
}

TEST(RlpEncoder, EncodeSimpleList) {
    rlp::RlpEncoder encoder;
    encoder.begin_list();
    encoder.add(from_hex("aa")); // Encodes as 0x81aa
    encoder.add(from_hex("bb")); // Encodes as 0x81bb
    encoder.end_list();
    EXPECT_EQ(to_hex(encoder.get_bytes()), "c481aa81bb");
}

TEST(RlpEncoder, EncodeNestedList) {
    rlp::RlpEncoder encoder;
    encoder.begin_list(); // Outer list
    encoder.add(uint64_t{1}); // 0x01
    encoder.begin_list(); // Inner list
    encoder.add(uint64_t{2}); // 0x02
    encoder.add(uint64_t{3}); // 0x03
    encoder.end_list();   // Inner list payload=0x0203 (len 2), header=0xC2 -> C20203 (len 3)
    encoder.end_list();   // Outer list payload=0x01 + C20203 (len 1+3=4), header=0xC4 -> C401C20203
    EXPECT_EQ(to_hex(encoder.get_bytes()), "c401c20203");
}

TEST(RlpEncoder, EncodeVectorUint) {
    rlp::RlpEncoder encoder;
    std::vector<uint64_t> v = {0xBBCCB5, 0xFFC0B5};
    encoder.add(v);
    EXPECT_EQ(to_hex(encoder.get_bytes()), "c883bbccb583ffc0b5");
}

// --- Additional Edge Case Tests ---

TEST(RlpEncoder, EncodeUint256Max) {
    rlp::RlpEncoder encoder;
    intx::uint256 max_val = ~intx::uint256{0}; // Maximum uint256
    encoder.add(max_val);
    // This should encode as a long string
    EXPECT_TRUE(encoder.get_bytes().size() > 1);
}

TEST(RlpEncoder, EncodeLargeString) {
    rlp::RlpEncoder encoder;
    std::string large_str(1000, 'x');
    rlp::Bytes large_bytes(reinterpret_cast<const uint8_t*>(large_str.data()), large_str.size());
    encoder.add(large_bytes);
    EXPECT_EQ(encoder.get_bytes().size(), 1000 + 4); // Payload + header
}

TEST(RlpEncoder, EncodeUnclosedListThrows) {
    rlp::RlpEncoder encoder;
    encoder.begin_list();
    encoder.add(uint64_t{1});
    // Don't call end_list()
    EXPECT_THROW(encoder.get_bytes(), std::logic_error);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
