# RLPx Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Crypto tests
add_executable(rlpx_crypto_tests
    ${CMAKE_CURRENT_LIST_DIR}/crypto_test.cpp
)

target_link_libraries(rlpx_crypto_tests PRIVATE
    rlpx
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
    GTest::gtest_main
)

target_include_directories(rlpx_protocol_messages_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# RLPx Session integration tests
add_executable(rlpx_session_tests
    ${CMAKE_CURRENT_LIST_DIR}/rlpx_session_test.cpp
)

target_link_libraries(rlpx_session_tests PRIVATE
    rlpx
    rlp
    GTest::gtest_main
)

target_include_directories(rlpx_session_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# RLPx state tests
add_executable(rlpx_state_tests
    ${CMAKE_CURRENT_LIST_DIR}/rlpx_state_test.cpp
)

target_link_libraries(rlpx_state_tests PRIVATE
    rlpx
    GTest::gtest_main
)

target_include_directories(rlpx_state_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# RLPx message routing tests
add_executable(rlpx_message_routing_tests
    ${CMAKE_CURRENT_LIST_DIR}/message_routing_test.cpp
)

target_link_libraries(rlpx_message_routing_tests PRIVATE
    rlpx
    rlp
    GTest::gtest_main
)

target_include_directories(rlpx_message_routing_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# RLPx socket lifecycle tests
add_executable(rlpx_socket_lifecycle_tests
    ${CMAKE_CURRENT_LIST_DIR}/socket_lifecycle_test.cpp
)

target_link_libraries(rlpx_socket_lifecycle_tests PRIVATE
    rlpx
    rlp
    GTest::gtest_main
)

target_include_directories(rlpx_socket_lifecycle_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(rlpx_crypto_tests)
gtest_discover_tests(rlpx_frame_cipher_tests)
gtest_discover_tests(rlpx_protocol_messages_tests)
gtest_discover_tests(rlpx_session_tests)
gtest_discover_tests(rlpx_state_tests)
gtest_discover_tests(rlpx_message_routing_tests)
gtest_discover_tests(rlpx_socket_lifecycle_tests)
