# RLPx CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Define the library target
add_library(rlpx STATIC
    ${CMAKE_CURRENT_LIST_DIR}/rlpx_error.cpp
    ${CMAKE_CURRENT_LIST_DIR}/auth/ecies_cipher.cpp
    ${CMAKE_CURRENT_LIST_DIR}/auth/auth_handshake.cpp
    ${CMAKE_CURRENT_LIST_DIR}/crypto/kdf.cpp
    ${CMAKE_CURRENT_LIST_DIR}/crypto/ecdh.cpp
    ${CMAKE_CURRENT_LIST_DIR}/crypto/hmac.cpp
    ${CMAKE_CURRENT_LIST_DIR}/crypto/aes.cpp
    ${CMAKE_CURRENT_LIST_DIR}/framing/frame_cipher.cpp
    ${CMAKE_CURRENT_LIST_DIR}/framing/message_stream.cpp
    ${CMAKE_CURRENT_LIST_DIR}/protocol/messages.cpp
    ${CMAKE_CURRENT_LIST_DIR}/socket/socket_transport.cpp
    ${CMAKE_CURRENT_LIST_DIR}/rlpx_session.cpp
)

# Specify public include directory
target_include_directories(rlpx PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../include>
    $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(rlpx PUBLIC
    Boost::boost
    libsecp256k1::secp256k1
    OpenSSL::SSL
    OpenSSL::Crypto
    Microsoft.GSL::GSL
    rlp
)

# Ensure C++17 standard
target_compile_features(rlpx PUBLIC cxx_std_17)

# Disable exceptions to ensure we use Result<> instead
if(MSVC)
    target_compile_options(rlpx PRIVATE /EHs-c-)
    target_compile_definitions(rlpx PRIVATE _HAS_EXCEPTIONS=0 BOOST_NO_EXCEPTIONS)
else()
    target_compile_options(rlpx PRIVATE -fno-exceptions)
    target_compile_definitions(rlpx PRIVATE BOOST_NO_EXCEPTIONS)
endif()

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../include/rlpx/
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
