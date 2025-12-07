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

# Add fuzz testing support (optional)
if(ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()

# Add RLPx tests subdirectory
add_subdirectory(rlpx)

# Add integration tests (network connectivity tests)
if(ENABLE_INTEGRATION_TESTS)
    message(STATUS "Integration tests enabled")

    # Network port connectivity test
    addtest(network_port_test
            integration/network_port_test.cpp
    )
    target_link_libraries(network_port_test PRIVATE rlp)

    # Network diagnostics utility (executable, not a test)
    add_executable(network_diagnostics
        integration/network_diagnostics.cpp
    )
    target_link_libraries(network_diagnostics PRIVATE rlp)

    # Copy to binary directory for easy access
    install(TARGETS network_diagnostics DESTINATION bin OPTIONAL)
endif()