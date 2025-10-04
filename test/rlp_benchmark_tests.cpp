#include <gtest/gtest.h>
#include <rlp_encoder.hpp>
#include <rlp_decoder.hpp>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <iomanip>

using namespace rlp;

// ===================================================================
// BENCHMARK UTILITIES
// ===================================================================

// Prevent compiler from optimizing away computations
// Similar to Google Benchmark's DoNotOptimize but for use with Google Test
namespace benchmark_util {

#if defined(_MSC_VER)
    #pragma optimize("", off)
    template <typename T>
    inline void DoNotOptimize(T const& value) {
        // Force the compiler to believe the value is used
        const_cast<char const volatile&>(reinterpret_cast<char const volatile&>(value));
    }
    #pragma optimize("", on)
#elif defined(__GNUC__) || defined(__clang__)
    template <typename T>
    inline void DoNotOptimize(T const& value) {
        asm volatile("" : : "r,m"(value) : "memory");
    }
    
    template <typename T>
    inline void DoNotOptimize(T& value) {
#if defined(__clang__)
        asm volatile("" : "+r,m"(value) : : "memory");
#else
        asm volatile("" : "+m,r"(value) : : "memory");
#endif
    }
#else
    // Fallback for unknown compilers - use a pointer with side effects
    template <typename T>
    inline void DoNotOptimize(T const& value) {
        static_cast<void>(std::as_const(value));
        asm volatile("" : : : "memory");
    }
#endif

} // namespace benchmark_util

// ===================================================================
// BENCHMARK FRAMEWORK
// ==================================================================="/gtest.h>
#include <rlp_encoder.hpp>
#include <rlp_decoder.hpp>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <iomanip>

using namespace rlp;

// ===================================================================
// BENCHMARK FRAMEWORK
// ===================================================================

class BenchmarkTimer {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    void stop() {
        end_time_ = std::chrono::high_resolution_clock::now();
    }
    double elapsed_ms() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time_ - start_time_);
        return duration.count() / 1000.0;
    }
    double elapsed_ns() const {
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time_ - start_time_);
        return static_cast<double>(duration.count());
    }
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
};

class BenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Fixed seed for reproducible benchmarks
        rng_.seed(42);
    }
    
    std::mt19937 rng_;
    
    // Performance thresholds (adjust based on target hardware)
    static constexpr double MAX_ENCODE_TIME_PER_BYTE_NS = 50.0;  // 50ns per byte
    static constexpr double MAX_DECODE_TIME_PER_BYTE_NS = 100.0; // 100ns per byte
    static constexpr double MAX_UINT32_ENCODE_TIME_NS = 1000.0;  // 1μs per uint32
    static constexpr double MAX_UINT32_DECODE_TIME_NS = 2000.0;  // 2μs per uint32
    
    rlp::Bytes random_bytes(size_t size) {
        std::uniform_int_distribution<int> byte_dist(0, 255);
        rlp::Bytes result;
        result.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            result.push_back(static_cast<uint8_t>(byte_dist(rng_)));
        }
        return result;
    }
    
    template<typename Func>
    double benchmark_operation(Func&& operation, int iterations = 1000) {
        // Warm up
        for (int i = 0; i < 10; ++i) {
            operation();
        }
        
        BenchmarkTimer timer;
        timer.start();
        for (int i = 0; i < iterations; ++i) {
            operation();
        }
        timer.stop();
        
        return timer.elapsed_ns() / iterations;
    }
    
    void report_performance(const std::string& test_name, double avg_time_ns, 
                          double threshold_ns, const std::string& unit = "operation") {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "[BENCHMARK] " << test_name << ": " 
                  << avg_time_ns << " ns/" << unit 
                  << " (threshold: " << threshold_ns << " ns/" << unit << ")" << std::endl;
        
        EXPECT_LT(avg_time_ns, threshold_ns) 
            << test_name << " performance regression detected: "
            << avg_time_ns << " ns/" << unit << " > " << threshold_ns << " ns/" << unit;
    }
};

// ===================================================================
// ENCODING BENCHMARKS
// ===================================================================

