#include <gtest/gtest.h>
#include <rlp_encoder.hpp>
#include <rlp_decoder.hpp>
#include <endian.hpp>
#include <vector>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>

using namespace rlp;

// --- Endian Tests ---

namespace {
template <typename T>
bool expect_roundtrip(const T& val) {
    const auto bytes = rlp::endian::to_big_compact(val);
    T restored{};
    const auto res = rlp::endian::from_big_compact(bytes, restored);
    if (!res.has_value()) return false;
    return restored == val;
}
} // anonymous

TEST(RlpEndian, Uint8Tests) {
    const uint8_t val = 0xAB;
    const auto bytes = rlp::endian::to_big_compact(val);
    EXPECT_EQ(bytes.size(), 1);
    EXPECT_EQ(bytes[0], 0xAB);
    
    ASSERT_TRUE(expect_roundtrip<uint8_t>(val));
}

TEST(RlpEndian, Uint16Tests) {
    const uint16_t val = 0xABCD;
    const auto bytes = rlp::endian::to_big_compact(val);
    EXPECT_EQ(bytes.size(), 2);
    EXPECT_EQ(bytes[0], 0xAB);
    EXPECT_EQ(bytes[1], 0xCD);
    
    ASSERT_TRUE(expect_roundtrip<uint16_t>(val));
    
    const uint16_t val_compact = 0x00CD;
    const auto bytes_compact = rlp::endian::to_big_compact(val_compact);
    EXPECT_EQ(bytes_compact.size(), 1);
    EXPECT_EQ(bytes_compact[0], 0xCD);
}

TEST(RlpEndian, Uint32Tests) {
    const uint32_t val = 0xABCDEF12;
    const auto bytes = rlp::endian::to_big_compact(val);
    EXPECT_EQ(bytes.size(), 4);
    EXPECT_EQ(bytes[0], 0xAB);
    EXPECT_EQ(bytes[1], 0xCD);
    EXPECT_EQ(bytes[2], 0xEF);
    EXPECT_EQ(bytes[3], 0x12);
    
    ASSERT_TRUE(expect_roundtrip<uint32_t>(val));
    
    const uint32_t val_compact = 0x0000EF12;
    const auto bytes_compact = rlp::endian::to_big_compact(val_compact);
    EXPECT_EQ(bytes_compact.size(), 2);
    EXPECT_EQ(bytes_compact[0], 0xEF);
    EXPECT_EQ(bytes_compact[1], 0x12);
}

TEST(RlpEndian, Uint64Tests) {
    const uint64_t val = 0xABCDEF1234567890ULL;
    const auto bytes = rlp::endian::to_big_compact(val);
    EXPECT_EQ(bytes.size(), 8);
    EXPECT_EQ(bytes[0], 0xAB);
    EXPECT_EQ(bytes[1], 0xCD);
    EXPECT_EQ(bytes[2], 0xEF);
    EXPECT_EQ(bytes[3], 0x12);
    EXPECT_EQ(bytes[4], 0x34);
    EXPECT_EQ(bytes[5], 0x56);
    EXPECT_EQ(bytes[6], 0x78);
    EXPECT_EQ(bytes[7], 0x90);
    
    ASSERT_TRUE(expect_roundtrip<uint64_t>(val));
    
    const uint64_t val_compact = 0x0000000034567890ULL;
    const auto bytes_compact = rlp::endian::to_big_compact(val_compact);
    EXPECT_EQ(bytes_compact.size(), 4);
    EXPECT_EQ(bytes_compact[0], 0x34);
    EXPECT_EQ(bytes_compact[1], 0x56);
    EXPECT_EQ(bytes_compact[2], 0x78);
    EXPECT_EQ(bytes_compact[3], 0x90);
}

TEST(RlpEndian, ZeroValues) {
    const uint8_t val8 = 0;
    const auto bytes8 = rlp::endian::to_big_compact(val8);
    EXPECT_TRUE(bytes8.empty());
    
    const uint16_t val16 = 0;
    const auto bytes16 = rlp::endian::to_big_compact(val16);
    EXPECT_TRUE(bytes16.empty());
    
    const uint32_t val32 = 0;
    const auto bytes32 = rlp::endian::to_big_compact(val32);
    EXPECT_TRUE(bytes32.empty());
    
    const uint64_t val64 = 0;
    const auto bytes64 = rlp::endian::to_big_compact(val64);
    EXPECT_TRUE(bytes64.empty());
}

