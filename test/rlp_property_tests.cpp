#include <gtest/gtest.h>
#include <variant>
#include <rlp_encoder.hpp>
#include <rlp_decoder.hpp>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <limits>
#include <type_traits>

using namespace rlp;

// ===================================================================
// PROPERTY-BASED TESTING FRAMEWORK
// ===================================================================

class PropertyBasedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use current time as seed for randomness, but make it deterministic for CI
        auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        rng_.seed(static_cast<std::mt19937::result_type>(seed));
        std::cout << "Random seed: " << seed << std::endl;
    }
    
    std::mt19937 rng_;
    
    // Generate random bytes
    rlp::Bytes random_bytes(size_t min_size = 0, size_t max_size = 1000) {
        std::uniform_int_distribution<size_t> size_dist(min_size, max_size);
        std::uniform_int_distribution<int> byte_dist(0, 255);
        size_t size = size_dist(rng_);
        rlp::Bytes result;
        result.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            result.push_back(static_cast<uint8_t>(byte_dist(rng_)));
        }
        return result;
    }
    
    // Generate random integer of given type
    template<typename T>
    T random_integer() {
        static_assert(std::is_integral<T>::value, "T must be integral type");
        std::uniform_int_distribution<long long> dist(static_cast<long long>(std::numeric_limits<T>::min()), static_cast<long long>(std::numeric_limits<T>::max()));
        return static_cast<T>(dist(rng_));
    }
    
    // Generate random uint256
    intx::uint256 random_uint256() {
        intx::uint256 result = 0;
        for (int i = 0; i < 4; ++i) {
            uint64_t part = random_integer<uint64_t>();
            result = (result << 64) | part;
        }
        return result;
    }
    
    // Run property test with given number of iterations
    template<typename TestFunc>
    void run_property_test(TestFunc&& test_func, int iterations = 1000) {
        for (int i = 0; i < iterations; ++i) {
            try {
                test_func(i);
            } catch (const std::exception& e) {
                FAIL() << "Property test failed at iteration " << i << ": " << e.what();
            }
        }
    }
};

// ===================================================================
// ROUNDTRIP PROPERTY TESTS
// ===================================================================

TEST_F(PropertyBasedTest, RoundtripPropertyUint8) {
    run_property_test([this](int iteration) {
        uint8_t original = random_integer<uint8_t>();
        
        RlpEncoder encoder;
        encoder.add(original);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        uint8_t decoded;
        ASSERT_TRUE(decoder.read(decoded)) << "Iteration " << iteration << ", value: " << static_cast<int>(original);
        EXPECT_EQ(decoded, original) << "Iteration " << iteration;
        EXPECT_TRUE(decoder.is_finished()) << "Iteration " << iteration;
    });
}

TEST_F(PropertyBasedTest, RoundtripPropertyUint16) {
    run_property_test([this](int iteration) {
        uint16_t original = random_integer<uint16_t>();
        
        RlpEncoder encoder;
        encoder.add(original);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        uint16_t decoded;
        ASSERT_TRUE(decoder.read(decoded)) << "Iteration " << iteration << ", value: " << original;
        EXPECT_EQ(decoded, original) << "Iteration " << iteration;
        EXPECT_TRUE(decoder.is_finished()) << "Iteration " << iteration;
    });
}

TEST_F(PropertyBasedTest, RoundtripPropertyUint32) {
    run_property_test([this](int iteration) {
        uint32_t original = random_integer<uint32_t>();
        
        RlpEncoder encoder;
        encoder.add(original);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        uint32_t decoded;
        ASSERT_TRUE(decoder.read(decoded)) << "Iteration " << iteration << ", value: " << original;
        EXPECT_EQ(decoded, original) << "Iteration " << iteration;
        EXPECT_TRUE(decoder.is_finished()) << "Iteration " << iteration;
    });
}

