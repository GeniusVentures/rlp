# Valgrind.cmake
# Provides support for running tests under Valgrind for memory leak detection
#
# Usage:
#   -DENABLE_VALGRIND=ON          # Enable Valgrind memcheck
#   -DVALGRIND_OPTS="..."         # Additional Valgrind options

option(ENABLE_VALGRIND "Run tests under Valgrind" OFF)
set(VALGRIND_OPTS "" CACHE STRING "Additional options to pass to Valgrind")

if(ENABLE_VALGRIND)
    find_program(VALGRIND_EXECUTABLE valgrind)
    
    if(NOT VALGRIND_EXECUTABLE)
        message(WARNING "Valgrind requested but not found. Install valgrind or disable ENABLE_VALGRIND")
        set(ENABLE_VALGRIND OFF CACHE BOOL "Run tests under Valgrind" FORCE)
    else()
        message(STATUS "Valgrind found: ${VALGRIND_EXECUTABLE}")
        
        # Set default Valgrind options
        set(VALGRIND_DEFAULT_OPTS
            --leak-check=full
            --show-leak-kinds=all
            --track-origins=yes
            --verbose
            --error-exitcode=1
            --suppressions=${CMAKE_SOURCE_DIR}/valgrind.supp
        )
        
        # Combine default and user-specified options
        set(VALGRIND_COMMAND_OPTS ${VALGRIND_DEFAULT_OPTS} ${VALGRIND_OPTS})
        
        # Function to wrap a test with Valgrind
        function(add_valgrind_test test_name test_target)
            if(TARGET ${test_target})
                add_test(
                    NAME ${test_name}_valgrind
                    COMMAND ${VALGRIND_EXECUTABLE} ${VALGRIND_COMMAND_OPTS} $<TARGET_FILE:${test_target}>
                )
                set_tests_properties(${test_name}_valgrind PROPERTIES
                    TIMEOUT 300  # Valgrind tests take longer
                    LABELS "valgrind;memcheck"
                )
            endif()
        endfunction()
        
        # Function to add Valgrind to all tests
        function(add_valgrind_to_all_tests)
            get_property(ALL_TESTS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY TESTS)
            foreach(test_name ${ALL_TESTS})
                # Skip if it's already a valgrind test
                if(NOT test_name MATCHES "_valgrind$")
                    add_valgrind_test(${test_name} ${test_name})
                endif()
            endforeach()
        endfunction()
        
        message(STATUS "Valgrind options: ${VALGRIND_COMMAND_OPTS}")
        message(STATUS "To run only Valgrind tests: ctest -L valgrind")
    endif()
endif()
