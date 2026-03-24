cmake_minimum_required(VERSION 3.16)

set(
        CMAKE_TOOLCHAIN_FILE
        "${CMAKE_SOURCE_DIR}/cmake/toolchain/cxx17.cmake"
        CACHE
        FILEPATH
        "Default toolchain"
)
add_definitions(-D_WIN32_WINNT=0x0601)
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

# Project definition
project(rlp
        VERSION 1.0.0
        DESCRIPTION "rlp/rlpx/discv4/discv5/eth library for C++"
        LANGUAGES C CXX
)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf CONFIG REQUIRED)

if(NOT TARGET protobuf::protoc)
    add_executable(protobuf::protoc IMPORTED)
endif()

if(EXISTS "${Protobuf_PROTOC_EXECUTABLE}")
    set_target_properties(protobuf::protoc PROPERTIES IMPORTED_LOCATION ${Protobuf_PROTOC_EXECUTABLE})
endif()

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${GSL_INCLUDE_DIR})
find_package(libsecp256k1 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS date_time filesystem random regex system thread log log_setup program_options json context coroutine)
find_package(Snappy CONFIG REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
endif()

add_subdirectory(src)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/rlp" FILES_MATCHING PATTERN "*.h*")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/generated" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/rlp" FILES_MATCHING PATTERN "*.h*")

set(rlp_CONFIG_DESTINATION_DIR "lib/cmake/rlp")
install(EXPORT rlp
        FILE rlpTargets.cmake
        NAMESPACE rlp::
        DESTINATION ${rlp_CONFIG_DESTINATION_DIR}
)

include(CMakePackageConfigHelpers)

# generate the config file that is includes the exports
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/rlpConfig.cmake"
        INSTALL_DESTINATION ${rlp_CONFIG_DESTINATION_DIR}
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# generate the version file for the config file
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/rlpConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY AnyNewerVersion
)

# install the configuration file
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/rlpConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/rlpConfigVersion.cmake
        DESTINATION ${rlp_CONFIG_DESTINATION_DIR}
)
