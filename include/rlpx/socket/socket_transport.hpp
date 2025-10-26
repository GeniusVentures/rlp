// Copyright 2025 GeniusVentures
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../rlpx_types.hpp"
#include "../rlpx_error.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

namespace rlpx::socket {

// Hide Boost types (Law of Demeter)
template<typename T>
using Awaitable = boost::asio::awaitable<T>;

// Transport layer abstraction over TCP socket
// Provides async read/write operations with proper error handling
class SocketTransport {
public:
    using tcp = boost::asio::ip::tcp;
    using Strand = boost::asio::strand<boost::asio::any_io_executor>;

    // Create transport from connected socket
    explicit SocketTransport(tcp::socket socket) noexcept;

    // Non-copyable, moveable
    SocketTransport(const SocketTransport&) = delete;
    SocketTransport& operator=(const SocketTransport&) = delete;
    SocketTransport(SocketTransport&&) noexcept = default;
    SocketTransport& operator=(SocketTransport&&) noexcept = default;

    // Async read exact number of bytes
    [[nodiscard]] Awaitable<Result<ByteBuffer>>
    read_exact(size_t num_bytes) noexcept;

    // Async write all bytes
    [[nodiscard]] Awaitable<VoidResult>
    write_all(ByteView data) noexcept;

    // Close socket gracefully
    [[nodiscard]] VoidResult close() noexcept;

    // Query connection state
    [[nodiscard]] bool is_open() const noexcept;

    // Get remote endpoint info
    [[nodiscard]] std::string remote_address() const noexcept;
    [[nodiscard]] uint16_t remote_port() const noexcept;

    // Get local endpoint info
    [[nodiscard]] std::string local_address() const noexcept;
    [[nodiscard]] uint16_t local_port() const noexcept;

private:
    tcp::socket socket_;
    Strand strand_; // Ensures thread-safe sequential operations
};

// Connect to remote endpoint with timeout
[[nodiscard]] Awaitable<Result<SocketTransport>>
connect_with_timeout(
    boost::asio::any_io_executor executor,
    std::string_view host,
    uint16_t port,
    std::chrono::milliseconds timeout
) noexcept;

} // namespace rlpx::socket
