# Define the test executables
addtest(rlp_encoder_tests
        rlp_encoder_tests.cpp
)

addtest(rlp_decoder_tests
        rlp_decoder_tests.cpp
)

addtest(rlp_edge_cases
        rlp_edge_cases.cpp
)

addtest(rlp_comprehensive_tests
        rlp_comprehensive_tests.cpp
)

addtest(rlp_property_tests
        rlp_property_tests.cpp
)

addtest(rlp_benchmark_tests
        rlp_benchmark_tests.cpp
)

addtest(discovery_test
        discovery_test.cpp
)

addtest(rlp_endian_tests
        rlp_endian_tests.cpp
)

target_link_libraries(discovery_test PRIVATE rlp)
target_link_libraries(rlp_endian_tests PRIVATE rlp)
target_link_libraries(rlp_encoder_tests PRIVATE rlp)
target_link_libraries(rlp_decoder_tests PRIVATE rlp)
target_link_libraries(rlp_edge_cases PRIVATE rlp)
target_link_libraries(rlp_comprehensive_tests PRIVATE rlp)
target_link_libraries(rlp_property_tests PRIVATE rlp)
target_link_libraries(rlp_benchmark_tests PRIVATE rlp)