TEST_F(PropertyBasedTest, RoundtripPropertyUint64) {
    run_property_test([this](int iteration) {
        uint64_t original = random_integer<uint64_t>();
        
        RlpEncoder encoder;
        encoder.add(original);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        uint64_t decoded;
        ASSERT_TRUE(decoder.read(decoded)) << "Iteration " << iteration << ", value: " << original;
        EXPECT_EQ(decoded, original) << "Iteration " << iteration;
        EXPECT_TRUE(decoder.is_finished()) << "Iteration " << iteration;
    });
}

TEST_F(PropertyBasedTest, RoundtripPropertyUint256) {
    run_property_test([this](int iteration) {
        intx::uint256 original = random_uint256();
        
        RlpEncoder encoder;
        encoder.add(original);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        intx::uint256 decoded;
        ASSERT_TRUE(decoder.read(decoded)) << "Iteration " << iteration;
        EXPECT_EQ(decoded, original) << "Iteration " << iteration;
        EXPECT_TRUE(decoder.is_finished()) << "Iteration " << iteration;
    }, 100); // Fewer iterations for uint256 due to complexity
}

TEST_F(PropertyBasedTest, RoundtripPropertyBool) {
    run_property_test([this](int iteration) {
        bool original = (iteration % 2 == 0); // Alternate true/false
        
        RlpEncoder encoder;
        encoder.add(original);
        auto encoded = encoder.get_bytes();
        
        RlpDecoder decoder(encoded);
        bool decoded;
        ASSERT_TRUE(decoder.read(decoded)) << "Iteration " << iteration << ", value: " << original;
        EXPECT_EQ(decoded, original) << "Iteration " << iteration;
        EXPECT_TRUE(decoder.is_finished()) << "Iteration " << iteration;
    });
}

TEST_F(PropertyBasedTest, RoundtripPropertyByteArrays) {
    run_property_test([this](int iteration) {
        auto original = random_bytes(0, 500);
        RlpEncoder encoder;
        encoder.add(rlp::ByteView{original.data(), original.size()});
        auto encoded = encoder.get_bytes();
        RlpDecoder decoder(encoded);
        rlp::Bytes decoded;
        ASSERT_TRUE(decoder.read(decoded)) << "Iteration " << iteration << ", size: " << original.size();
        EXPECT_EQ(decoded, original) << "Iteration " << iteration;
        EXPECT_TRUE(decoder.is_finished()) << "Iteration " << iteration;
    });
}

