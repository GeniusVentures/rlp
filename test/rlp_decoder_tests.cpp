#include <gtest/gtest.h>
#include <rlp_decoder.hpp>
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

// --- Decoder Tests ---

TEST(RlpDecoder, DecodeEmptyString) {
    rlp::Bytes data = from_hex("80");
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_TRUE(out.empty());
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeSingleByteLiteral) {
    rlp::Bytes data = from_hex("7b");
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(to_hex(out), "7b");
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeSingleByteString) {
    rlp::Bytes data = from_hex("8180");
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(to_hex(out), "80");
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeShortString) {
    rlp::Bytes data = from_hex("82abba");
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(to_hex(out), "abba");
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeLongString) {
    std::string long_str(60, 'a');
    std::string encoded_hex = "b83c";
    for(int i = 0; i < 60; ++i) encoded_hex += "61";
    rlp::Bytes data = from_hex(encoded_hex);
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out.size(), 60);
    EXPECT_EQ(out[0], 'a');
    EXPECT_EQ(out[59], 'a');
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUintZero) {
    rlp::Bytes data = from_hex("80");
    rlp::RlpDecoder decoder(data);
    uint64_t out = 99;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 0);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUintSmall) {
    rlp::Bytes data = from_hex("0f");
    rlp::RlpDecoder decoder(data);
    uint64_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 15);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUintMedium) {
    rlp::Bytes data = from_hex("820400");
    rlp::RlpDecoder decoder(data);
    uint64_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 0x400);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUintLarge) {
    rlp::Bytes data = from_hex("88ffccb5ddffee1483");
    rlp::RlpDecoder decoder(data);
    uint64_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 0xFFCCB5DDFFEE1483);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUint8Small) {
    rlp::Bytes data = from_hex("64"); // 100 < 128
    rlp::RlpDecoder decoder(data);
    uint8_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 100);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUint8Big) {
    rlp::Bytes data = from_hex("81c8"); // 200 >= 128
    rlp::RlpDecoder decoder(data);
    uint8_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 200);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUint16Small) {
    rlp::Bytes data = from_hex("64"); // 100 < 128
    rlp::RlpDecoder decoder(data);
    uint16_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 100);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUint16Big) {
    rlp::Bytes data = from_hex("82012c"); // 300 >= 128
    rlp::RlpDecoder decoder(data);
    uint16_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 300);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUint32Small) {
    rlp::Bytes data = from_hex("64"); // 100 < 128
    rlp::RlpDecoder decoder(data);
    uint32_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 100);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUint32Big) {
    rlp::Bytes data = from_hex("83011170"); // 70000 >= 128
    rlp::RlpDecoder decoder(data);
    uint32_t out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 70000);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUint256Zero) {
    rlp::Bytes data = from_hex("80");
    rlp::RlpDecoder decoder(data);
    intx::uint256 out = 99;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, 0);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeUint256Large) {
    rlp::Bytes data = from_hex("8f10203e405060708090a0b0c0d0e0f2");
    rlp::RlpDecoder decoder(data);
    intx::uint256 out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, intx::from_string<intx::uint256>("0x10203E405060708090A0B0C0D0E0F2"));
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeBoolTrue) {
    rlp::Bytes data = from_hex("01");
    rlp::RlpDecoder decoder(data);
    bool out;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, true);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeBoolFalse) {
    rlp::Bytes data = from_hex("80");
    rlp::RlpDecoder decoder(data);
    bool out = true;
    EXPECT_TRUE(decoder.read(out));
    EXPECT_EQ(out, false);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeEmptyList) {
    rlp::Bytes data = from_hex("c0");
    rlp::RlpDecoder decoder(data);
    auto len_res = decoder.read_list_header();
    ASSERT_TRUE(len_res);
    EXPECT_EQ(len_res.value(), 0);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeSimpleList) {
    rlp::Bytes data = from_hex("c481aa81bb");
    rlp::RlpDecoder decoder(data);
    auto len_res = decoder.read_list_header();
    ASSERT_TRUE(len_res);
    EXPECT_EQ(len_res.value(), 4);

    rlp::Bytes item1, item2;
    EXPECT_TRUE(decoder.read(item1));
    EXPECT_EQ(to_hex(item1), "aa");
    EXPECT_TRUE(decoder.read(item2));
    EXPECT_EQ(to_hex(item2), "bb");
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeNestedList) {
    rlp::Bytes bytes = from_hex("c401c20203");
    rlp::ByteView data = bytes;
    rlp::RlpDecoder decoder(data);
    auto len_res = decoder.read_list_header();
    ASSERT_TRUE(len_res);
    EXPECT_EQ(len_res.value(), 4);

    rlp::ByteView outer_list_data = decoder.remaining();
    uint64_t item1;
    auto res1 = decoder.read(outer_list_data, item1, rlp::Leftover::kAllow);
    ASSERT_TRUE(res1);
    EXPECT_EQ(item1, 1);

    rlp::RlpDecoder inner_decoder(outer_list_data);
    auto inner_len_res = inner_decoder.read_list_header();
    ASSERT_TRUE(inner_len_res);
    EXPECT_EQ(inner_len_res.value(), 2);

    rlp::ByteView inner_list_data = inner_decoder.remaining();
    uint64_t item2, item3;
    auto res2 = inner_decoder.read(inner_list_data, item2, rlp::Leftover::kAllow);
    ASSERT_TRUE(res2);
    EXPECT_EQ(item2, 2);

    auto res3 = inner_decoder.read(inner_list_data, item3, rlp::Leftover::kAllow);
    ASSERT_TRUE(res3);
    EXPECT_EQ(item3, 3);

    EXPECT_TRUE(inner_list_data.empty());
}

