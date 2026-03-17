# Test CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Discover GoogleTest cases at ctest time instead of build time.
# This avoids build failures on runners where executing test binaries during
# compilation has environment/runtime differences.
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

# Add test subdirectories
add_subdirectory(rlp)
add_subdirectory(eth)
if(ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
add_subdirectory(rlpx)
add_subdirectory(discv4)



