# ETH Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# ETH messages tests
add_executable(eth_messages_test
    eth_messages_test.cpp
)
target_link_libraries(eth_messages_test PRIVATE rlp GTest::gtest_main)

# ETH objects tests
add_executable(eth_objects_test
    eth_objects_test.cpp
)
target_link_libraries(eth_objects_test PRIVATE rlp GTest::gtest_main)

# ETH watch integration tests
add_executable(eth_watch_integration_test
    eth_watch_integration_test.cpp
)
target_link_libraries(eth_watch_integration_test PRIVATE rlp GTest::gtest_main)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(eth_messages_test)
gtest_discover_tests(eth_objects_test)
gtest_discover_tests(eth_watch_integration_test)