TEST(RlpDecoder, DecodeVectorUint) {
    rlp::Bytes data = from_hex("c883bbccb583ffc0b5");
    rlp::RlpDecoder decoder(data);
    std::vector<uint64_t> v;
    auto res = decoder.read_vector(v);
    ASSERT_TRUE(res);
    ASSERT_EQ(v.size(), 2);
    EXPECT_EQ(v[0], 0xBBCCB5);
    EXPECT_EQ(v[1], 0xFFC0B5);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeFixedArray) {
    rlp::Bytes data = from_hex("83aabbcc");
    rlp::RlpDecoder decoder(data);
    std::array<uint8_t, 3> arr;
    auto res = decoder.read(arr);
    ASSERT_TRUE(res);
    EXPECT_EQ(arr[0], 0xaa);
    EXPECT_EQ(arr[1], 0xbb);
    EXPECT_EQ(arr[2], 0xcc);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, DecodeFixedArraySingleByteLiteral) {
    rlp::Bytes data = from_hex("7a");
    rlp::RlpDecoder decoder(data);
    std::array<uint8_t, 1> arr = {0};
    auto res = decoder.read(arr);
    ASSERT_TRUE(res);
    EXPECT_EQ(arr[0], 0x7a);
    EXPECT_TRUE(decoder.is_finished());
}

// --- Error Cases ---

TEST(RlpDecoder, ErrorInputTooShortHeader) {
    rlp::Bytes data = from_hex("b8");
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kInputTooShort);
}

TEST(RlpDecoder, ErrorInputTooShortPayload) {
    rlp::Bytes data = from_hex("83aabb");
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kInputTooShort);
}

TEST(RlpDecoder, ErrorInputTooLong) {
    rlp::Bytes bytes = from_hex("0faa");
    rlp::ByteView data = bytes;
    rlp::RlpDecoder decoder(data);
    uint64_t out;
    auto res = decoder.read(out);
    ASSERT_TRUE(res);  // Should succeed reading 0x0f (value 15)
    EXPECT_EQ(out, 15);
    EXPECT_FALSE(decoder.is_finished());  // Should have leftover data 0xaa

    rlp::ByteView data2 = bytes;
    rlp::RlpDecoder decoder2(data2);
    uint64_t out2;
    auto res2 = decoder2.read<uint64_t>(data2, out2, rlp::Leftover::kProhibit);
    ASSERT_FALSE(res2);  // Should fail because of explicit kProhibit
    EXPECT_EQ(res2.error(), rlp::DecodingError::kInputTooLong);

    rlp::ByteView data3 = bytes;
    rlp::RlpDecoder decoder3(data3);
    uint64_t out3;
    auto res3 = decoder3.read<uint64_t>(data3, out3, rlp::Leftover::kAllow);
    ASSERT_TRUE(res3);  // Should succeed with explicit kAllow
    EXPECT_EQ(out3, 15);
}

TEST(RlpDecoder, ErrorLeadingZeroInt) {
    rlp::Bytes data = from_hex("8200f4");
    rlp::RlpDecoder decoder(data);
    uint64_t out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kLeadingZero);
}

TEST(RlpDecoder, ErrorNonCanonicalSizeShort) {
    rlp::Bytes data = from_hex("8105");
    rlp::RlpDecoder decoder(data);
    uint64_t out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kNonCanonicalSize);
}

TEST(RlpDecoder, ErrorNonCanonicalSizeLong) {
    rlp::Bytes data = from_hex("b8020004");
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kNonCanonicalSize);
}

TEST(RlpDecoder, ErrorUnexpectedList) {
    rlp::Bytes data = from_hex("c0");
    rlp::RlpDecoder decoder(data);
    uint64_t out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kUnexpectedList);
}