TEST(RlpEndian, VectorOperations) {
    const std::vector<uint32_t> vec = {0xABCDEF12, 0x34567890, 0x0000FFEE};
    
    std::vector<rlp::Bytes> vec_bytes;
    for (const auto& val : vec) {
        vec_bytes.push_back(rlp::endian::to_big_compact(val));
    }
    
    EXPECT_EQ(vec_bytes.size(), 3);
    EXPECT_EQ(vec_bytes[0].size(), 4);
    EXPECT_EQ(vec_bytes[1].size(), 4);
    EXPECT_EQ(vec_bytes[2].size(), 2);
    
    EXPECT_EQ(vec_bytes[0][0], 0xAB);
    EXPECT_EQ(vec_bytes[0][1], 0xCD);
    EXPECT_EQ(vec_bytes[0][2], 0xEF);
    EXPECT_EQ(vec_bytes[0][3], 0x12);
    
    EXPECT_EQ(vec_bytes[2][0], 0xFF);
    EXPECT_EQ(vec_bytes[2][1], 0xEE);
    
    for (const auto& v : vec) {
        ASSERT_TRUE(expect_roundtrip<uint32_t>(v));
    }
}

TEST(RlpEndian, ArrayOperations) {
    const std::array<uint16_t, 4> arr = {0xABCD, 0x1234, 0x00EF, 0x0000};
    
    std::array<rlp::Bytes, 4> arr_bytes;
    for (size_t i = 0; i < arr.size(); ++i) {
        arr_bytes[i] = rlp::endian::to_big_compact(arr[i]);
    }
    
    EXPECT_EQ(arr_bytes[0].size(), 2);
    EXPECT_EQ(arr_bytes[1].size(), 2);
    EXPECT_EQ(arr_bytes[2].size(), 1);
    EXPECT_TRUE(arr_bytes[3].empty());
    
    EXPECT_EQ(arr_bytes[0][0], 0xAB);
    EXPECT_EQ(arr_bytes[0][1], 0xCD);
    EXPECT_EQ(arr_bytes[2][0], 0xEF);
    
    for (size_t i = 0; i < arr.size(); ++i) {
        ASSERT_TRUE(expect_roundtrip<uint16_t>(arr[i]));
    }
}

TEST(RlpEndian, CArrayOperations) {
    constexpr uint32_t c_array[] = {0xDEADBEEF, 0xCAFEBABE, 0x12345678};
    constexpr size_t array_size  = sizeof(c_array) / sizeof(c_array[0]);
    
    rlp::Bytes c_array_bytes[array_size];
    for (size_t i = 0; i < array_size; ++i) {
        c_array_bytes[i] = rlp::endian::to_big_compact(c_array[i]);
    }
    
    EXPECT_EQ(c_array_bytes[0].size(), 4);
    EXPECT_EQ(c_array_bytes[1].size(), 4);
    EXPECT_EQ(c_array_bytes[2].size(), 4);
    
    EXPECT_EQ(c_array_bytes[0][0], 0xDE);
    EXPECT_EQ(c_array_bytes[0][1], 0xAD);
    EXPECT_EQ(c_array_bytes[0][2], 0xBE);
    EXPECT_EQ(c_array_bytes[0][3], 0xEF);
    
    EXPECT_EQ(c_array_bytes[1][0], 0xCA);
    EXPECT_EQ(c_array_bytes[1][1], 0xFE);
    EXPECT_EQ(c_array_bytes[1][2], 0xBA);
    EXPECT_EQ(c_array_bytes[1][3], 0xBE);
    
    for (size_t i = 0; i < array_size; ++i) {
        ASSERT_TRUE(expect_roundtrip<uint32_t>(c_array[i]));
    }
}

TEST(RlpEndian, EdgeCases) {
    const uint8_t max8 = 0xFF;
    const auto bytes_max8 = rlp::endian::to_big_compact(max8);
    EXPECT_EQ(bytes_max8.size(), 1);
    EXPECT_EQ(bytes_max8[0], 0xFF);
    
    const uint16_t max16 = 0xFFFF;
    const auto bytes_max16 = rlp::endian::to_big_compact(max16);
    EXPECT_EQ(bytes_max16.size(), 2);
    EXPECT_EQ(bytes_max16[0], 0xFF);
    EXPECT_EQ(bytes_max16[1], 0xFF);
    
    const uint32_t max32 = 0xFFFFFFFF;
    const auto bytes_max32 = rlp::endian::to_big_compact(max32);
    EXPECT_EQ(bytes_max32.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(bytes_max32[i], 0xFF);
    }
    
    const uint64_t max64 = 0xFFFFFFFFFFFFFFFFULL;
    const auto bytes_max64 = rlp::endian::to_big_compact(max64);
    EXPECT_EQ(bytes_max64.size(), 8);
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_EQ(bytes_max64[i], 0xFF);
    }
    
    const uint8_t min8 = 1;
    const auto bytes_min8 = rlp::endian::to_big_compact(min8);
    EXPECT_EQ(bytes_min8.size(), 1);
    EXPECT_EQ(bytes_min8[0], 0x01);
    
    const uint16_t min16 = 0x0100;
    const auto bytes_min16 = rlp::endian::to_big_compact(min16);
    EXPECT_EQ(bytes_min16.size(), 2);
    EXPECT_EQ(bytes_min16[0], 0x01);
    EXPECT_EQ(bytes_min16[1], 0x00);
    
    const uint32_t min32 = 0x01000000;
    const auto bytes_min32 = rlp::endian::to_big_compact(min32);
    EXPECT_EQ(bytes_min32.size(), 4);
    EXPECT_EQ(bytes_min32[0], 0x01);
    EXPECT_EQ(bytes_min32[1], 0x00);
    EXPECT_EQ(bytes_min32[2], 0x00);
    EXPECT_EQ(bytes_min32[3], 0x00);
}