TEST_F(BenchmarkTest, BenchmarkUint8Encoding) {
    std::vector<uint8_t> test_values;
    for (int i = 0; i < 256; ++i) {
        test_values.push_back(static_cast<uint8_t>(i));
    }
    
    size_t value_index = 0;
    auto avg_time = benchmark_operation([&]() {
        RlpEncoder encoder;
        encoder.add(test_values[value_index % test_values.size()]);
        auto result = encoder.get_bytes();
        value_index++;
        // Prevent optimization
        benchmark_util::DoNotOptimize(result.size());
    });
    
    report_performance("uint8 encoding", avg_time, MAX_UINT32_ENCODE_TIME_NS);
}

TEST_F(BenchmarkTest, BenchmarkUint32Encoding) {
    std::vector<uint32_t> test_values;
    std::uniform_int_distribution<uint32_t> dist;
    for (int i = 0; i < 1000; ++i) {
        test_values.push_back(dist(rng_));
    }
    
    size_t value_index = 0;
    auto avg_time = benchmark_operation([&]() {
        RlpEncoder encoder;
        encoder.add(test_values[value_index % test_values.size()]);
        auto result = encoder.get_bytes();
        value_index++;
        // Prevent optimization
        benchmark_util::DoNotOptimize(result.size());
    });
    
    report_performance("uint32 encoding", avg_time, MAX_UINT32_ENCODE_TIME_NS);
}

TEST_F(BenchmarkTest, BenchmarkUint256Encoding) {
    std::vector<intx::uint256> test_values;
    std::uniform_int_distribution<uint64_t> uint64_dist;
    for (int i = 0; i < 100; ++i) {
        intx::uint256 value = 0;
        for (int j = 0; j < 4; ++j) {
            uint64_t part = uint64_dist(rng_);
            value = (value << 64) | part;
        }
        test_values.push_back(value);
    }
    
    size_t value_index = 0;
    auto avg_time = benchmark_operation([&]() {
        RlpEncoder encoder;
        encoder.add(test_values[value_index % test_values.size()]);
        auto result = encoder.get_bytes();
        value_index++;
        // Prevent optimization
        benchmark_util::DoNotOptimize(result.size());
    }, 100); // Fewer iterations for uint256
    
    // uint256 encoding is more complex, allow 10x more time
    report_performance("uint256 encoding", avg_time, MAX_UINT32_ENCODE_TIME_NS * 10);
}

TEST_F(BenchmarkTest, BenchmarkByteArrayEncoding) {
    std::vector<rlp::Bytes> test_data;
    std::vector<size_t> sizes = {0, 1, 10, 55, 56, 100, 255, 256, 1000};
    
    for (size_t size : sizes) {
        test_data.push_back(random_bytes(size));
    }
    
    size_t data_index = 0;
    auto avg_time = benchmark_operation([&]() {
        const auto& data = test_data[data_index % test_data.size()];
        RlpEncoder encoder;
        encoder.add(rlp::ByteView{data.data(), data.size()});
        auto result = encoder.get_bytes();
        data_index++;
        // Prevent optimization
        benchmark_util::DoNotOptimize(result.size());
    });
    
    report_performance("byte array encoding", avg_time, MAX_UINT32_ENCODE_TIME_NS * 2);
}

TEST_F(BenchmarkTest, BenchmarkLargeByteArrayEncoding) {
    // Test encoding performance scales linearly with data size
    std::vector<size_t> sizes = {1024, 4096, 16384, 65536}; // 1KB to 64KB
    
    for (size_t size : sizes) {
        auto data = random_bytes(size);
        
        auto avg_time = benchmark_operation([&]() {
            RlpEncoder encoder;
            encoder.add(rlp::ByteView{data.data(), data.size()});
            auto result = encoder.get_bytes();
            // Prevent optimization
            benchmark_util::DoNotOptimize(result.size());
        }, 100); // Fewer iterations for large data
        
        double time_per_byte = avg_time / size;
        report_performance("large byte array (" + std::to_string(size) + " bytes) encoding", 
                         time_per_byte, MAX_ENCODE_TIME_PER_BYTE_NS, "byte");
    }
}