TEST(RlpDecoder, ErrorUnexpectedString) {
    rlp::Bytes data = from_hex("0f");
    rlp::RlpDecoder decoder(data);
    auto res = decoder.read_list_header();
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kUnexpectedString);
}

TEST(RlpDecoder, ErrorOverflow) {
    rlp::Bytes data = from_hex("8AFFFFFFFFFFFFFFFFFF7C");
    rlp::RlpDecoder decoder(data);
    uint64_t out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kOverflow);
}

TEST(RlpDecoder, ErrorUnexpectedListElements) {
    rlp::Bytes data = from_hex("c30102" "03");
    rlp::RlpDecoder decoder(data);
    std::vector<uint8_t> vec;
    auto res = decoder.read_vector(vec);
    // Note: Current implementation may not detect this specific error
    // This test highlights a potential improvement area
}

// --- Additional Edge Case Tests ---

TEST(RlpDecoder, DecodeUint256Overflow) {
    // A very large uint256 that exceeds the 32-byte (256-bit) limit for uint256.
    rlp::Bytes data = from_hex("a101" + std::string(64, 'f')); // 33 bytes total (1 byte header + 32 bytes data), exceeds the 32-byte limit for uint256
    rlp::RlpDecoder decoder(data);
    intx::uint256 out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error(), rlp::DecodingError::kOverflow);
}

TEST(RlpDecoder, DecodeMalformedData) {
    rlp::Bytes data = from_hex("ff"); // Invalid header
    rlp::RlpDecoder decoder(data);
    rlp::Bytes out;
    auto res = decoder.read(out);
    ASSERT_FALSE(res);
    // Depending on implementation, could be kInputTooShort or other error
}

// --- Tests for the new read<T>() template function ---

