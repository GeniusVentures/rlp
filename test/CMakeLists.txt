# Define the test executable using the addtest function from cmake/functions.cmake
# This function links GTest::gtest_main and GTest::gmock_main automatically
addtest(rlp_test
        rlp_test.cpp
)

# Link the library under test (rlp) using the target namespace defined during install export
# If building within the same project, CMake might resolve 'rlp' directly.
# Using the namespaced target 'rlp::rlp' is safer for consistency with installed usage.
# However, the target 'rlp::rlp' only exists *after* installation or if defined as an ALIAS.
# Let's try linking directly to the target 'rlp' defined in src/.
target_link_libraries(rlp_test PRIVATE
        rlp # Link directly to the library target defined in src/
)

# Ensure GTest headers are found (should be handled by CommonBuildParameters finding GTest)
# target_include_directories(rlp_test PRIVATE ${GTest_INCLUDE_DIR}) # Likely not needed