# Test CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Add test subdirectories
add_subdirectory(rlp)
add_subdirectory(eth)
if(ENABLE_FUZZING)
    add_subdirectory(fuzz)
endif()
add_subdirectory(rlpx)
add_subdirectory(discv4)
add_subdirectory(discv5)



