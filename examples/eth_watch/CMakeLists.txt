# eth_watch example CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

add_executable(eth_watch
    eth_watch.cpp
)

# Include project headers
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(eth_watch
    PRIVATE
    rlpx
    rlp
    Boost::boost
    Boost::json
)

