# Architecture — GNUS.AI ETH P2P Event Watching

**Date**: March 6, 2026
**Status**: MVP Complete

---

## Overview

This library implements a C++20 light client for monitoring EVM smart contract events
on Ethereum-compatible chains (Ethereum, Polygon, BSC, Base) using the native P2P
DevP2P stack — no JSON-RPC, no WebSockets, no centralized intermediary.

The core flow:

```
discv4 (UDP)          → find peers
RLPx (TCP/ECIES)      → encrypted session
eth/66+ subprotocol   → NewBlockHashes → GetReceipts → Receipts
EthWatchService       → EventFilter → ABI decode → typed callback
```

---

## Layer Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│   eth_watch CLI  ·  EthWatchService  ·  DecodedEventCallback│
├─────────────────────────────────────────────────────────────┤
│                  ETH Protocol Layer (eth/66+)                │
│   messages.hpp  ·  objects.hpp  ·  abi_decoder.hpp          │
│   event_filter.hpp  ·  chain_tracker.hpp                    │
├─────────────────────────────────────────────────────────────┤
│                  RLPx Transport Layer                        │
│   rlpx_session  ·  frame_cipher  ·  auth_handshake          │
│   ECIES  ·  ECDH  ·  AES-256-CTR  ·  Boost.Asio             │
├─────────────────────────────────────────────────────────────┤
│                  Discovery Layer                             │
│   discv4_client  ·  PING/PONG  ·  bootnodes                 │
├─────────────────────────────────────────────────────────────┤
│                  RLP Codec Layer                             │
│   rlp_encoder  ·  rlp_decoder  ·  rlp_streaming             │
│   endian  ·  intx::uint256                                   │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Reference

### RLP Codec (`include/rlp/`, `src/rlp/`)

Encodes and decodes all Ethereum wire format data.

```cpp
// Encoding
rlp::RlpEncoder enc;
enc.append(uint64_t{100});
enc.append(some_address);
auto bytes = enc.finish();

// Decoding
auto result = rlp::decode_uint64(view);   // Result<uint64_t>
auto header = eth::codec::decode_block_header(view);
```

Key types:
- `rlp::ByteView` — non-owning view into a byte buffer
- `rlp::Result<T>` — `boost::outcome` result type
- `rlp::Hash256`, `rlp::Address`, `rlp::Bloom` — fixed-size byte arrays
- `intx::uint256` — 256-bit unsigned integer (project-local, `include/rlp/intx.hpp`)

---

### RLPx Session (`include/rlpx/`, `src/rlpx/`)

Encrypted P2P session over TCP. Handles the ECIES handshake, frame cipher, and
protocol message dispatch. Uses Boost.Asio coroutines.

```cpp
// Connecting
rlpx::SessionConnectParams params{
    .remote_host   = "1.2.3.4",
    .remote_port   = 30303,
    .peer_public_key = peer_pubkey,
};
auto session = co_await rlpx::RlpxSession::connect(params);

// Registering handlers
session->set_hello_handler([](const rlpx::protocol::HelloMessage& msg) { ... });
session->set_generic_handler([](const rlpx::framing::Message& msg) { ... });

// Sending a message
session->post_message(rlpx::framing::Message{.id = offset + eth_id, .payload = bytes});
```

---

### Discovery v4 (`include/discv4/`, `src/discv4/`)

UDP-based peer discovery using the discv4 protocol. Finds peers on the target chain
and supplies their enode addresses for RLPx connection.

```cpp
discv4::Discv4Client client(io_context, private_key);
client.set_peer_discovered_callback([](const discv4::NodeRecord& node) {
    // Connect via RLPx to node.endpoint
});
client.start(discv4::get_sepolia_bootnodes());
```

**Important**: Bootstrap nodes are for discovery only. They do NOT send block data.
The discovered peers (full/archive nodes) are what provide `NewBlockHashes`.

---

