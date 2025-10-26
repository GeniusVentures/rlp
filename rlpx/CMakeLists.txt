# RLPx CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Define the library target
add_library(rlpx STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/rlpx_error.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/auth/ecies_cipher.cpp
    # Additional source files will be added as they are implemented
    # ${CMAKE_CURRENT_LIST_DIR}/src/auth/auth_handshake.cpp
    # ${CMAKE_CURRENT_LIST_DIR}/src/framing/frame_cipher.cpp
    # ${CMAKE_CURRENT_LIST_DIR}/src/framing/message_stream.cpp
    # ${CMAKE_CURRENT_LIST_DIR}/src/rlpx_session.cpp
)

# Specify public include directory
target_include_directories(rlpx PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:include/rlpx>
)

# Link dependencies
target_link_libraries(rlpx PUBLIC
    Boost::boost
    libsecp256k1::secp256k1
    OpenSSL::SSL
    OpenSSL::Crypto
    Microsoft.GSL::GSL
)

# Ensure C++17 standard
target_compile_features(rlpx PUBLIC cxx_std_17)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rlpx
    FILES_MATCHING PATTERN "*.hpp"
)

# Install library
install(TARGETS rlpx
    EXPORT rlpx-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
