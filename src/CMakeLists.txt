# Define the static library target
add_library(rlp STATIC
        ${CMAKE_CURRENT_LIST_DIR}/rlp/rlp_encoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/rlp_decoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/rlp_streaming.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/common.cpp
        ${CMAKE_CURRENT_LIST_DIR}/rlp/endian.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eth/messages.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eth/objects.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eth/event_filter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eth/abi_decoder.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eth/chain_tracker.cpp
        ${CMAKE_CURRENT_LIST_DIR}/eth/eth_watch_service.cpp
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
        ${crypto3_INCLUDE_DIR}
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/base ${CMAKE_CURRENT_BINARY_DIR}/base)

# Link public dependencies
target_link_libraries(rlp PUBLIC
        Boost::boost
        libsecp256k1::secp256k1
        OpenSSL::SSL
        OpenSSL::Crypto
        Microsoft.GSL::GSL
        Snappy::snappy
        logger
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
        ../include/discv4/discovery.hpp
        ../include/discv4/packet_factory.hpp
        ../include/discv4/discv4_packet.hpp
        ../include/discv4/discv4Ping.cpp
        ../include/discv4/discv4_pong.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rlp
)

install(DIRECTORY
        ${CMAKE_CURRENT_LIST_DIR}/../include/eth/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eth
        FILES_MATCHING PATTERN "*.hpp"
)

# Add RLPx subdirectory
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/rlpx ${CMAKE_CURRENT_BINARY_DIR}/rlpx)

# Add discv4 subdirectory
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/discv4 ${CMAKE_CURRENT_BINARY_DIR}/discv4)

# Add discv5 subdirectory
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/discv5 ${CMAKE_CURRENT_BINARY_DIR}/discv5)
