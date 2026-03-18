# Define the static library target
add_library(discv4 STATIC
        ${CMAKE_CURRENT_LIST_DIR}/packet_factory.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv4_packet.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv4_ping.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv4_pong.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv4_error.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv4_client.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv4_enr_request.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv4_enr_response.cpp
)

target_include_directories(discv4 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../include>
        $<INSTALL_INTERFACE:include>
        ${crypto3_INCLUDE_DIR}
)

# Link public dependencies
target_link_libraries(discv4 PUBLIC
    rlpx
)

# Ensure C++17 standard
target_compile_features(discv4 PUBLIC cxx_std_17)

# Explicitly install public headers to the base include directory
install(FILES
        # discv4 Peer Discovery
        ${CMAKE_CURRENT_LIST_DIR}/../include/discv4/discovery.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/discv4/packet_factory.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/discv4/discv4Packet.hpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/discv4/discv4Ping.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../include/discv4/discv4Pong.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/discv4
)

install(DIRECTORY
        ${CMAKE_CURRENT_LIST_DIR}/../include/discv4/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/discv4
        FILES_MATCHING PATTERN "*.hpp"
)


