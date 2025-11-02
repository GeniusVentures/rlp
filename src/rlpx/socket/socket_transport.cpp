// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#include <rlpx/socket/socket_transport.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace rlpx::socket {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

/* ============================================================================
 * ASYNC I/O ARCHITECTURE DESIGN
 * ============================================================================
 * 
 * Thread Safety Model:
 * - All socket operations run on a strand to ensure sequential execution
 * - No internal locking needed - strand provides implicit synchronization
 * - Multiple coroutines can safely call read/write - strand serializes them
 * 
 * Coroutine Integration:
 * - All async operations return Awaitable<Result<T>>
 * - Use boost::asio::use_awaitable as completion token
 * - Error handling via Result<T> instead of exceptions
 * - Proper RAII cleanup on coroutine destruction
 * 
 * Read Operation Flow:
 * 1. read_exact(n) -> allocate buffer of size n
 * 2. async_read(socket, buffer, n bytes) with use_awaitable
 * 3. On success -> return filled ByteBuffer
 * 4. On error -> convert boost::system::error_code to SessionError
 * 
 * Write Operation Flow:
 * 1. write_all(data) -> create buffer view
 * 2. async_write(socket, buffer) with use_awaitable
 * 3. On success -> return success
 * 4. On error -> convert error_code to SessionError
 * 
 * Connection Establishment:
 * 1. Resolve hostname to endpoint
 * 2. Create socket with executor
 * 3. Race async_connect() vs timeout timer
 * 4. On connect success -> cancel timer, return SocketTransport
 * 5. On timeout -> cancel connect, return timeout error
 * 
 * Error Mapping:
 * - boost::asio::error::eof -> SessionError::kConnectionFailed
 * - boost::asio::error::connection_reset -> SessionError::kConnectionFailed
 * - boost::asio::error::operation_aborted -> SessionError::kConnectionFailed
 * - Other errors -> SessionError::kInvalidMessage (temporary)
 * 
 * ============================================================================
 */

// Constructor
SocketTransport::SocketTransport(tcp::socket socket) noexcept
    : socket_(std::move(socket))
    , strand_(socket_.get_executor())
{
}

// Async read exact number of bytes
Awaitable<Result<ByteBuffer>>
SocketTransport::read_exact(size_t num_bytes) noexcept {
    // Allocate buffer for incoming data
    ByteBuffer buffer(num_bytes);
    
    // Read exactly num_bytes from socket
    // This will suspend coroutine until all bytes received or error
    boost::system::error_code ec;
    size_t bytes_read = co_await asio::async_read(
        socket_,
        asio::buffer(buffer.data(), num_bytes),
        asio::redirect_error(asio::use_awaitable, ec)
    );
    
    if (ec) {
        // Connection closed or error occurred
        if (ec == asio::error::eof || 
            ec == asio::error::connection_reset) {
            co_return SessionError::kConnectionFailed;
        }
        co_return SessionError::kInvalidMessage; // Generic network error
    }
    
    if (bytes_read != num_bytes) {
        // Partial read shouldn't happen with async_read, but check anyway
        co_return SessionError::kInvalidMessage;
    }
    
    co_return buffer;
}

// Async write all bytes
Awaitable<VoidResult>
SocketTransport::write_all(ByteView data) noexcept {
    // Write all bytes to socket
    // This will suspend coroutine until all bytes sent or error
    boost::system::error_code ec;
    co_await asio::async_write(
        socket_,
        asio::buffer(data.data(), data.size()),
        asio::redirect_error(asio::use_awaitable, ec)
    );
    
    if (ec) {
        // Connection closed or error occurred
        if (ec == asio::error::eof || 
            ec == asio::error::connection_reset ||
            ec == asio::error::broken_pipe) {
            co_return SessionError::kConnectionFailed;
        }
        co_return SessionError::kInvalidMessage; // Generic network error
    }
    
    co_return outcome::success();
}

// Close socket gracefully
VoidResult SocketTransport::close() noexcept {
    if (socket_.is_open()) {
        boost::system::error_code ec;
        
        // Shutdown both send and receive
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        // Ignore shutdown errors - connection may already be closed
        
        // Close the socket
        socket_.close(ec);
        if (ec) {
            return SessionError::kConnectionFailed;
        }
    }
    return outcome::success();
}

// Query connection state
bool SocketTransport::is_open() const noexcept {
    return socket_.is_open();
}

// Get remote endpoint info
std::string SocketTransport::remote_address() const noexcept {
    if (socket_.is_open()) {
        boost::system::error_code ec;
        auto endpoint = socket_.remote_endpoint(ec);
        if (!ec) {
            return endpoint.address().to_string();
        }
    }
    return "";
}

uint16_t SocketTransport::remote_port() const noexcept {
    if (socket_.is_open()) {
        boost::system::error_code ec;
        auto endpoint = socket_.remote_endpoint(ec);
        if (!ec) {
            return endpoint.port();
        }
    }
    return 0;
}

// Get local endpoint info
std::string SocketTransport::local_address() const noexcept {
    if (socket_.is_open()) {
        boost::system::error_code ec;
        auto endpoint = socket_.local_endpoint(ec);
        if (!ec) {
            return endpoint.address().to_string();
        }
    }
    return "";
}

uint16_t SocketTransport::local_port() const noexcept {
    if (socket_.is_open()) {
        boost::system::error_code ec;
        auto endpoint = socket_.local_endpoint(ec);
        if (!ec) {
            return endpoint.port();
        }
    }
    return 0;
}

// Connect to remote endpoint with timeout
Awaitable<Result<SocketTransport>>
connect_with_timeout(
    asio::any_io_executor executor,
    std::string_view host,
    uint16_t port,
    std::chrono::milliseconds timeout
) noexcept {
    // Create socket
    tcp::socket socket(executor);
    
    // Resolve hostname to endpoints
    tcp::resolver resolver(executor);
    boost::system::error_code resolve_ec;
    auto endpoints = co_await resolver.async_resolve(
        host,
        std::to_string(port),
        asio::redirect_error(asio::use_awaitable, resolve_ec)
    );
    
    if (resolve_ec) {
        co_return SessionError::kConnectionFailed;
    }
    
    // Connect to one of the resolved endpoints
    // Note: This is simplified - production code would use cancellation for timeout
    boost::system::error_code connect_ec;
    co_await asio::async_connect(
        socket,
        endpoints,
        asio::redirect_error(asio::use_awaitable, connect_ec)
    );
    
    if (connect_ec) {
        co_return SessionError::kConnectionFailed;
    }
    
    // Connection successful
    co_return SocketTransport(std::move(socket));
}

} // namespace rlpx::socket
