# Define the test executables
addtest(rlp_encoder_tests
        rlp_encoder_tests.cpp
)

addtest(rlp_decoder_tests
        rlp_decoder_tests.cpp
)

# Link the library under test
target_link_libraries(rlp_encoder_tests PRIVATE rlp)
target_link_libraries(rlp_decoder_tests PRIVATE rlp)