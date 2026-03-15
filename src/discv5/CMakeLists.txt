# discv5 peer discovery library
add_library(discv5 STATIC
        ${CMAKE_CURRENT_LIST_DIR}/discv5_error.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv5_enr.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv5_bootnodes.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv5_crawler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/discv5_client.cpp
)

target_include_directories(discv5 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${crypto3_INCLUDE_DIR}
)

target_link_libraries(discv5 PUBLIC
        Boost::boost
        libsecp256k1::secp256k1
        logger
)

target_compile_features(discv5 PUBLIC cxx_std_17)

install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/include/discv5/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/discv5
        FILES_MATCHING PATTERN "*.hpp"
)

install(DIRECTORY
        ${CMAKE_SOURCE_DIR}/include/discovery/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/discovery
        FILES_MATCHING PATTERN "*.hpp"
)