TEST_F(BenchmarkTest, BenchmarkListEncoding) {
    // Create lists of various sizes with mixed content
    std::vector<int> list_sizes = {1, 10, 100, 1000};
    
    for (int list_size : list_sizes) {
        auto avg_time = benchmark_operation([&]() {
            RlpEncoder encoder;
            encoder.begin_list();
            
            for (int i = 0; i < list_size; ++i) {
                encoder.add(static_cast<uint32_t>(i));
            }
            
            encoder.end_list();
            auto result = encoder.get_bytes();
            // Prevent optimization
            benchmark_util::DoNotOptimize(result.size());
        }, std::max(1, 1000 / list_size)); // Fewer iterations for larger lists
        
        double time_per_element = avg_time / list_size;
        report_performance("list (" + std::to_string(list_size) + " elements) encoding",
                         time_per_element, MAX_UINT32_ENCODE_TIME_NS * 2, "element");
    }
}

// ===================================================================
// DECODING BENCHMARKS
// ===================================================================

TEST_F(BenchmarkTest, BenchmarkUint32Decoding) {
    // Pre-encode test data
    std::vector<rlp::Bytes> encoded_values;
    std::uniform_int_distribution<uint32_t> dist;
    
    for (int i = 0; i < 1000; ++i) {
        RlpEncoder encoder;
        encoder.add(dist(rng_));
        encoded_values.push_back(encoder.get_bytes());
    }
    
    size_t value_index = 0;
    auto avg_time = benchmark_operation([&]() {
        const auto& encoded = encoded_values[value_index % encoded_values.size()];
        RlpDecoder decoder(encoded);
        uint32_t value;
    auto result = decoder.read(value);
    value_index++;
    // Prevent optimization
    benchmark_util::DoNotOptimize(result.has_value());
    benchmark_util::DoNotOptimize(value);
    });
    
    report_performance("uint32 decoding", avg_time, MAX_UINT32_DECODE_TIME_NS);
}

TEST_F(BenchmarkTest, BenchmarkByteArrayDecoding) {
    // Pre-encode test data of various sizes
    std::vector<rlp::Bytes> encoded_data;
    std::vector<size_t> sizes = {0, 1, 10, 55, 56, 100, 255, 256, 1000};
    
    for (size_t size : sizes) {
        auto data = random_bytes(size);
        RlpEncoder encoder;
        encoder.add(rlp::ByteView{data.data(), data.size()});
        encoded_data.push_back(encoder.get_bytes());
    }
    
    size_t data_index = 0;
    auto avg_time = benchmark_operation([&]() {
        const auto& encoded = encoded_data[data_index % encoded_data.size()];
        RlpDecoder decoder(encoded);
    rlp::Bytes result;
    auto decode_result = decoder.read(result);
    data_index++;
    // Prevent optimization
    benchmark_util::DoNotOptimize(decode_result.has_value());
    });

    report_performance("byte array decoding", avg_time, MAX_UINT32_DECODE_TIME_NS * 2);
}

TEST_F(BenchmarkTest, BenchmarkLargeByteArrayDecoding) {
    // Test decoding performance scales linearly with data size
    std::vector<size_t> sizes = {1024, 4096, 16384, 65536}; // 1KB to 64KB
    
    for (size_t size : sizes) {
        auto data = random_bytes(size);
        RlpEncoder encoder;
        encoder.add(rlp::ByteView{data.data(), data.size()});
        auto encoded = encoder.get_bytes();
        
        auto avg_time = benchmark_operation([&]() {
            RlpDecoder decoder(encoded);
            rlp::Bytes result;
            auto decode_result = decoder.read(result);
            // Prevent optimization
            benchmark_util::DoNotOptimize(decode_result.has_value());
        }, 100); // Fewer iterations for large data
        
        double time_per_byte = avg_time / size;
        report_performance("large byte array (" + std::to_string(size) + " bytes) decoding",
                         time_per_byte, MAX_DECODE_TIME_PER_BYTE_NS, "byte");
    }
}

