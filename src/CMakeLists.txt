
# Define the static library target
add_library(rlp STATIC
        ${CMAKE_CURRENT_LIST_DIR}/rlp_encoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp_decoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/common.cpp
        ${CMAKE_CURRENT_LIST_DIR}/endian.cpp
        ${CMAKE_CURRENT_LIST_DIR}/PeerDiscovery/packet_factory.cpp
        ${CMAKE_CURRENT_LIST_DIR}/PeerDiscovery/Discv4Packet.cpp
        ${CMAKE_CURRENT_LIST_DIR}/PeerDiscovery/Discv4Ping.cpp
        ${CMAKE_CURRENT_LIST_DIR}/PeerDiscovery/Discv4Pong.cpp
)

# Set config of crypto3
set(crypto3_INCLUDE_DIR "${ZKLLVM_BUILD_DIR}/zkLLVM/include")
include_directories(${crypto3_INCLUDE_DIR})

# Set config of secp256k1 project
set(libsecp256k1_DIR "${_THIRDPARTY_BUILD_DIR}/libsecp256k1/lib/cmake/libsecp256k1")
find_package(libsecp256k1 CONFIG REQUIRED)

# Specify public include directory for BUILD interface
target_include_directories(rlp PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

# Link public dependencies
target_link_libraries(rlp PUBLIC
        Boost::boost
        libsecp256k1::secp256k1
)

# Ensure C++17 standard
target_compile_features(rlp PUBLIC cxx_std_17)

# Explicitly install public headers to the base include directory
install(FILES
        ${CMAKE_CURRENT_LIST_DIR}/../include/common.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp_decoder.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp_encoder.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/endian.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/PeerDiscovery/discovery.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/PeerDiscovery/packet_factory.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/PeerDiscovery/Discv4Packet.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/PeerDiscovery/Discv4Ping.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/PeerDiscovery/Discv4Pong.hpp
        # Add impl/endian.ipp if it exists in include/impl/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # Install directly into <prefix>/include/
)