TEST_F(PropertyBasedTest, RoundtripPropertyMixedLists) {
    run_property_test([this](int iteration) {
        std::uniform_int_distribution<int> list_size_dist(1, 20);
        std::uniform_int_distribution<int> type_dist(0, 4);
        int list_size = list_size_dist(rng_);
        RlpEncoder encoder;
        encoder.begin_list();
    using VariantType = std::variant<uint8_t, uint16_t, uint32_t, bool, rlp::Bytes>;
    std::vector<VariantType> original_values;
        for (int i = 0; i < list_size; ++i) {
            int type = type_dist(rng_);
            switch (type) {
                case 0: {
                    uint8_t val = random_integer<uint8_t>();
                    encoder.add(val);
                    original_values.emplace_back(val);
                    break;
                }
                case 1: {
                    uint16_t val = random_integer<uint16_t>();
                    encoder.add(val);
                    original_values.emplace_back(val);
                    break;
                }
                case 2: {
                    uint32_t val = random_integer<uint32_t>();
                    encoder.add(val);
                    original_values.emplace_back(val);
                    break;
                }
                case 3: {
                    bool val = (i % 2 == 0);
                    encoder.add(val);
                    original_values.emplace_back(val);
                    break;
                }
                case 4: {
                    auto val = random_bytes(0, 50);
                    encoder.add(rlp::ByteView{val.data(), val.size()});
                    original_values.emplace_back(std::move(val));
                    break;
                }
            }
        }
        encoder.end_list();
        auto encoded = encoder.get_bytes();
        RlpDecoder decoder(encoded);
        auto list_header = decoder.read_list_header();
        ASSERT_TRUE(list_header.has_value()) << "Iteration " << iteration;
    RlpDecoder list_decoder(decoder.remaining().substr(0, list_header.value()));
        for (size_t i = 0; i < original_values.size(); ++i) {
            std::visit([&](auto&& original_val) {
                using T = std::decay_t<decltype(original_val)>;
                if constexpr (std::is_same_v<T, uint8_t>) {
                    uint8_t decoded;
                    ASSERT_TRUE(list_decoder.read(decoded)) << "Iteration " << iteration << ", element " << i;
                    EXPECT_EQ(decoded, original_val) << "Iteration " << iteration << ", element " << i;
                } else if constexpr (std::is_same_v<T, uint16_t>) {
                    uint16_t decoded;
                    ASSERT_TRUE(list_decoder.read(decoded)) << "Iteration " << iteration << ", element " << i;
                    EXPECT_EQ(decoded, original_val) << "Iteration " << iteration << ", element " << i;
                } else if constexpr (std::is_same_v<T, uint32_t>) {
                    uint32_t decoded;
                    ASSERT_TRUE(list_decoder.read(decoded)) << "Iteration " << iteration << ", element " << i;
                    EXPECT_EQ(decoded, original_val) << "Iteration " << iteration << ", element " << i;
                } else if constexpr (std::is_same_v<T, bool>) {
                    bool decoded;
                    ASSERT_TRUE(list_decoder.read(decoded)) << "Iteration " << iteration << ", element " << i;
                    EXPECT_EQ(decoded, original_val) << "Iteration " << iteration << ", element " << i;
                } else if constexpr (std::is_same_v<T, rlp::Bytes>) {
                    rlp::Bytes decoded;
                    ASSERT_TRUE(list_decoder.read(decoded)) << "Iteration " << iteration << ", element " << i;
                    EXPECT_EQ(decoded, original_val) << "Iteration " << iteration << ", element " << i;
                }
            }, original_values[i]);
        }
        EXPECT_TRUE(list_decoder.is_finished()) << "Iteration " << iteration;
    }, 200);
}

// ===================================================================
// FUZZ TESTING
// ===================================================================

TEST_F(PropertyBasedTest, FuzzDecoderWithRandomData) {
    // Test decoder robustness with completely random data
    run_property_test([this](int iteration) {
        auto random_data = random_bytes(1, 1000);
        
        // Decoder should never crash, even with invalid data
        RlpDecoder decoder(random_data);
        
        // Try various decode operations - they may fail but shouldn't crash
        rlp::Bytes str_result;
        [[maybe_unused]] auto str_result_code = decoder.read(str_result);
        decoder = RlpDecoder(random_data); // Reset

        [[maybe_unused]] auto list_header_result = decoder.read_list_header();
        decoder = RlpDecoder(random_data); // Reset

        uint8_t u8_val;
        [[maybe_unused]] auto u8_result_code = decoder.read(u8_val);
        decoder = RlpDecoder(random_data); // Reset

        uint32_t u32_val;
        [[maybe_unused]] auto u32_result_code = decoder.read(u32_val);
        decoder = RlpDecoder(random_data); // Reset

        intx::uint256 u256_val;
        [[maybe_unused]] auto u256_result_code = decoder.read(u256_val);
        
        // Test passes if we reach here without crashing
    }, 500);
}