### ETH Protocol Messages (`include/eth/messages.hpp`, `include/eth/objects.hpp`)

All eth/66+ messages are implemented with full encode/decode and the
`request_id` envelope required by eth/66+.

```cpp
// Request receipts for a block
eth::GetReceiptsMessage req{.request_id = 1, .block_hashes = {hash}};
auto encoded = eth::protocol::encode_get_receipts(req);

// Decode incoming receipts
auto msg = eth::protocol::decode_receipts(payload);
// msg.request_id correlates back to the request
// msg.receipts[i] = vector of Receipt for block i
```

Transaction types fully supported: legacy, EIP-2930, EIP-1559 (EIP-2718 wire format).

---

### Event Filtering (`include/eth/event_filter.hpp`)

Follows `eth_getLogs` semantics. Each `EventFilter` specifies:
- `addresses` — contract addresses to watch (empty = any contract)
- `topics` — per-position constraints (`nullopt` = wildcard)
- `from_block` / `to_block` — optional block range

```cpp
eth::EventFilter filter;
filter.addresses.push_back(contract_address);
filter.topics.push_back(eth::abi::event_signature_hash("Transfer(address,address,uint256)"));

eth::EventWatcher watcher;
auto id = watcher.watch(filter, [](const eth::MatchedEvent& ev) { ... });
watcher.process_receipt(receipt, tx_hash, block_number, block_hash);
watcher.unwatch(id);
```

---

### ABI Decoder (`include/eth/abi_decoder.hpp`)

Decodes EVM ABI-encoded event parameters from log topics and data fields.

```cpp
// Compute topic[0] for an event signature
auto sig_hash = eth::abi::event_signature_hash("Transfer(address,address,uint256)");

// Describe the event parameters
std::vector<eth::abi::AbiParam> params = {
    {eth::abi::AbiParamKind::kAddress, true,  "from"},   // indexed
    {eth::abi::AbiParamKind::kAddress, true,  "to"},     // indexed
    {eth::abi::AbiParamKind::kUint,    false, "value"},  // non-indexed (in data)
};

// Decode a log entry
auto result = eth::abi::decode_log(log, "Transfer(address,address,uint256)", params);
// result->at(0) = Address (from)
// result->at(1) = Address (to)
// result->at(2) = intx::uint256 (value)
```

Supported types: `address`, `uint256`, `bytes32`, `bool`, `bytes`, `string`.

---

### EthWatchService (`include/eth/eth_watch_service.hpp`)

The main integration point. Ties together the event filter, ABI decoder, chain
tracker, and outbound send callback into a single object.

```cpp
eth::EthWatchService svc;

// Wire outbound messages back to the RLPx session
svc.set_send_callback([&session, eth_offset](uint8_t eth_id, std::vector<uint8_t> payload) {
    session->post_message({.id = eth_offset + eth_id, .payload = std::move(payload)});
});

// Register a watch
auto id = svc.watch_event(
    contract_address,
    "Transfer(address,address,uint256)",
    params,
    [](const eth::MatchedEvent& ev, const std::vector<eth::abi::AbiValue>& vals) {
        const auto& from  = std::get<eth::codec::Address>(vals[0]);
        const auto& to    = std::get<eth::codec::Address>(vals[1]);
        const auto& value = std::get<intx::uint256>(vals[2]);
        // ... process event
    });

// Feed incoming eth wire messages (from set_generic_handler)
svc.process_message(eth_id, payload);
// Automatically emits GetReceipts for new blocks,
// correlates Receipts responses, fires callbacks.

// Query chain tip
uint64_t tip_number = svc.tip();
```

---

### ChainTracker (`include/eth/chain_tracker.hpp`)

Prevents duplicate `GetReceipts` requests for the same block. Maintains a
sliding window of seen block hashes (default 1024).

