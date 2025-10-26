# RLPx Protocol Implementation

## Overview

This directory contains the implementation of the RLPx transport protocol for secure P2P Ethereum connections.

## Architecture

The implementation follows a layered architecture:

```
┌─────────────────────────────────────┐
│         RlpxSession                 │  ← High-level session API
├─────────────────────────────────────┤
│       MessageStream                 │  ← Message framing & compression
├─────────────────────────────────────┤
│       FrameCipher                   │  ← AES-CTR encryption + MAC
├─────────────────────────────────────┤
│       AuthHandshake                 │  ← ECIES handshake
├─────────────────────────────────────┤
│       EciesCipher                   │  ← ECIES encryption primitives
└─────────────────────────────────────┘
```

## Directory Structure

- `include/` - Public headers
  - `auth/` - Authentication and ECIES encryption
  - `framing/` - Message framing and encryption
  - `crypto/` - Cryptographic primitives
  - `protocol/` - Protocol messages (Hello, Disconnect, Ping/Pong)
- `src/` - Implementation files
- `test/` - Unit and integration tests

## Design Principles

1. **Const-correctness**: Parameters passed by `const&`, grouped values returned by `const&`
2. **Named initialization**: Structs use designated initializers for clarity
3. **Minimal allocations**: Buffers reused, move semantics preferred
4. **Lock-free**: Atomic operations and channels, no mutexes in hot paths
5. **Unique ownership**: `unique_ptr` preferred over `shared_ptr`
6. **Law of Demeter**: Boost types hidden behind type aliases
7. **Outcome-based errors**: No exceptions in hot paths

## Usage Example

```cpp
#include <rlpx_session.hpp>

boost::asio::awaitable<void> example() {
    // Connect to peer
    auto session = co_await rlpx::RlpxSession::connect({
        .remote_host = "127.0.0.1",
        .remote_port = 30303,
        .local_public_key = node_key.public_key(),
        .local_private_key = node_key.private_key(),
        .peer_public_key = remote_peer_key,
        .client_id = "MyClient/v1.0",
        .listen_port = 30303
    });
    
    if (!session) {
        // Handle error
        co_return;
    }
    
    // Send ping
    co_await (*session)->post_message({
        .id = rlpx::kPingMessageId,
        .payload = build_ping()
    });
    
    // Receive response
    auto msg = co_await (*session)->receive_message();
    if (msg && msg->id == rlpx::kPongMessageId) {
        // Handle pong
    }
    
    // Disconnect gracefully
    co_await (*session)->disconnect(rlpx::DisconnectReason::kClientQuitting);
}
```

## Status

🚧 **Phase 3 Implementation In Progress**

- [x] Directory structure
- [x] Type system and error handling
- [x] ECIES cipher implementation
- [ ] Authentication handshake
- [ ] Frame cipher
- [ ] Message stream
- [ ] Session management
- [ ] Protocol messages
- [ ] Unit tests
- [ ] Integration tests

## Dependencies

- Boost.Asio (coroutines, networking)
- Boost.Outcome (error handling)
- OpenSSL (AES, HMAC, SHA256)
- libsecp256k1 (ECDH, signatures)
- Microsoft GSL (span)
- RLP library (message encoding)

## References

- [RLPx Transport Protocol](https://github.com/ethereum/devp2p/blob/master/rlpx.md)
- [Silkworm Implementation](https://github.com/erigontech/silkworm/tree/main/silkworm/sentry/rlpx)
