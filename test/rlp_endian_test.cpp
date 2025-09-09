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

TEST(RlpEndian, Uint8Tests) {
    const uint8_t val = 0xAB;
    const auto bytes = rlp::endian::to_big_compact(val);
    EXPECT_EQ(bytes.size(), 1);
    EXPECT_EQ(bytes[0], 0xAB);
    
    uint8_t restored;
    const bool success = rlp::endian::from_big_compact(bytes, restored);
    EXPECT_TRUE(success);
    EXPECT_EQ(restored, val);
}

TEST(RlpEndian, Uint16Tests) {
    const uint16_t val = 0xABCD;
    const auto bytes = rlp::endian::to_big_compact(val);
    EXPECT_EQ(bytes.size(), 2);
    EXPECT_EQ(bytes[0], 0xAB);
    EXPECT_EQ(bytes[1], 0xCD);
    
    uint16_t restored;
    const bool success = rlp::endian::from_big_compact(bytes, restored);
    EXPECT_TRUE(success);
    EXPECT_EQ(restored, val);
    
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
    
    uint32_t restored;
    const bool success = rlp::endian::from_big_compact(bytes, restored);
    EXPECT_TRUE(success);
    EXPECT_EQ(restored, val);
    
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
    
    uint64_t restored;
    const bool success = rlp::endian::from_big_compact(bytes, restored);
    EXPECT_TRUE(success);
    EXPECT_EQ(restored, val);
    
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
    
    std::vector<uint32_t> restored_vec;
    for (const auto& bytes : vec_bytes) {
        uint32_t val;
        const bool success = rlp::endian::from_big_compact(bytes, val);
        EXPECT_TRUE(success);
        restored_vec.push_back(val);
    }
    
    EXPECT_EQ(restored_vec, vec);
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
    
    std::array<uint16_t, 4> restored_arr;
    for (size_t i = 0; i < arr_bytes.size(); ++i) {
        uint16_t val = 0;
        if (!arr_bytes[i].empty()) {
            const bool success = rlp::endian::from_big_compact(arr_bytes[i], val);
            EXPECT_TRUE(success);
        }
        restored_arr[i] = val;
    }
    
    EXPECT_EQ(restored_arr, arr);
}

TEST(RlpEndian, CArrayOperations) {
    const uint32_t c_array[] = {0xDEADBEEF, 0xCAFEBABE, 0x12345678};
    const size_t array_size = sizeof(c_array) / sizeof(c_array[0]);
    
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
    
    uint32_t restored_c_array[array_size];
    for (size_t i = 0; i < array_size; ++i) {
        const bool success = rlp::endian::from_big_compact(c_array_bytes[i], restored_c_array[i]);
        EXPECT_TRUE(success);
    }
    
    for (size_t i = 0; i < array_size; ++i) {
        EXPECT_EQ(restored_c_array[i], c_array[i]);
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
        const auto bytes = rlp::endian::to_big_compact(val);
        uint8_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, val) << "Failed for uint8_t value: 0x" << std::hex << static_cast<int>(val);
    }
    
    for (uint16_t val : {0x01, 0xFF, 0x0100, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF}) {
        const auto bytes = rlp::endian::to_big_compact(val);
        uint16_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, val) << "Failed for uint16_t value: 0x" << std::hex << val;
    }
    
    for (uint32_t val : {0x01U, 0xFFU, 0x0100U, 0xFFFFU, 0x010000U, 0x7FFFFFFFU, 0x80000000U, 0xFFFFFFFEU, 0xFFFFFFFFU}) {
        const auto bytes = rlp::endian::to_big_compact(val);
        uint32_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, val) << "Failed for uint32_t value: 0x" << std::hex << val;
    }
    
    for (uint64_t val : {0x01ULL, 0xFFULL, 0x0100ULL, 0xFFFFULL, 0x010000ULL, 0xFFFFFFULL, 
                         0x01000000ULL, 0xFFFFFFFFULL, 0x0100000000ULL, 0x7FFFFFFFFFFFFFFFULL, 
                         0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFFULL}) {
        const auto bytes = rlp::endian::to_big_compact(val);
        uint64_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, val) << "Failed for uint64_t value: 0x" << std::hex << val;
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
    for (uint8_t i = 0; i < 255; ++i) {
        large_vec.push_back(i);
    }
    
    for (size_t i = 0; i < large_vec.size(); ++i) {
        const auto bytes = rlp::endian::to_big_compact(large_vec[i]);
        uint8_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, large_vec[i]) << "Failed at index " << i;
    }
}

TEST(RlpEndian, ArrayOperationsExtended) {
    std::array<uint8_t, 256> byte_array;
    for (size_t i = 0; i < byte_array.size(); ++i) {
        byte_array[i] = static_cast<uint8_t>(i);
    }
    
    for (size_t i = 0; i < byte_array.size(); ++i) {
        const auto bytes = rlp::endian::to_big_compact(byte_array[i]);
        uint8_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, byte_array[i]) << "Failed at index " << i;
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
        const auto bytes = rlp::endian::to_big_compact(pattern_array[i]);
        uint64_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, pattern_array[i]) << "Failed at pattern array index " << i;
    }
}

TEST(RlpEndian, CArrayOperationsExtended) {
    const uint32_t c_array[10] = {
        0x00000000, 0x00000001, 0x000000FF, 0x0000FFFF, 0x00FFFFFF,
        0xFFFFFFFF, 0x12345678, 0x9ABCDEF0, 0xDEADBEEF, 0xCAFEBABE
    };
    
    const size_t c_array_size = sizeof(c_array) / sizeof(c_array[0]);
    
    for (size_t i = 0; i < c_array_size; ++i) {
        const auto bytes = rlp::endian::to_big_compact(c_array[i]);
        uint32_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, c_array[i]) << "Failed at C array index " << i;
    }
    
    const uint16_t multi_array[3][4] = {
        {0x0000, 0x0001, 0x00FF, 0xFFFF},
        {0x1234, 0x5678, 0x9ABC, 0xDEF0},
        {0xAAAA, 0x5555, 0xF0F0, 0x0F0F}
    };
    
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            const auto bytes = rlp::endian::to_big_compact(multi_array[i][j]);
            uint16_t restored;
            const bool success = rlp::endian::from_big_compact(bytes, restored);
            EXPECT_TRUE(success);
            EXPECT_EQ(restored, multi_array[i][j]) << "Failed at multi-array [" << i << "][" << j << "]";
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
        
        uint64_t restored;
        const bool success = rlp::endian::from_big_compact(bytes, restored);
        EXPECT_TRUE(success);
        EXPECT_EQ(restored, test_case.value) 
            << "Restoration failed for value 0x" << std::hex << test_case.value;
    }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
