# RLP (Recursive Length Prefix) Library


This is a C++ library for encoding and decoding data in the Recursive Length Prefix (RLP) format, as used in Ethereum and other blockchain systems. It provides a robust, type-safe implementation with support for basic types (`Bytes`, `uint64_t`, `uint256`, `bool`), lists, and convenience methods for vectors and fixed-size arrays.

## Features

- **Encoding**: Supports RLP encoding of raw bytes, unsigned integers (`uint8_t` to `uint256`), booleans, and nested lists.
- **Decoding**: Decodes RLP data into corresponding C++ types, with error handling for malformed inputs.
- **Type Safety**: Uses C++17 SFINAE and templates to ensure correct type handling.
- **Cross-Platform**: Build instructions provided for multiple platforms using Ninja and CMake.

## Dependencies

- **C++17**: Required for template features and type traits.
- **Boost.Outcome**: Used for error handling (`BOOST_OUTCOME_TRY`).
- **Google Test**: For unit tests (`gtest`).
- **intx**: Extended precision integer library for `uint256` support.

## Project Structure

- `src/`: Source files (`rlp_encoder.cpp`, `rlp_decoder.cpp`, etc.).
- `include/`: Header files (`rlp_encoder.hpp`, `rlp_decoder.hpp`, etc.).
- `test/`: Unit tests (`rlp_test.cpp`).
- `build/`: Platform-specific build directories (e.g., `build/OSX/`, `build/Linux/`).

## Building the Project

This project uses Ninja as the build system, with CMake for configuration. Builds are organized in platform-specific directories under `build/`, such as `build/OSX/` for macOS, `build/Linux/` for Linux, etc. Each platform directory contains subdirectories for build configurations (`Debug`, `Release`, `RelWithDebInfo`).

### Prerequisites

- Install CMake (`cmake`).
- Install Ninja (`ninja`).
- Ensure C++17-compatible compiler (e.g., `g++`, `clang++`).
- Install dependencies:
  - Boost (`boost-outcome` or full Boost suite).
  - Google Test (`libgtest-dev` on Ubuntu, or build from source).
  - intx (include as a submodule or install separately).

### Build Instructions

For each platform, follow these steps. Example shown for macOS (`OSX`):

#### Debug Build
```bash
cd build/OSX
mkdir -p Debug
cd Debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -G "Ninja" -DSANITIZE_ADDRESS=code
ninja
```

#### Release Build
```bash
cd build/OSX
mkdir -p Release
cd Release
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Ninja"
ninja
```

#### Release with Debug Info
```bash
cd build/OSX
mkdir -p RelWithDebInfo
cd RelWithDebInfo
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -G "Ninja" -DSANITIZE_ADDRESS=code
ninja
```

The ```-DSANITIZE_ADDRESS=code``` is optional for finding memory leaks and other memory issues

- **Output**: Built files (e.g., test executable `rlp_test`) are in `build/OSX/Debug/`, `build/OSX/Release/`, etc.
- **Platforms**: Replace `OSX` with `Linux`, `Windows`, `Android`, or `iOS` as needed. Adjust CMake flags for cross-compilation if targeting Android or iOS.

### Running Tests
After building, run the test executable:
```bash
cd build/OSX/Debug
./rlp_test
```

## Usage

### Encoding Example
```cpp
#include <rlp_encoder.hpp>

int main() {
    rlp::RlpEncoder encoder;
    encoder.begin_list();
    encoder.add(42); // uint64_t
    encoder.add(true); // bool
    encoder.end_list();
    rlp::Bytes encoded = encoder.move_bytes();
    // encoded contains RLP-encoded list [42, true]
    return 0;
}
```

### Decoding Example
```cpp
#include <rlp_decoder.hpp>

int main() {
    rlp::Bytes data = /* some RLP data */;
    rlp::RlpDecoder decoder(data);
    auto list_payload_bytes = decoder.read_list_header_bytes(); // Returns byte count, not item count
    if (list_payload_bytes) {
        uint64_t num;
        bool flag;
        decoder.read(num);
        decoder.read(flag);
        // Use num and flag
    }
    return 0;
}
```

## Light-Client Monitoring Stack

The repository now contains the core building blocks required by the
[architecture design](Architecture.md) for running a SuperGenius light client that
monitors contract activity directly from EVM P2P networks. The
implementation spans three cooperating layers that map directly to the
design’s stages:

### 1. Peer Discovery (Discv4)
- Located under `include/rlp/PeerDiscovery/` and `src/rlp/PeerDiscovery/`, the
  discovery module RLP-encodes/decodes v4 packets, manages bootstrap node
  metadata, and orchestrates UDP flows via Boost.Asio, mirroring the design’s
  discovery responsibilities.
- Unit coverage in `test/rlp/discovery_test.cpp` exercises PING/PONG
  construction, parsing, and timeout handling to validate the discovery flow
  end-to-end.

### 2. RLPx Transport and Session Management
- The `rlpx` subsystem (`include/rlpx/` and `src/rlpx/`) implements the secure
  TCP transport, including ECIES handshakes, AES/HMAC frame ciphers, message
  framing, and coroutine-based session orchestration—directly covering the
  RLPx and `eth` subprotocol requirements from the design.
- High-level session APIs expose message handlers for HELLO, STATUS, PING, and
  generic gossip, enabling the transaction/log filtering pipeline described in
  the design document.

### 3. Ethereum RLP Processing and Filtering Primitives
- Core encoding/decoding utilities in `include/rlp/` and `src/rlp/` provide the
  recursive RLP handling needed to parse block headers, transactions, and
  receipts once they are received over the wire, satisfying the design’s
  execution-layer processing goals.
- `include/rlp/rlp_ethereum.hpp` supplies Ethereum-specific helpers (e.g., for
  block header fields and bloom filters) that align with the consensus
  verification and log filtering steps laid out in the design.

These components can be composed inside SuperGenius to establish DevP2P
connections, validate chain consistency from checkpoints, and stream contract
events without centralized RPC endpoints.

## Testing

The project includes comprehensive unit tests in `test/rlp_test.cpp`, covering:
- Encoding/decoding of basic types.
- Nested lists and vectors.
- Error cases (e.g., leading zeros, overflow, non-canonical sizes).

Run tests after building to verify functionality.

## Contributing

- Fork the repository.
- Create a feature branch.
- Submit a pull request with your changes and updated tests.

## License

This project is licensed under the MIT License. See [LICENSE.txt](LICENSE.txt) for details.

