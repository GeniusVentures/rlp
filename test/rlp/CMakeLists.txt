# RLP Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# RLP encoder tests
addtest(rlp_encoder_tests
    rlp_encoder_tests.cpp
)
target_link_libraries(rlp_encoder_tests
    rlp
)

# RLP decoder tests
addtest(rlp_decoder_tests
    rlp_decoder_tests.cpp
)
target_link_libraries(rlp_decoder_tests
    rlp
)

# RLP endian tests
addtest(rlp_endian_tests
    rlp_endian_tests.cpp
)
target_link_libraries(rlp_endian_tests
    rlp
)

# RLP edge cases tests
addtest(rlp_edge_cases
    rlp_edge_cases.cpp
)
target_link_libraries(rlp_edge_cases
    rlp
)

# RLP benchmark tests
addtest(rlp_benchmark_tests
    rlp_benchmark_tests.cpp
)
target_link_libraries(rlp_benchmark_tests
    rlp
)

# RLP property tests
addtest(rlp_property_tests
    rlp_property_tests.cpp
)
target_link_libraries(rlp_property_tests
    rlp
)

# RLP comprehensive tests
addtest(rlp_comprehensive_tests
    rlp_comprehensive_tests.cpp
)
target_link_libraries(rlp_comprehensive_tests
    rlp
)

# RLP ethereum tests
addtest(rlp_ethereum_tests
    rlp_ethereum_tests.cpp
)
target_link_libraries(rlp_ethereum_tests
    rlp
)

# RLP random tests
addtest(rlp_random_tests
    rlp_random_tests.cpp
)
target_link_libraries(rlp_random_tests
    rlp
)

# RLP type safety tests
addtest(rlp_type_safety_tests
    rlp_type_safety_tests.cpp
)
target_link_libraries(rlp_type_safety_tests
    rlp
)

# RLP enhanced API tests
addtest(rlp_enhanced_api_tests
    rlp_enhanced_api_tests.cpp
)
target_link_libraries(rlp_enhanced_api_tests
    rlp
)

# RLP streaming decoder tests
addtest(rlp_streaming_decoder_tests
    rlp_streaming_decoder_tests.cpp
)
target_link_libraries(rlp_streaming_decoder_tests
    rlp
)

# RLP streaming simple API demo
addtest(rlp_streaming_simple_api_demo
    rlp_streaming_simple_api_demo.cpp
)
target_link_libraries(rlp_streaming_simple_api_demo
    rlp
)

# RLP ethereum real world examples
addtest(rlp_ethereum_real_world_examples
    rlp_ethereum_real_world_examples.cpp
)
target_link_libraries(rlp_ethereum_real_world_examples
    rlp
)

# RLP profiling tests
addtest(rlp_profiling_tests
    rlp_profiling_tests.cpp
)
target_link_libraries(rlp_profiling_tests
    rlp
)

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
