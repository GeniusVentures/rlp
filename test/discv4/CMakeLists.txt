# Discovery v4 Protocol Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Discovery tests
add_executable(discv4_discovery_tests
    ${CMAKE_CURRENT_LIST_DIR}/discovery_test.cpp
)

target_link_libraries(discv4_discovery_tests PRIVATE
    discv4
    rlpx
    rlp
    GTest::gtest_main
)

target_include_directories(discv4_discovery_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Protocol tests
add_executable(discv4_protocol_test
    ${CMAKE_CURRENT_LIST_DIR}/discv4_protocol_test.cpp
)

target_link_libraries(discv4_protocol_test PRIVATE
    discv4
    rlpx
    rlp
    GTest::gtest_main
    libsecp256k1::secp256k1
)

target_include_directories(discv4_protocol_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# DialHistory tests (header-only, no extra libs needed)
add_executable(discv4_dial_history_test
    ${CMAKE_CURRENT_LIST_DIR}/dial_history_test.cpp
)

target_link_libraries(discv4_dial_history_test PRIVATE
    GTest::gtest_main
)

target_include_directories(discv4_dial_history_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# discv4_client lifetime tests
add_executable(discv4_client_test
    ${CMAKE_CURRENT_LIST_DIR}/discv4_client_test.cpp
)

target_link_libraries(discv4_client_test PRIVATE
    discv4
    rlpx
    rlp
    GTest::gtest_main
)

target_include_directories(discv4_client_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add tests to CTest
include(GoogleTest)
if(NOT CMAKE_CROSSCOMPILING)
    gtest_discover_tests(discv4_discovery_tests)
    gtest_discover_tests(discv4_protocol_test)
    gtest_discover_tests(discv4_dial_history_test)
    gtest_discover_tests(discv4_client_test)
endif()
