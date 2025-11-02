# Define the test executables
addtest(rlp_encoder_tests
        rlp/rlp_encoder_tests.cpp
)

addtest(rlp_decoder_tests
        rlp/rlp_decoder_tests.cpp
)

addtest(rlp_edge_cases
        rlp/rlp_edge_cases.cpp
)

addtest(rlp_comprehensive_tests
        rlp/rlp_comprehensive_tests.cpp
)

addtest(rlp_property_tests
        rlp/rlp_property_tests.cpp
)

addtest(rlp_benchmark_tests
        rlp/rlp_benchmark_tests.cpp
)

addtest(discovery_test
        rlp/discovery_test.cpp
)

addtest(rlp_endian_tests
        rlp/rlp_endian_tests.cpp
)

addtest(rlp_ethereum_tests
        rlp/rlp_ethereum_tests.cpp
)

addtest(rlp_random_tests
        rlp/rlp_random_tests.cpp
)

addtest(rlp_type_safety_tests
        rlp/rlp_type_safety_tests.cpp
)

addtest(rlp_streaming_decoder_tests
        rlp/rlp_streaming_decoder_tests.cpp
)

addtest(rlp_streaming_simple_api_demo
        rlp/rlp_streaming_simple_api_demo.cpp
)

target_link_libraries(discovery_test PRIVATE rlp)
target_link_libraries(rlp_endian_tests PRIVATE rlp)
target_link_libraries(rlp_ethereum_tests PRIVATE rlp)
target_link_libraries(rlp_encoder_tests PRIVATE rlp)
target_link_libraries(rlp_decoder_tests PRIVATE rlp)
target_link_libraries(rlp_edge_cases PRIVATE rlp)
target_link_libraries(rlp_comprehensive_tests PRIVATE rlp)
target_link_libraries(rlp_property_tests PRIVATE rlp)
target_link_libraries(rlp_benchmark_tests PRIVATE rlp)
target_link_libraries(rlp_random_tests PRIVATE rlp)
target_link_libraries(rlp_type_safety_tests PRIVATE rlp)
target_link_libraries(rlp_streaming_decoder_tests PRIVATE rlp)
target_link_libraries(rlp_streaming_simple_api_demo PRIVATE rlp)

# Add RLPx tests subdirectory
add_subdirectory(rlpx)