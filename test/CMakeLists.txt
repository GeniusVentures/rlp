# Define the test executables
addtest(encoder_tests
        encoder_tests.cpp
)

addtest(decoder_tests
        decoder_tests.cpp
)

# Link the library under test
target_link_libraries(encoder_tests PRIVATE rlp)
target_link_libraries(decoder_tests PRIVATE rlp)