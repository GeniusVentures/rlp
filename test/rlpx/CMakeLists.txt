# RLPx Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Crypto tests
add_executable(rlpx_crypto_tests
    ${CMAKE_CURRENT_LIST_DIR}/crypto_test.cpp
)

target_link_libraries(rlpx_crypto_tests PRIVATE
    rlpx
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(rlpx_crypto_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Frame cipher tests
add_executable(rlpx_frame_cipher_tests
    ${CMAKE_CURRENT_LIST_DIR}/frame_cipher_test.cpp
)

target_link_libraries(rlpx_frame_cipher_tests PRIVATE
    rlpx
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(rlpx_frame_cipher_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Protocol messages tests
add_executable(rlpx_protocol_messages_tests
    ${CMAKE_CURRENT_LIST_DIR}/protocol_messages_test.cpp
)

target_link_libraries(rlpx_protocol_messages_tests PRIVATE
    rlpx
    rlp
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(rlpx_protocol_messages_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(rlpx_crypto_tests)
gtest_discover_tests(rlpx_frame_cipher_tests)
gtest_discover_tests(rlpx_protocol_messages_tests)