TEST(RlpEndian, BoundaryValues) {
    for (uint8_t val : {0x01, 0x7F, 0x80, 0xFE, 0xFF}) {
        ASSERT_TRUE(expect_roundtrip<uint8_t>(val)) << "Failed for uint8_t value: 0x" << std::hex << static_cast<int>(val);
    }
    
    for (uint16_t val : {0x01, 0xFF, 0x0100, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF}) {
        ASSERT_TRUE(expect_roundtrip<uint16_t>(val)) << "Failed for uint16_t value: 0x" << std::hex << val;
    }
    
    for (uint32_t val : {0x01U, 0xFFU, 0x0100U, 0xFFFFU, 0x010000U, 0x7FFFFFFFU, 0x80000000U, 0xFFFFFFFEU, 0xFFFFFFFFU}) {
        ASSERT_TRUE(expect_roundtrip<uint32_t>(val)) << "Failed for uint32_t value: 0x" << std::hex << val;
    }
    
    for (uint64_t val : {0x01ULL, 0xFFULL, 0x0100ULL, 0xFFFFULL, 0x010000ULL, 0xFFFFFFULL, 
                         0x01000000ULL, 0xFFFFFFFFULL, 0x0100000000ULL, 0x7FFFFFFFFFFFFFFFULL, 
                         0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFFULL}) {
        ASSERT_TRUE(expect_roundtrip<uint64_t>(val)) << "Failed for uint64_t value: 0x" << std::hex << val;
    }
}

TEST(RlpEndian, VectorOperationsExtended) {
    const std::vector<uint32_t> empty_vec;
    EXPECT_TRUE(empty_vec.empty());
    
    const std::vector<uint16_t> single_vec = {0xABCD};
    const auto bytes_single = rlp::endian::to_big_compact(single_vec[0]);
    EXPECT_EQ(bytes_single.size(), 2);
    EXPECT_EQ(bytes_single[0], 0xAB);
    EXPECT_EQ(bytes_single[1], 0xCD);
    
    std::vector<uint8_t> large_vec;
    for (size_t i = 0; i < 255; ++i) {
        large_vec.push_back(static_cast<uint8_t>(i));
    }
    
    for (size_t i = 0; i < large_vec.size(); ++i) {
        ASSERT_TRUE(expect_roundtrip<uint8_t>(large_vec[i])) << "Failed at index " << i;
    }
}

TEST(RlpEndian, ArrayOperationsExtended) {
    std::array<uint8_t, 256> byte_array;
    // The assignment is safe here because i only takes values from 0 to 255.
    for (size_t i = 0; i < byte_array.size(); ++i) {
        byte_array[i] = i;
    }
    
    for (size_t i = 0; i < byte_array.size(); ++i) {
        ASSERT_TRUE(expect_roundtrip<uint8_t>(byte_array[i])) << "Failed at index " << i;
    }
    
    const std::array<uint64_t, 8> pattern_array = {
        0x0000000000000000ULL,
        0x0000000000000001ULL,
        0x00000000000000FFULL,
        0x000000000000FFFFULL,
        0x0000000000FFFFFFULL,
        0x00000000FFFFFFFFULL,
        0xAAAAAAAAAAAAAAAAULL,
        0xFFFFFFFFFFFFFFFFULL
    };
    
    for (size_t i = 0; i < pattern_array.size(); ++i) {
        ASSERT_TRUE(expect_roundtrip<uint64_t>(pattern_array[i])) << "Failed at pattern array index " << i;
    }
}