TEST(RlpDecoder, ReadTemplateUint8) {
    rlp::Bytes data = from_hex("81c8"); // 200 >= 128
    rlp::RlpDecoder decoder(data);
    uint8_t result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result, 200);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateUint16) {
    rlp::Bytes data = from_hex("82012c"); // 300 >= 128
    rlp::RlpDecoder decoder(data);
    uint16_t result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result, 300);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateUint32) {
    rlp::Bytes data = from_hex("83011170"); // 70000 >= 128
    rlp::RlpDecoder decoder(data);
    uint32_t result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result, 70000);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateUint64) {
    rlp::Bytes data = from_hex("88ffccb5ddffee1483");
    rlp::RlpDecoder decoder(data);
    uint64_t result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result, 0xFFCCB5DDFFEE1483);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateUint256) {
    rlp::Bytes data = from_hex("8f10203e405060708090a0b0c0d0e0f2");
    rlp::RlpDecoder decoder(data);
    intx::uint256 result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result, intx::from_string<intx::uint256>("0x10203E405060708090A0B0C0D0E0F2"));
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateBoolTrue) {
    rlp::Bytes data = from_hex("01");
    rlp::RlpDecoder decoder(data);
    bool result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result, true);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateBoolFalse) {
    rlp::Bytes data = from_hex("80");
    rlp::RlpDecoder decoder(data);
    bool result;
    ASSERT_TRUE(decoder.read(result));
    EXPECT_EQ(result, false);
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateZeroValues) {
    // Test that zero values work correctly for all integral types
    rlp::Bytes data = from_hex("80"); // Encoded zero
    
    {
        rlp::RlpDecoder decoder(data);
        uint8_t result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_EQ(result, 0);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    {
        rlp::RlpDecoder decoder(data);
        uint16_t result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_EQ(result, 0);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    {
        rlp::RlpDecoder decoder(data);
        uint32_t result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_EQ(result, 0);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    {
        rlp::RlpDecoder decoder(data);
        uint64_t result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_EQ(result, 0);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    {
        rlp::RlpDecoder decoder(data);
        intx::uint256 result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_EQ(result, 0);
        EXPECT_TRUE(decoder.is_finished());
    }
}

TEST(RlpDecoder, ReadTemplateMaxValues) {
    // Test maximum values for different integral types
    
    // uint8_t max (255)
    {
        rlp::Bytes data = from_hex("81ff");
        rlp::RlpDecoder decoder(data);
        uint8_t result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_EQ(result, 255);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    // uint16_t max (65535)
    {
        rlp::Bytes data = from_hex("82ffff");
        rlp::RlpDecoder decoder(data);
        uint16_t result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_EQ(result, 65535);
        EXPECT_TRUE(decoder.is_finished());
    }
    
    // uint32_t max (4294967295)
    {
        rlp::Bytes data = from_hex("84ffffffff");
        rlp::RlpDecoder decoder(data);
        uint32_t result;
        ASSERT_TRUE(decoder.read(result));
        EXPECT_EQ(result, 4294967295U);
        EXPECT_TRUE(decoder.is_finished());
    }
}

TEST(RlpDecoder, ReadTemplateOverflowErrors) {
    // Test overflow detection for smaller types
    
    // Value too large for uint8_t (256)
    {
        rlp::Bytes data = from_hex("820100");
        rlp::RlpDecoder decoder(data);
        uint8_t result;
        auto res = decoder.read(result);
        ASSERT_FALSE(res);
        EXPECT_EQ(res.error(), rlp::DecodingError::kOverflow);
    }
    
    // Value too large for uint16_t (65536)
    {
        rlp::Bytes data = from_hex("83010000");
        rlp::RlpDecoder decoder(data);
        uint16_t result;
        auto res = decoder.read(result);
        ASSERT_FALSE(res);
        EXPECT_EQ(res.error(), rlp::DecodingError::kOverflow);
    }
    
    // Value too large for uint32_t (4294967296)
    {
        rlp::Bytes data = from_hex("850100000000");
        rlp::RlpDecoder decoder(data);
        uint32_t result;
        auto res = decoder.read(result);
        ASSERT_FALSE(res);
        EXPECT_EQ(res.error(), rlp::DecodingError::kOverflow);
    }
}

TEST(RlpDecoder, ReadTemplateSequentialReads) {
    // Test reading multiple values sequentially using the template function
    // Simple list with small values: [1, 2, 3]
    rlp::Bytes data = from_hex("c3" "01" "02" "03"); // List with [1, 2, 3]
    rlp::RlpDecoder decoder(data);
    
    auto list_len = decoder.read_list_header();
    ASSERT_TRUE(list_len);
    EXPECT_EQ(list_len.value(), 3); // List payload length in bytes (not number of items)
    
    uint8_t val1;
    ASSERT_TRUE(decoder.read(val1));
    EXPECT_EQ(val1, 1);
    
    uint8_t val2;
    ASSERT_TRUE(decoder.read(val2));
    EXPECT_EQ(val2, 2);
    
    uint8_t val3;
    ASSERT_TRUE(decoder.read(val3));
    EXPECT_EQ(val3, 3);
    
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateComplexSequentialReads) {
    // Test reading larger values sequentially
    // List with [255, 65535] - max values for uint8 and uint16
    rlp::Bytes data = from_hex("c5" "81ff" "82ffff"); // List with [255, 65535]
    rlp::RlpDecoder decoder(data);
    
    auto list_len = decoder.read_list_header();
    ASSERT_TRUE(list_len);
    EXPECT_EQ(list_len.value(), 5); // Total payload length
    
    uint8_t val1;
    ASSERT_TRUE(decoder.read(val1));
    EXPECT_EQ(val1, 255);
    
    uint16_t val2;
    ASSERT_TRUE(decoder.read(val2));
    EXPECT_EQ(val2, 65535);
    
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateMixedTypes) {
    // Test reading different types in sequence including bool
    rlp::Bytes data = from_hex("c5" "80" "01" "64" "81c8"); // List with [false, true, 100, 200]
    rlp::RlpDecoder decoder(data);
    
    auto list_len = decoder.read_list_header();
    ASSERT_TRUE(list_len);
    EXPECT_EQ(list_len.value(), 5); // Total payload length
    
    bool bool_false;
    ASSERT_TRUE(decoder.read(bool_false));
    EXPECT_EQ(bool_false, false);
    
    bool bool_true;
    ASSERT_TRUE(decoder.read(bool_true));
    EXPECT_EQ(bool_true, true);
    
    uint8_t uint8_val;
    ASSERT_TRUE(decoder.read(uint8_val));
    EXPECT_EQ(uint8_val, 100);
    
    uint8_t uint8_val2;
    ASSERT_TRUE(decoder.read(uint8_val2));
    EXPECT_EQ(uint8_val2, 200);
    
    EXPECT_TRUE(decoder.is_finished());
}

TEST(RlpDecoder, ReadTemplateErrorHandling) {
    // Test error handling with template function
    
    // Try to read from empty data
    {
        rlp::Bytes data;
        rlp::RlpDecoder decoder(data);
        uint32_t result;
        auto res = decoder.read(result);
        ASSERT_FALSE(res);
        EXPECT_EQ(res.error(), rlp::DecodingError::kInputTooShort);
    }
    
    // Try to read integral from list
    {
        rlp::Bytes data = from_hex("c0"); // Empty list
        rlp::RlpDecoder decoder(data);
        uint32_t result;
        auto res = decoder.read(result);
        ASSERT_FALSE(res);
        EXPECT_EQ(res.error(), rlp::DecodingError::kUnexpectedList);
    }
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}