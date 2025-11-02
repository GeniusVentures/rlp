/**
 * @file boost_exception_stub.cpp
 * @brief Stub implementation for Boost exception handling when exceptions are disabled
 * 
 * When BOOST_NO_EXCEPTIONS is defined, Boost requires these functions to be implemented.
 * Since we're using Result<> for error handling, we terminate the program if Boost
 * attempts to throw an exception (which should never happen in our code paths).
 */

#include <cstdlib>
#include <exception>
#include <iostream>

// Include Boost config to get proper source_location support
#include <boost/config.hpp>
#include <boost/assert/source_location.hpp>

namespace boost {

/**
 * @brief Custom exception handler for Boost when exceptions are disabled
 * 
 * This function is called by Boost when it would normally throw an exception.
 * Since we've designed our code to avoid exception paths, this should never be called.
 * If it is called, we terminate the program to catch the bug during development.
 */
[[noreturn]] void throw_exception(std::exception const& e) {
    std::cerr << "FATAL: Boost attempted to throw exception: " << e.what() << std::endl;
    std::cerr << "This indicates a bug - exception paths should not be reachable" << std::endl;
    std::abort();
}

/**
 * @brief Custom exception handler with source location
 */
[[noreturn]] void throw_exception(std::exception const& e, 
                                   source_location const& loc) {
    std::cerr << "FATAL: Boost attempted to throw exception at "
              << loc.file_name() << ":" << loc.line() << std::endl;
    std::cerr << "Exception: " << e.what() << std::endl;
    std::cerr << "This indicates a bug - exception paths should not be reachable" << std::endl;
    std::abort();
}

} // namespace boost