TEST(RlpEndian, CArrayOperationsExtended) {
    const uint32_t c_array[10] = {
        0x00000000, 0x00000001, 0x000000FF, 0x0000FFFF, 0x00FFFFFF,
        0xFFFFFFFF, 0x12345678, 0x9ABCDEF0, 0xDEADBEEF, 0xCAFEBABE
    };
    
    const size_t c_array_size = sizeof(c_array) / sizeof(c_array[0]);
    
    for (size_t i = 0; i < c_array_size; ++i) {
        ASSERT_TRUE(expect_roundtrip<uint32_t>(c_array[i])) << "Failed at C array index " << i;
    }
    
    const uint16_t multi_array[3][4] = {
        {0x0000, 0x0001, 0x00FF, 0xFFFF},
        {0x1234, 0x5678, 0x9ABC, 0xDEF0},
        {0xAAAA, 0x5555, 0xF0F0, 0x0F0F}
    };
    
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            const auto bytes = rlp::endian::to_big_compact(multi_array[i][j]);
            ASSERT_TRUE(expect_roundtrip<uint16_t>(multi_array[i][j])) << "Failed at multi-array [" << i << "][" << j << "]";
        }
    }
}

TEST(RlpEndian, CompactRepresentation) {
    struct TestCase {
        const uint64_t value;
        const size_t expected_size;
        const std::vector<uint8_t> expected_bytes;
    };
    
    const std::vector<TestCase> test_cases = {
        {0x0000000000000000ULL, 0, {}},
        {0x0000000000000001ULL, 1, {0x01}},
        {0x00000000000000FFULL, 1, {0xFF}},
        {0x000000000000FF00ULL, 2, {0xFF, 0x00}},
        {0x0000000000FF0000ULL, 3, {0xFF, 0x00, 0x00}},
        {0x00000000FF000000ULL, 4, {0xFF, 0x00, 0x00, 0x00}},
        {0x000000FF00000000ULL, 5, {0xFF, 0x00, 0x00, 0x00, 0x00}},
        {0x0000FF0000000000ULL, 6, {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {0x00FF000000000000ULL, 7, {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {0xFF00000000000000ULL, 8, {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {0x123456789ABCDEF0ULL, 8, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}}
    };
    
    for (const auto& test_case : test_cases) {
        const auto bytes = rlp::endian::to_big_compact(test_case.value);
        EXPECT_EQ(bytes.size(), test_case.expected_size) 
            << "Size mismatch for value 0x" << std::hex << test_case.value;
        
        for (size_t i = 0; i < test_case.expected_bytes.size(); ++i) {
            EXPECT_EQ(bytes[i], test_case.expected_bytes[i])
                << "Byte mismatch at index " << i << " for value 0x" << std::hex << test_case.value;
        }
        
        ASSERT_TRUE(expect_roundtrip<uint64_t>(test_case.value)) 
            << "Restoration failed for value 0x" << std::hex << test_case.value;
    }
}

TEST(RlpEndian, DeserializationFailure) {
    // Test deserializing bytes that are too large for the target type.
    
    // Case 1: >1 byte into uint8_t
    const rlp::Bytes oversized_for_u8{0x01, 0x02};
    uint8_t restored_u8;
    const auto result_u8 = rlp::endian::from_big_compact(oversized_for_u8, restored_u8);
    ASSERT_FALSE(result_u8.has_value());
    EXPECT_EQ(result_u8.error(), rlp::DecodingError::kOverflow);

    // Case 2: >2 bytes into uint16_t
    const rlp::Bytes oversized_for_u16{0x01, 0x02, 0x03};
    uint16_t restored_u16;
    const auto result_u16 = rlp::endian::from_big_compact(oversized_for_u16, restored_u16);
    ASSERT_FALSE(result_u16.has_value());
    EXPECT_EQ(result_u16.error(), rlp::DecodingError::kOverflow);

    // Case 3: >4 bytes into uint32_t
    const rlp::Bytes oversized_for_u32{0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t restored_u32;
    const auto result_u32 = rlp::endian::from_big_compact(oversized_for_u32, restored_u32);
    ASSERT_FALSE(result_u32.has_value());
    EXPECT_EQ(result_u32.error(), rlp::DecodingError::kOverflow);

    // Case 4: >8 bytes into uint64_t
    const rlp::Bytes oversized_for_u64{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    uint64_t restored_u64;
    const auto result_u64 = rlp::endian::from_big_compact(oversized_for_u64, restored_u64);
    ASSERT_FALSE(result_u64.has_value());
    EXPECT_EQ(result_u64.error(), rlp::DecodingError::kOverflow);
}

TEST(RlpEndian, Uint256Zero) {
    const intx::uint256 zero = 0;
    const auto bytes_zero = rlp::endian::to_big_compact(zero);
    EXPECT_TRUE(bytes_zero.empty());
    intx::uint256 restored_zero;
    const auto res_zero = rlp::endian::from_big_compact(bytes_zero, restored_zero);
    EXPECT_TRUE(res_zero.has_value());
    EXPECT_EQ(restored_zero, zero);
}

TEST(RlpEndian, Uint256Small) {
    const intx::uint256 small = static_cast<uint64_t>(0x12345678ULL);
    const auto bytes_small = rlp::endian::to_big_compact(small);
    EXPECT_EQ(bytes_small.size(), 4);
    intx::uint256 restored_small;
    const auto res_small = rlp::endian::from_big_compact(bytes_small, restored_small);
    EXPECT_TRUE(res_small.has_value());
    EXPECT_EQ(restored_small, small);
}

TEST(RlpEndian, Uint256Large) {
    const intx::uint256 large = (intx::uint256(1) << 200) | (intx::uint256(0x12345678ULL) << 32) | intx::uint256(0x9ABCDEF0ULL);
    const auto bytes_large = rlp::endian::to_big_compact(large);
    intx::uint256 restored_large;
    const auto res_large = rlp::endian::from_big_compact(bytes_large, restored_large);
    EXPECT_TRUE(res_large.has_value());
    EXPECT_EQ(restored_large, large);
}

TEST(RlpEndian, Uint256Overflow) {
    // 33 non-zero bytes should be rejected for uint256
    rlp::Bytes too_large(33, static_cast<uint8_t>(0xFF));
    intx::uint256 out_overflow;
    const auto res_overflow = rlp::endian::from_big_compact(too_large, out_overflow);
    ASSERT_FALSE(res_overflow.has_value());
    EXPECT_EQ(res_overflow.error(), rlp::DecodingError::kOverflow);
}

TEST(RlpEndian, Uint256LeadingZero) {
    // Leading zero: first byte zero and length > 1
    rlp::Bytes leading_zero{0x00, 0x01};
    intx::uint256 out_lz;
    const auto res_lz = rlp::endian::from_big_compact(leading_zero, out_lz);
    ASSERT_FALSE(res_lz.has_value());
    EXPECT_EQ(res_lz.error(), rlp::DecodingError::kLeadingZero);
}

TEST(RlpEndian, LeadingZeroSmallTypes) {
    // Leading zero should be rejected for fixed-size unsigned types when length > 1
    rlp::Bytes leading{0x00, 0x01};

    uint8_t out8;
    const auto r8 = rlp::endian::from_big_compact(leading, out8);
    ASSERT_FALSE(r8.has_value());
    EXPECT_EQ(r8.error(), rlp::DecodingError::kLeadingZero);

    uint16_t out16;
    const auto r16 = rlp::endian::from_big_compact(leading, out16);
    ASSERT_FALSE(r16.has_value());
    EXPECT_EQ(r16.error(), rlp::DecodingError::kLeadingZero);

    uint32_t out32;
    const auto r32 = rlp::endian::from_big_compact(leading, out32);
    ASSERT_FALSE(r32.has_value());
    EXPECT_EQ(r32.error(), rlp::DecodingError::kLeadingZero);

    uint64_t out64;
    const auto r64 = rlp::endian::from_big_compact(leading, out64);
    ASSERT_FALSE(r64.has_value());
    EXPECT_EQ(r64.error(), rlp::DecodingError::kLeadingZero);
}

TEST(RlpEndian, SingleZeroByteDeserializesToZero) {
    rlp::Bytes single_zero{0x00};

    uint8_t out8 = 0xFF;
    const auto r8 = rlp::endian::from_big_compact(single_zero, out8);
    EXPECT_TRUE(r8.has_value());
    EXPECT_EQ(out8, static_cast<uint8_t>(0));

    uint16_t out16 = 0xFFFF;
    const auto r16 = rlp::endian::from_big_compact(single_zero, out16);
    EXPECT_TRUE(r16.has_value());
    EXPECT_EQ(out16, static_cast<uint16_t>(0));

    uint32_t out32 = 0xFFFFFFFF;
    const auto r32 = rlp::endian::from_big_compact(single_zero, out32);
    EXPECT_TRUE(r32.has_value());
    EXPECT_EQ(out32, static_cast<uint32_t>(0));

    uint64_t out64 = 0xFFFFFFFFFFFFFFFFULL;
    const auto r64 = rlp::endian::from_big_compact(single_zero, out64);
    EXPECT_TRUE(r64.has_value());
    EXPECT_EQ(out64, static_cast<uint64_t>(0));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
