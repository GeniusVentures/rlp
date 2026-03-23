add_library(rlp_logger
    ../../include/base/rlp-logger.hpp
    rlp-logger.cpp
)

target_include_directories(rlp_logger
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(rlp_logger
    PUBLIC
    spdlog::spdlog
    fmt::fmt
)

install(TARGETS rlp_logger EXPORT rlp
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../include/base/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/base
        FILES_MATCHING PATTERN "*.hpp"
)