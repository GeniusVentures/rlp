# Fuzz Testing Support
# Requires Clang with libFuzzer support or AFL
#
# Usage:
#   cmake -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++ ..
#   cmake --build . --target fuzz_rlp_decoder
#   ./fuzz_rlp_decoder -max_total_time=60

option(ENABLE_FUZZING "Build fuzz test targets" OFF)

if(ENABLE_FUZZING)
    # Check if we're using Clang
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(WARNING "Fuzzing is best supported with Clang. Current compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
    
    # Fuzzing flags
    set(FUZZING_FLAGS
        -fsanitize=fuzzer,address
        -fno-omit-frame-pointer
        -g
    )
    
    # Function to create a fuzz target
    function(add_fuzz_target target_name source_file)
        add_executable(${target_name} ${source_file})
        
        target_link_libraries(${target_name} PRIVATE rlp)
        
        # Add fuzzing flags
        target_compile_options(${target_name} PRIVATE ${FUZZING_FLAGS})
        target_link_options(${target_name} PRIVATE ${FUZZING_FLAGS})
        
        # Don't add to CTest - these are run manually
        set_target_properties(${target_name} PROPERTIES
            EXCLUDE_FROM_ALL OFF
            FOLDER "Fuzz Tests"
        )
        
        message(STATUS "Added fuzz target: ${target_name}")
    endfunction()
    
    # Add fuzz targets
    add_fuzz_target(fuzz_rlp_decoder ${CMAKE_CURRENT_LIST_DIR}/fuzz_rlp_decoder.cpp)
    add_fuzz_target(fuzz_rlp_encoder ${CMAKE_CURRENT_LIST_DIR}/fuzz_rlp_encoder.cpp)
    
    # Create a corpus directory
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_decoder)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_encoder)
    
    # Add some initial corpus files
    # Empty string
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_decoder/empty "")
    
    # Single byte values
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_decoder/single_00 "\x00")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_decoder/single_01 "\x01")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_decoder/single_7f "\x7f")
    
    # Short string
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_decoder/short_string "\x85hello")
    
    # Empty list
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_decoder/empty_list "\xc0")
    
    # List with one element
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_decoder/list_one "\xc1\x01")
    
    message(STATUS "Fuzzing enabled. Corpus directories created.")
    message(STATUS "To run fuzz tests:")
    message(STATUS "  ./fuzz_rlp_decoder -max_total_time=60 corpus_decoder")
    message(STATUS "  ./fuzz_rlp_encoder -max_total_time=60 corpus_encoder")
    
endif()