TEST_F(BenchmarkTest, BenchmarkListDecoding) {
    // Pre-encode lists of various sizes
    std::vector<int> list_sizes = {1, 10, 100, 1000};
    std::vector<rlp::Bytes> encoded_lists;
    
    for (int list_size : list_sizes) {
        RlpEncoder encoder;
        encoder.begin_list();
        for (int i = 0; i < list_size; ++i) {
            encoder.add(static_cast<uint32_t>(i));
        }
        encoder.end_list();
        encoded_lists.push_back(encoder.get_bytes());
    }
    
    for (size_t i = 0; i < list_sizes.size(); ++i) {
        int list_size = list_sizes[i];
        const auto& encoded = encoded_lists[i];
        
        auto avg_time = benchmark_operation([&]() {
            RlpDecoder decoder(encoded);
            auto list_header = decoder.read_list_header();
            if (list_header.has_value()) {
                for (size_t j = 0; j < static_cast<size_t>(list_size); ++j) {
                    uint32_t value;
                    auto decode_result = decoder.read(value);
                }
            }
            // Prevent optimization
            benchmark_util::DoNotOptimize(list_header.has_value());
        }, std::max(1, 1000 / list_size)); // Fewer iterations for larger lists

        double time_per_element = avg_time / list_size;
        report_performance("list (" + std::to_string(list_size) + " elements) decoding",
                         time_per_element, MAX_UINT32_DECODE_TIME_NS * 3, "element");
    }
}

// ===================================================================
// ROUND-TRIP BENCHMARKS
// ===================================================================

TEST_F(BenchmarkTest, BenchmarkRoundTrip) {
    std::vector<uint32_t> test_values;
    std::uniform_int_distribution<uint32_t> dist;
    for (int i = 0; i < 1000; ++i) {
        test_values.push_back(dist(rng_));
    }
    
    size_t value_index = 0;
    auto avg_time = benchmark_operation([&]() {
        uint32_t original = test_values[value_index % test_values.size()];
        
        // Encode
        RlpEncoder encoder;
        encoder.add(original);
        auto encoded = encoder.get_bytes();
        
        // Decode
        RlpDecoder decoder(encoded);
    uint32_t decoded;
    auto decode_result = decoder.read(decoded);

    value_index++;
    // Prevent optimization
    benchmark_util::DoNotOptimize(decode_result.has_value());
    benchmark_util::DoNotOptimize(decoded == original);
    });
    
    report_performance("uint32 round-trip", avg_time, (MAX_UINT32_ENCODE_TIME_NS + MAX_UINT32_DECODE_TIME_NS));
}

// ===================================================================
// MEMORY ALLOCATION BENCHMARKS
// ===================================================================

TEST_F(BenchmarkTest, BenchmarkMemoryEfficiency) {
    // Test that encoding doesn't cause excessive memory allocations
    std::vector<size_t> data_sizes = {100, 1000, 10000};
    
    for (size_t size : data_sizes) {
        auto data = random_bytes(size);
        
        auto avg_time = benchmark_operation([&]() {
            RlpEncoder encoder;
            encoder.add(rlp::ByteView{data.data(), data.size()});
            auto result = encoder.get_bytes();
            
            // Verify reasonable memory overhead
            size_t expected_min_size = size + 1; // At least original size + header
            size_t expected_max_size = size + 10; // Reasonable overhead
            
            EXPECT_GE(result.size(), expected_min_size);
            EXPECT_LE(result.size(), expected_max_size);
            
            // Prevent optimization
            benchmark_util::DoNotOptimize(result.size());
        }, 100);
        
        // Memory efficiency test - encoding shouldn't take too long
        double time_per_byte = avg_time / size;
        report_performance("memory efficiency (" + std::to_string(size) + " bytes)",
                         time_per_byte, MAX_ENCODE_TIME_PER_BYTE_NS * 2, "byte");
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "\n===================================================================\n";
    std::cout << "RLP PERFORMANCE BENCHMARK SUITE\n";
    std::cout << "===================================================================\n\n";
    
    auto result = RUN_ALL_TESTS();
    
    std::cout << "\n===================================================================\n";
    std::cout << "BENCHMARK COMPLETE\n";
    std::cout << "===================================================================\n\n";
    
    return result;
}