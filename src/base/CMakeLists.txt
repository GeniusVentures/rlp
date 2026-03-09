add_library(logger
        ../../include/base/logger.hpp
    logger.cpp
)
target_link_libraries(logger
    PUBLIC
    spdlog::spdlog
    fmt::fmt
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../../include/base/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/base
        FILES_MATCHING PATTERN "*.hpp"
)