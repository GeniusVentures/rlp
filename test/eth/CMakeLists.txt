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

# ETH transaction (type 0/1/2) encode/decode tests
add_executable(eth_transactions_test
    eth_transactions_test.cpp
)
target_link_libraries(eth_transactions_test PRIVATE rlp GTest::gtest_main)

# EventFilter / EventWatcher tests
add_executable(event_filter_test
    event_filter_test.cpp
)
target_link_libraries(event_filter_test PRIVATE rlp GTest::gtest_main)

# ABI decoder tests
add_executable(abi_decoder_test
    abi_decoder_test.cpp
)
target_link_libraries(abi_decoder_test PRIVATE rlp GTest::gtest_main)

# EthWatchService tests
add_executable(eth_watch_service_test
    eth_watch_service_test.cpp
)
target_link_libraries(eth_watch_service_test PRIVATE rlp GTest::gtest_main)

# EthWatchCli tests
add_executable(eth_watch_cli_test
    eth_watch_cli_test.cpp
)
target_link_libraries(eth_watch_cli_test PRIVATE rlp GTest::gtest_main)

# ChainTracker tests
add_executable(chain_tracker_test
    chain_tracker_test.cpp
)
target_link_libraries(chain_tracker_test PRIVATE rlp GTest::gtest_main)

# GNUS.AI contract address tests
add_executable(gnus_contracts_test
    gnus_contracts_test.cpp
)
target_link_libraries(gnus_contracts_test PRIVATE rlp GTest::gtest_main)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(eth_messages_test)
gtest_discover_tests(eth_objects_test)
gtest_discover_tests(eth_watch_integration_test)
gtest_discover_tests(eth_transactions_test)
gtest_discover_tests(event_filter_test)
gtest_discover_tests(abi_decoder_test)
gtest_discover_tests(eth_watch_service_test)
gtest_discover_tests(eth_watch_cli_test)
gtest_discover_tests(chain_tracker_test)
gtest_discover_tests(gnus_contracts_test)

