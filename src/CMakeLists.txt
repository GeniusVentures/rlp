# Define the static library target
add_library(rlp STATIC
        ${CMAKE_CURRENT_LIST_DIR}/rlp/rlp_encoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/rlp_decoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/rlp_streaming.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/common.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/endian.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/PeerDiscovery/packet_factory.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/PeerDiscovery/Discv4Packet.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/PeerDiscovery/Discv4Ping.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/PeerDiscovery/Discv4Pong.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eth/messages.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eth/objects.cpp
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
        # Core headers
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/common.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/types.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/constants.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/errors.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/result.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/traits.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/intx.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/endian.hpp
        # Encoder/Decoder
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/rlp_encoder.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/rlp_decoder.hpp
        # Streaming API
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/rlp_streaming.hpp
        # Peer Discovery
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/PeerDiscovery/discovery.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/PeerDiscovery/packet_factory.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/PeerDiscovery/Discv4Packet.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/PeerDiscovery/Discv4Ping.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/rlp/PeerDiscovery/Discv4Pong.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rlp
)

install(DIRECTORY
        ${CMAKE_CURRENT_LIST_DIR}/../include/eth/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eth
        FILES_MATCHING PATTERN "*.hpp"
)

# Add RLPx subdirectory
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/rlpx ${CMAKE_CURRENT_BINARY_DIR}/rlpx)
