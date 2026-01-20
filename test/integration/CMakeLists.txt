# Integration tests CMakeLists.txt
# Network connectivity tests for P2P integration

cmake_minimum_required(VERSION 3.20)
project(rlp_integration_tests CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force static runtime to match Boost/GTest libraries
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Configure Boost
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)

# Find dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(GTest REQUIRED)

# Network port test (Google Test based)
add_executable(network_port_test
    network_port_test.cpp
)

target_link_libraries(network_port_test
    PRIVATE
    GTest::gtest
    GTest::gtest_main
    Boost::system
)

target_include_directories(network_port_test
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Network diagnostics utility (standalone executable)
add_executable(network_diagnostics
    network_diagnostics.cpp
)

target_link_libraries(network_diagnostics
    PRIVATE
    Boost::system
)

target_include_directories(network_diagnostics
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Install targets
install(TARGETS network_port_test network_diagnostics
    RUNTIME DESTINATION bin
)