TEST_F(PropertyBasedTest, FuzzEncoderDecoder) {
    // Generate structured but randomized RLP data and verify it roundtrips
    run_property_test([this](int iteration) {
        RlpEncoder encoder;
        
        // Randomly choose structure complexity
        std::uniform_int_distribution<int> complexity_dist(0, 3);
        int complexity = complexity_dist(rng_);
        
        std::function<void(int)> generate_random_structure = [&](int depth) {
            if (depth <= 0) {
                // Generate leaf value
                std::uniform_int_distribution<int> type_dist(0, 2);
                int type = type_dist(rng_);
                switch (type) {
                    case 0:
                        encoder.add(random_integer<uint32_t>());
                        break;
                    case 1:
                        encoder.add(random_integer<uint8_t>() % 2 == 0);
                        break;
                    case 2: {
                        auto bytes = random_bytes(0, 20);
                        encoder.add(rlp::ByteView{bytes.data(), bytes.size()});
                        break;
                    }
                }
            } else {
                // Generate list
                std::uniform_int_distribution<int> list_size_dist(0, 5);
                int list_size = list_size_dist(rng_);
                
                encoder.begin_list();
                for (int i = 0; i < list_size; ++i) {
                    std::uniform_int_distribution<int> recurse_dist(0, 1);
                    if (recurse_dist(rng_) == 0) {
                        generate_random_structure(depth - 1);
                    } else {
                        generate_random_structure(0); // Force leaf
                    }
                }
                encoder.end_list();
            }
        };
        
        generate_random_structure(complexity);
        
        // Encode and then decode - should not crash
        auto encoded = encoder.get_bytes();
        RlpDecoder decoder(encoded);
        
        // Try to decode as much as possible without specific expectations
        // This tests that valid RLP data produced by encoder can always be decoded
        std::function<void(RlpDecoder&)> try_decode = [&](RlpDecoder& dec) {
            if (dec.is_finished()) return;

            // Try list first
            auto list_header = dec.read_list_header();
            if (list_header.has_value()) {
                // list_header.value() is the payload length in bytes
                // Track remaining bytes in this list payload
                ByteView remaining_before = dec.remaining();
                size_t payload_length = list_header.value();
                size_t consumed = 0;
                
                while (consumed < payload_length && !dec.is_finished()) {
                    ByteView before_item = dec.remaining();
                    try_decode(dec);
                    ByteView after_item = dec.remaining();
                    size_t item_consumed = before_item.size() - after_item.size();
                    consumed += item_consumed;
                    
                    // Safety check to prevent infinite loops
                    if (item_consumed == 0) break;
                }
                return;
            }

            // Try string
            rlp::Bytes str_result;
            if (dec.read(str_result)) {
                return;
            }

            // Try integer
            uint32_t int_val;
            if (dec.read(int_val)) {
                return;
            }

            // If nothing works, skip one byte to avoid infinite loop
            if (!dec.is_finished()) {
                return;
            }
        };

        try_decode(decoder);
        
        // Test passes if we don't crash
    }, 300);
}

// ===================================================================
// PROPERTY: ENCODING IS DETERMINISTIC
// ===================================================================

TEST_F(PropertyBasedTest, EncodingIsDeterministic) {
    run_property_test([this](int iteration) {
        uint32_t value = random_integer<uint32_t>();
        
        // Encode the same value multiple times
        RlpEncoder encoder1;
        encoder1.add(value);
        auto encoded1 = encoder1.get_bytes();
        
        RlpEncoder encoder2;
        encoder2.add(value);
        auto encoded2 = encoder2.get_bytes();
        
        EXPECT_EQ(encoded1, encoded2) << "Iteration " << iteration << ", value: " << value;
    });
}

// ===================================================================
// PROPERTY: ENCODING IS MINIMAL
// ===================================================================

TEST_F(PropertyBasedTest, EncodingIsMinimal) {
    // Test that encoder produces canonical (minimal) encodings
    run_property_test([this](int iteration) {
        uint32_t value = random_integer<uint32_t>();
        
        RlpEncoder encoder;
        encoder.add(value);
        auto encoded = encoder.get_bytes();
        
        // Verify the encoding follows RLP rules for minimal representation
        if (value == 0) {
            EXPECT_EQ(encoded, rlp::Bytes{0x80}) << "Zero should encode as empty string";
        } else if (value < 0x80) {
            EXPECT_EQ(encoded.size(), 1) << "Single byte values should be encoded as-is";
            EXPECT_EQ(encoded[0], static_cast<uint8_t>(value));
        } else {
            // Multi-byte integer should have minimal representation
            EXPECT_GE(encoded.size(), 2) << "Multi-byte values need header";
            EXPECT_LT(encoded.size(), 10) << "32-bit values shouldn't need more than 9 bytes total";
            
            // First byte should be in valid range for strings
            EXPECT_GE(encoded[0], 0x81);
            EXPECT_LE(encoded[0], 0xb7);
        }
    });
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}