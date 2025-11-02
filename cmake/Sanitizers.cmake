# Sanitizers.cmake
# Provides options to enable various sanitizers for memory safety and undefined behavior detection
#
# Usage:
#   -DENABLE_ASAN=ON          # Enable AddressSanitizer
#   -DENABLE_UBSAN=ON         # Enable UndefinedBehaviorSanitizer
#   -DENABLE_TSAN=ON          # Enable ThreadSanitizer
#   -DENABLE_MSAN=ON          # Enable MemorySanitizer (Clang only)
#   -DENABLE_LSAN=ON          # Enable LeakSanitizer (can be combined with ASan)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer (Clang only)" OFF)
option(ENABLE_LSAN "Enable LeakSanitizer" OFF)

# Function to add sanitizer flags to a target
function(add_sanitizers target_name)
    if(NOT TARGET ${target_name})
        message(WARNING "Target ${target_name} does not exist, skipping sanitizer configuration")
        return()
    endif()
    
    set(SANITIZER_FLAGS "")
    set(SANITIZER_LINK_FLAGS "")
    
    # AddressSanitizer
    if(ENABLE_ASAN)
        message(STATUS "Enabling AddressSanitizer for ${target_name}")
        if(MSVC)
            list(APPEND SANITIZER_FLAGS /fsanitize=address)
            # Disable incremental linking for ASan on MSVC
            set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} /INCREMENTAL:NO")
        else()
            list(APPEND SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer)
            set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=address")
        endif()
    endif()
    
    # UndefinedBehaviorSanitizer
    if(ENABLE_UBSAN)
        message(STATUS "Enabling UndefinedBehaviorSanitizer for ${target_name}")
        if(MSVC)
            # MSVC doesn't have full UBSan support, but has some runtime checks
            list(APPEND SANITIZER_FLAGS /RTC1)
        else()
            list(APPEND SANITIZER_FLAGS
                -fsanitize=undefined
                -fno-sanitize-recover=undefined
                -fno-omit-frame-pointer
            )
            set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=undefined")
        endif()
    endif()
    
    # ThreadSanitizer (mutually exclusive with ASan/MSan)
    if(ENABLE_TSAN)
        if(ENABLE_ASAN OR ENABLE_MSAN)
            message(FATAL_ERROR "ThreadSanitizer cannot be used with AddressSanitizer or MemorySanitizer")
        endif()
        message(STATUS "Enabling ThreadSanitizer for ${target_name}")
        if(NOT MSVC)
            list(APPEND SANITIZER_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
            set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=thread")
        else()
            message(WARNING "ThreadSanitizer is not supported on MSVC")
        endif()
    endif()
    
    # MemorySanitizer (Clang only, mutually exclusive with ASan/TSan)
    if(ENABLE_MSAN)
        if(ENABLE_ASAN OR ENABLE_TSAN)
            message(FATAL_ERROR "MemorySanitizer cannot be used with AddressSanitizer or ThreadSanitizer")
        endif()
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(STATUS "Enabling MemorySanitizer for ${target_name}")
            list(APPEND SANITIZER_FLAGS
                -fsanitize=memory
                -fno-omit-frame-pointer
                -fsanitize-memory-track-origins=2
            )
            set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=memory")
        else()
            message(WARNING "MemorySanitizer is only supported with Clang")
        endif()
    endif()
    
    # LeakSanitizer (can be combined with ASan)
    if(ENABLE_LSAN AND NOT MSVC)
        message(STATUS "Enabling LeakSanitizer for ${target_name}")
        if(NOT ENABLE_ASAN)
            list(APPEND SANITIZER_FLAGS -fsanitize=leak -fno-omit-frame-pointer)
            set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=leak")
        endif()
        # LSan is automatically enabled with ASan, so no need to add it again
    endif()
    
    # Apply sanitizer flags to the target
    if(SANITIZER_FLAGS)
        target_compile_options(${target_name} PRIVATE ${SANITIZER_FLAGS})
        
        # Apply linker flags
        if(SANITIZER_LINK_FLAGS)
            get_target_property(TARGET_TYPE ${target_name} TYPE)
            if(NOT TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
                if(MSVC)
                    set_target_properties(${target_name} PROPERTIES
                        LINK_FLAGS "${SANITIZER_LINK_FLAGS}"
                    )
                else()
                    target_link_options(${target_name} PRIVATE ${SANITIZER_LINK_FLAGS})
                endif()
            endif()
        endif()
    endif()
endfunction()

# Global function to add sanitizers to all targets
function(add_sanitizers_to_all_targets)
    if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN OR ENABLE_MSAN OR ENABLE_LSAN)
        message(STATUS "Sanitizers enabled globally")
        
        # Add compile options globally
        set(GLOBAL_SANITIZER_FLAGS "")
        
        if(ENABLE_ASAN)
            if(MSVC)
                add_compile_options(/fsanitize=address)
            else()
                add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
                add_link_options(-fsanitize=address)
            endif()
        endif()
        
        if(ENABLE_UBSAN AND NOT MSVC)
            add_compile_options(-fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
            add_link_options(-fsanitize=undefined)
        endif()
        
        if(ENABLE_TSAN AND NOT MSVC)
            add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
            add_link_options(-fsanitize=thread)
        endif()
        
        if(ENABLE_MSAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-fsanitize=memory -fno-omit-frame-pointer -fsanitize-memory-track-origins=2)
            add_link_options(-fsanitize=memory)
        endif()
        
        if(ENABLE_LSAN AND NOT ENABLE_ASAN AND NOT MSVC)
            add_compile_options(-fsanitize=leak -fno-omit-frame-pointer)
            add_link_options(-fsanitize=leak)
        endif()
        
        # Set environment variables for better error reporting
        set(ENV{ASAN_OPTIONS} "detect_leaks=1:check_initialization_order=1:strict_init_order=1")
        set(ENV{UBSAN_OPTIONS} "print_stacktrace=1:halt_on_error=1")
        set(ENV{TSAN_OPTIONS} "second_deadlock_stack=1")
    endif()
endfunction()

# Print status of enabled sanitizers
if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN OR ENABLE_MSAN OR ENABLE_LSAN)
    message(STATUS "=== Sanitizers Configuration ===")
    if(ENABLE_ASAN)
        message(STATUS "  AddressSanitizer: ENABLED")
    endif()
    if(ENABLE_UBSAN)
        message(STATUS "  UndefinedBehaviorSanitizer: ENABLED")
    endif()
    if(ENABLE_TSAN)
        message(STATUS "  ThreadSanitizer: ENABLED")
    endif()
    if(ENABLE_MSAN)
        message(STATUS "  MemorySanitizer: ENABLED")
    endif()
    if(ENABLE_LSAN)
        message(STATUS "  LeakSanitizer: ENABLED")
    endif()
    message(STATUS "================================")
endif()
