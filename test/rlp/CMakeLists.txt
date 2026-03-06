# RLP Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# RLP encoder tests
add_executable(rlp_encoder_tests
    rlp_encoder_tests.cpp
)
target_link_libraries(rlp_encoder_tests PRIVATE rlp GTest::gtest_main)

# RLP decoder tests
add_executable(rlp_decoder_tests
    rlp_decoder_tests.cpp
)
target_link_libraries(rlp_decoder_tests PRIVATE rlp GTest::gtest_main)

# RLP endian tests
add_executable(rlp_endian_tests
    rlp_endian_tests.cpp
)
target_link_libraries(rlp_endian_tests PRIVATE rlp GTest::gtest_main)

# RLP edge cases tests
add_executable(rlp_edge_cases
    rlp_edge_cases.cpp
)
target_link_libraries(rlp_edge_cases PRIVATE rlp GTest::gtest_main)

# RLP benchmark tests
add_executable(rlp_benchmark_tests
    rlp_benchmark_tests.cpp
)
target_link_libraries(rlp_benchmark_tests PRIVATE rlp GTest::gtest_main)

# RLP property tests
add_executable(rlp_property_tests
    rlp_property_tests.cpp
)
target_link_libraries(rlp_property_tests PRIVATE rlp GTest::gtest_main)

# RLP comprehensive tests
add_executable(rlp_comprehensive_tests
    rlp_comprehensive_tests.cpp
)
target_link_libraries(rlp_comprehensive_tests PRIVATE rlp GTest::gtest_main)

# RLP ethereum tests
add_executable(rlp_ethereum_tests
    rlp_ethereum_tests.cpp
)
target_link_libraries(rlp_ethereum_tests PRIVATE rlp GTest::gtest_main)

# RLP random tests
add_executable(rlp_random_tests
    rlp_random_tests.cpp
)
target_link_libraries(rlp_random_tests PRIVATE rlp GTest::gtest_main)

# RLP type safety tests
add_executable(rlp_type_safety_tests
    rlp_type_safety_tests.cpp
)
target_link_libraries(rlp_type_safety_tests PRIVATE rlp GTest::gtest_main)

# RLP enhanced API tests
add_executable(rlp_enhanced_api_tests
    rlp_enhanced_api_tests.cpp
)
target_link_libraries(rlp_enhanced_api_tests PRIVATE rlp GTest::gtest_main)

# RLP streaming decoder tests
add_executable(rlp_streaming_decoder_tests
    rlp_streaming_decoder_tests.cpp
)
target_link_libraries(rlp_streaming_decoder_tests PRIVATE rlp GTest::gtest_main)

# RLP streaming simple API demo
add_executable(rlp_streaming_simple_api_demo
    rlp_streaming_simple_api_demo.cpp
)
target_link_libraries(rlp_streaming_simple_api_demo PRIVATE rlp GTest::gtest_main)

# RLP ethereum real world examples
add_executable(rlp_ethereum_real_world_examples
    rlp_ethereum_real_world_examples.cpp
)
target_link_libraries(rlp_ethereum_real_world_examples PRIVATE rlp GTest::gtest_main)

# RLP profiling tests
add_executable(rlp_profiling_tests
    rlp_profiling_tests.cpp
)
target_link_libraries(rlp_profiling_tests PRIVATE rlp GTest::gtest_main)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(rlp_encoder_tests)
gtest_discover_tests(rlp_decoder_tests)
gtest_discover_tests(rlp_endian_tests)
gtest_discover_tests(rlp_edge_cases)
gtest_discover_tests(rlp_benchmark_tests)
gtest_discover_tests(rlp_property_tests)
gtest_discover_tests(rlp_comprehensive_tests)
gtest_discover_tests(rlp_ethereum_tests)
gtest_discover_tests(rlp_random_tests)
gtest_discover_tests(rlp_type_safety_tests)
gtest_discover_tests(rlp_enhanced_api_tests)
gtest_discover_tests(rlp_streaming_decoder_tests)
gtest_discover_tests(rlp_streaming_simple_api_demo)
gtest_discover_tests(rlp_ethereum_real_world_examples)
gtest_discover_tests(rlp_profiling_tests)

# Suppress nodiscard warnings in test code for cleaner output
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(RLP_TEST_TARGETS
        rlp_encoder_tests
        rlp_decoder_tests
        rlp_endian_tests
        rlp_edge_cases
        rlp_benchmark_tests
        rlp_property_tests
        rlp_comprehensive_tests
        rlp_ethereum_tests
        rlp_random_tests
        rlp_type_safety_tests
        rlp_enhanced_api_tests
        rlp_streaming_decoder_tests
        rlp_streaming_simple_api_demo
        rlp_ethereum_real_world_examples
        rlp_profiling_tests
    )
    foreach(target ${RLP_TEST_TARGETS})
        target_compile_options(${target} PRIVATE -Wno-unused-result)
    endforeach()
endif()
