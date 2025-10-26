# BOOST VERSION TO USE
set(BOOST_MAJOR_VERSION "1" CACHE STRING "Boost Major Version")
set(BOOST_MINOR_VERSION "85" CACHE STRING "Boost Minor Version")
set(BOOST_PATCH_VERSION "0" CACHE STRING "Boost Patch Version")

# convenience settings
set(BOOST_VERSION "${BOOST_MAJOR_VERSION}.${BOOST_MINOR_VERSION}.${BOOST_PATCH_VERSION}")
set(BOOST_VERSION_2U "${BOOST_MAJOR_VERSION}_${BOOST_MINOR_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DEFINED USE_BOOST_INCLUDE_POSTFIX)
        set(BOOST_INCLUDE_POSTFIX "/boost-${BOOST_VERSION_2U}" CACHE STRING "Boost include postfix")
endif()

# --------------------------------------------------------
# Set config of GTest
set(GTest_DIR "${_THIRDPARTY_BUILD_DIR}/GTest/lib/cmake/GTest")
set(GTest_INCLUDE_DIR "${_THIRDPARTY_BUILD_DIR}/GTest/include")
find_package(GTest CONFIG REQUIRED)
include_directories(${GTest_INCLUDE_DIR})
add_compile_definitions(CRYPTO3_CODEC_BASE58)

# --------------------------------------------------------
# Set config of OpenSSL
find_package(OpenSSL REQUIRED)

# --------------------------------------------------------
# Set config of Microsoft GSL (header-only library)
set(GSL_INCLUDE_DIR "${_THIRDPARTY_BUILD_DIR}/Microsoft.GSL/include")
include_directories(${GSL_INCLUDE_DIR})
# Create interface library for GSL
add_library(Microsoft.GSL INTERFACE IMPORTED)
set_target_properties(Microsoft.GSL PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GSL_INCLUDE_DIR}"
)
add_library(Microsoft.GSL::GSL ALIAS Microsoft.GSL)

# Boost should be loaded before libp2p v0.1.2
# --------------------------------------------------------
# Set config of Boost project
set(_BOOST_ROOT "${_THIRDPARTY_BUILD_DIR}/boost/build")
set(Boost_LIB_DIR "${_BOOST_ROOT}/lib")
set(Boost_INCLUDE_DIR "${_BOOST_ROOT}/include/boost-${BOOST_VERSION_2U}")
set(Boost_DIR "${Boost_LIB_DIR}/cmake/Boost-${BOOST_VERSION}")
set(boost_atomic_DIR "${Boost_LIB_DIR}/cmake/boost_atomic-${BOOST_VERSION}")
set(boost_chrono_DIR "${Boost_LIB_DIR}/cmake/boost_chrono-${BOOST_VERSION}")
set(boost_container_DIR "${Boost_LIB_DIR}/cmake/boost_container-${BOOST_VERSION}")
set(boost_date_time_DIR "${Boost_LIB_DIR}/cmake/boost_date_time-${BOOST_VERSION}")
set(boost_filesystem_DIR "${Boost_LIB_DIR}/cmake/boost_filesystem-${BOOST_VERSION}")
set(boost_headers_DIR "${Boost_LIB_DIR}/cmake/boost_headers-${BOOST_VERSION}")
set(boost_json_DIR "${Boost_LIB_DIR}/cmake/boost_json-${BOOST_VERSION}")
set(boost_log_DIR "${Boost_LIB_DIR}/cmake/boost_log-${BOOST_VERSION}")
set(boost_log_setup_DIR "${Boost_LIB_DIR}/cmake/boost_log_setup-${BOOST_VERSION}")
set(boost_program_options_DIR "${Boost_LIB_DIR}/cmake/boost_program_options-${BOOST_VERSION}")
set(boost_random_DIR "${Boost_LIB_DIR}/cmake/boost_random-${BOOST_VERSION}")
set(boost_regex_DIR "${Boost_LIB_DIR}/cmake/boost_regex-${BOOST_VERSION}")
set(boost_system_DIR "${Boost_LIB_DIR}/cmake/boost_system-${BOOST_VERSION}")
set(boost_thread_DIR "${Boost_LIB_DIR}/cmake/boost_thread-${BOOST_VERSION}")
set(boost_unit_test_framework_DIR "${Boost_LIB_DIR}/cmake/boost_unit_test_framework-${BOOST_VERSION}")
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_NO_SYSTEM_PATHS ON)
option(Boost_USE_STATIC_RUNTIME "Use static runtimes" ON)