```cpp
eth::ChainTracker tracker;              // default window = 1024

if (tracker.mark_seen(block_hash, block_number)) {
    // First time seeing this block — request receipts
} else {
    // Already seen — skip
}

uint64_t tip = tracker.tip();
auto     tip_hash = tracker.tip_hash(); // optional<Hash256>
```

`EthWatchService` owns a `ChainTracker` internally and consults it automatically
inside `request_receipts`. Callers do not need to manage it directly.

---

### CLI Helpers (`include/eth/eth_watch_cli.hpp`)

Header-only utilities for command-line argument parsing.

```cpp
// Parse an Ethereum address (with or without 0x prefix)
auto addr = eth::cli::parse_address("0x9af8050220D8C355CA3c6dC00a78B474cd3e3c70");

// Get standard ABI params for well-known event signatures
auto params = eth::cli::infer_params("Transfer(address,address,uint256)");
// Returns the 3-param Transfer list; empty for unknown signatures

// A watch specification from the command line
eth::cli::WatchSpec spec{"0x9af805...", "Transfer(address,address,uint256)"};
```

---

## Data Flow: NewBlockHashes → Decoded Event Callback

```
Peer sends NewBlockHashes([{hash1, 100}, {hash2, 101}])
    │
    ▼
eth_watch generic_handler receives msg (id = offset + 0x01)
    │
    ▼
EthWatchService::process_message(kNewBlockHashesMessageId, payload)
    │
    ├─ decode_new_block_hashes(payload)
    │
    └─ for each entry:
           ChainTracker::mark_seen(hash, number)
               │ first time? yes → continue; no → skip
               ▼
           GetReceiptsMessage{request_id=N, block_hashes=[hash]}
               │
               ▼
           encode_get_receipts → send_cb_(kGetReceiptsMessageId, bytes)
               │
               ▼
           pending_requests_[N] = {hash, number}

Peer sends Receipts(request_id=N, [[receipt0, receipt1, ...]])
    │
    ▼
EthWatchService::process_message(kReceiptsMessageId, payload)
    │
    ├─ decode_receipts(payload)
    │
    ├─ look up pending_requests_[N] → {block_hash, block_number}
    │
    └─ process_receipts(block_receipts, tx_hashes, block_number, block_hash)
           │
           └─ EventWatcher::process_receipt(receipt, tx_hash, block_number, block_hash)
                  │
                  └─ for each log in receipt:
                         EventFilter::matches(log, block_number)?
                             │ yes
                             ▼
                         ABI decode → DecodedEventCallback(ev, vals)
```

---

## GNUS.AI Contract Addresses

All addresses are registered and tested in `test/eth/gnus_contracts_test.cpp`.

### Production (Mainnet)

| Chain | Address |
|---|---|
| Ethereum | `0x614577036F0a024DBC1C88BA616b394DD65d105a` |
| Polygon | `0x127E47abA094a9a87D084a3a93732909Ff031419` |
| BSC | `0x614577036F0a024DBC1C88BA616b394DD65d105a` |
| Base | `0x614577036F0a024DBC1C88BA616b394DD65d105a` |

### Testnet

| Chain | Address |
|---|---|
| Sepolia | `0x9af8050220D8C355CA3c6dC00a78B474cd3e3c70` |
| Polygon Amoy | `0xeC20bDf2f9f77dc37Ee8313f719A3cbCFA0CD1eB` |
| BSC Testnet | `0xeC20bDf2f9f77dc37Ee8313f719A3cbCFA0CD1eB` |
| Base Testnet | `0xeC20bDf2f9f77dc37Ee8313f719A3cbCFA0CD1eB` |

---

## Known Limitations

See `CHECKPOINT.md` — Known Limitations section for the full list.
Key items: bootstrap nodes are discovery-only; no Bloom filter pre-screening;
no historical backfill; single peer connection.

---

## Build & Test

```bash
cd build/OSX/Debug
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
ninja
ctest   # 441/441 should pass
```
