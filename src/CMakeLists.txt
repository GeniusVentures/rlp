
# Define the static library target
add_library(rlp STATIC
        ${CMAKE_CURRENT_LIST_DIR}/rlp_encoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp_decoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/common.cpp
        ${CMAKE_CURRENT_LIST_DIR}/endian.cpp
)

# Specify public include directory for BUILD interface
target_include_directories(rlp PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

# Link public dependencies
target_link_libraries(rlp PUBLIC
        Boost::boost
)

# Ensure C++17 standard
target_compile_features(rlp PUBLIC cxx_std_17)

# Explicitly install public headers to the base include directory
install(FILES
        ${CMAKE_CURRENT_LIST_DIR}/../include/common.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp_decoder.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp_encoder.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/endian.hpp
        # Add impl/endian.ipp if it exists in include/impl/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # Install directly into <prefix>/include/
)