# header only libraries must not be added here
find_package(Boost REQUIRED COMPONENTS date_time filesystem random regex system thread log log_setup program_options)
include_directories(${Boost_INCLUDE_DIRS})

# --------------------------------------------------------
# set config for crypto3
option(BUILD_TESTS "Build tests" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_APPS "Enable application targets." FALSE)
option(BUILD_EXAMPLES "Enable demonstration targets." FALSE)
option(BUILD_DOCS "Enable documentation targets." FALSE)
set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/docs" CACHE STRING "Specify doxygen output directory")

include_directories(
        "${CMAKE_CURRENT_LIST_DIR}/../include"
)

include("${CMAKE_CURRENT_LIST_DIR}/../src/CMakeLists.txt")

if(BUILD_TESTS)
        add_executable(${PROJECT_NAME}_encoder_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_encoder_tests.cpp"
        )

        add_executable(${PROJECT_NAME}_decoder_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_decoder_tests.cpp"
        )

        add_executable(discovery_test
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/discovery_test.cpp"
        )

        add_executable(${PROJECT_NAME}_endian_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_endian_tests.cpp"
        )

        add_executable(${PROJECT_NAME}_edge_cases
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_edge_cases.cpp"
        )

        # Use main benchmark and property test files
                add_executable(${PROJECT_NAME}_benchmark_tests
                        "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_benchmark_tests.cpp"
                )

                add_executable(${PROJECT_NAME}_property_tests
                        "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_property_tests.cpp"
                )

                add_executable(${PROJECT_NAME}_comprehensive_tests
                        "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_comprehensive_tests.cpp"
                )

                add_executable(rlp_ethereum_tests
                        "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_ethereum_tests.cpp"
                )

                add_executable(rlp_random_tests
                        "${CMAKE_CURRENT_LIST_DIR}/../test/rlp/rlp_random_tests.cpp"
                )

        target_link_libraries(${PROJECT_NAME}_encoder_tests PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
        target_link_libraries(${PROJECT_NAME}_decoder_tests PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
        target_link_libraries(${PROJECT_NAME}_endian_tests PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
        target_link_libraries(${PROJECT_NAME}_edge_cases PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
        target_link_libraries(discovery_test PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
        target_link_libraries(${PROJECT_NAME}_benchmark_tests PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
        target_link_libraries(${PROJECT_NAME}_property_tests PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
        target_link_libraries(${PROJECT_NAME}_comprehensive_tests PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
                target_link_libraries(rlp_ethereum_tests PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
                target_link_libraries(rlp_random_tests PUBLIC ${PROJECT_NAME} GTest::gtest Boost::boost)
        
        # Add RLPx tests
        add_executable(rlpx_crypto_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlpx/crypto_test.cpp"
        )
        add_executable(rlpx_frame_cipher_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlpx/frame_cipher_test.cpp"
        )
        add_executable(rlpx_protocol_messages_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlpx/protocol_messages_test.cpp"
        )
        add_executable(rlpx_session_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlpx/rlpx_session_test.cpp"
        )
        add_executable(rlpx_state_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlpx/rlpx_state_test.cpp"
        )
        add_executable(rlpx_message_routing_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlpx/message_routing_test.cpp"
        )
        add_executable(rlpx_socket_lifecycle_tests
                "${CMAKE_CURRENT_LIST_DIR}/../test/rlpx/socket_lifecycle_test.cpp"
        )
        
        target_link_libraries(rlpx_crypto_tests PUBLIC rlpx GTest::gtest_main Boost::boost)
        target_link_libraries(rlpx_frame_cipher_tests PUBLIC rlpx GTest::gtest_main Boost::boost)
        target_link_libraries(rlpx_protocol_messages_tests PUBLIC rlpx ${PROJECT_NAME} GTest::gtest_main Boost::boost)
        target_link_libraries(rlpx_session_tests PUBLIC rlpx ${PROJECT_NAME} GTest::gtest_main Boost::boost)
        target_link_libraries(rlpx_state_tests PUBLIC rlpx GTest::gtest_main Boost::boost)
        target_link_libraries(rlpx_message_routing_tests PUBLIC rlpx ${PROJECT_NAME} GTest::gtest_main Boost::boost)
        target_link_libraries(rlpx_socket_lifecycle_tests PUBLIC rlpx ${PROJECT_NAME} GTest::gtest_main Boost::boost)
                # Register all test executables with CTest
                enable_testing()
                add_test(NAME rlp_encoder_tests COMMAND $<TARGET_FILE:${PROJECT_NAME}_encoder_tests>)
                add_test(NAME rlp_decoder_tests COMMAND $<TARGET_FILE:${PROJECT_NAME}_decoder_tests>)
                add_test(NAME discovery_test COMMAND $<TARGET_FILE:discovery_test>)
                add_test(NAME rlp_endian_tests COMMAND $<TARGET_FILE:${PROJECT_NAME}_endian_tests>)
                add_test(NAME rlp_edge_cases COMMAND $<TARGET_FILE:${PROJECT_NAME}_edge_cases>)
                add_test(NAME rlp_benchmark_tests COMMAND $<TARGET_FILE:${PROJECT_NAME}_benchmark_tests>)
                add_test(NAME rlp_property_tests COMMAND $<TARGET_FILE:${PROJECT_NAME}_property_tests>)
                add_test(NAME rlp_comprehensive_tests COMMAND $<TARGET_FILE:${PROJECT_NAME}_comprehensive_tests>)
                add_test(NAME rlp_ethereum_tests COMMAND $<TARGET_FILE:rlp_ethereum_tests>)
                add_test(NAME rlp_random_tests COMMAND $<TARGET_FILE:rlp_random_tests>)
                add_test(NAME rlpx_crypto_tests COMMAND $<TARGET_FILE:rlpx_crypto_tests>)
                add_test(NAME rlpx_frame_cipher_tests COMMAND $<TARGET_FILE:rlpx_frame_cipher_tests>)
                add_test(NAME rlpx_protocol_messages_tests COMMAND $<TARGET_FILE:rlpx_protocol_messages_tests>)
                add_test(NAME rlpx_session_tests COMMAND $<TARGET_FILE:rlpx_session_tests>)
                add_test(NAME rlpx_state_tests COMMAND $<TARGET_FILE:rlpx_state_tests>)
                add_test(NAME rlpx_message_routing_tests COMMAND $<TARGET_FILE:rlpx_message_routing_tests>)
                add_test(NAME rlpx_socket_lifecycle_tests COMMAND $<TARGET_FILE:rlpx_socket_lifecycle_tests>)
        
endif()

# Install Headers
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.h*")

install(TARGETS ${PROJECT_NAME} EXPORT RLPTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FRAMEWORK DESTINATION ${CMAKE_INSTALL_PREFIX}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
        EXPORT RLPTargets
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rlp
        NAMESPACE rlp::
)

include(CMakePackageConfigHelpers)

# generate the config file that is includes the exports
configure_package_config_file(${PROJECT_ROOT}/cmake/config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/RLPConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rlp
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# generate the version file for the config file
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/RLPConfigVersion.cmake"
        VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}"
        COMPATIBILITY AnyNewerVersion
)

# install the configuration file
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/RLPConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rlp
)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/RLPConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rlp
)
