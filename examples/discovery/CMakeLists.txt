cmake_minimum_required(VERSION 3.15)

add_executable(test_discovery
    test_discovery.cpp
)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_discovery
    PRIVATE
    rlpx
    rlp
    discv4
    logger
    Boost::boost
    Boost::json
)

# Disable ASAN: live-network integration test uses Boost.Asio coroutines which
# trigger false positives on macOS ARM64 (same reason as eth_watch).
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(test_discovery PRIVATE -fno-sanitize=address)
endif()
