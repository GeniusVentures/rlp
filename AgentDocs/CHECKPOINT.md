# Checkpoint — 2026-03-14

## Current Status
- The **ENRRequest / ENRResponse** wire path is implemented and unit-tested.
- The live discovery path now does `bond -> request_enr -> ParseEthForkId -> set DiscoveredPeer.eth_fork_id`.
- `DialScheduler::filter_fn` and `make_fork_id_filter()` are implemented.
- `examples/discovery/test_discovery.cpp` is already wired to use the ENR pre-dial filter.
- The latest live Sepolia run with that filter enabled produced:
  - `discovered peers: 24733`
  - `dialed: 0`
  - `connected (right chain): 0`

### Current interpretation
1. The ENR wire/unit work is done.
2. The live failure has moved: it is no longer “missing ENR filter hookup”.
3. The immediate problem is now **why no usable `eth_fork_id` reaches the filter in the live path**.
4. The next step is to follow **go-ethereum’s real discv4 ENR flow** and debug the live request/response sequence,
   not to add more architecture or broad refactors.

---

## Files Most Relevant To The Next Step

| File | Why it matters now |
|---|---|
| `examples/discovery/test_discovery.cpp` | Live Sepolia harness; now sets `scheduler->filter_fn` |
| `src/discv4/discv4_client.cpp` | Current bond -> ENR -> callback flow |
| `include/discv4/discv4_client.hpp` | `DiscoveredPeer.eth_fork_id`, `request_enr()` API |
| `include/discv4/dial_scheduler.hpp` | `FilterFn`, `filter_fn`, `make_fork_id_filter()` |
| `go-ethereum/p2p/discover/v4_udp.go` | Reference flow for `RequestENR`, `ensureBond`, `Resolve` |
| `go-ethereum/eth/protocols/eth/discovery.go` | Reference `NewNodeFilter` logic |
| `test/discv4/enr_client_test.cpp` | Loopback request/reply coverage |
| `test/discv4/enr_enrichment_test.cpp` | ENR enrichment coverage |
| `test/discv4/dial_filter_test.cpp` | Pre-dial filtering coverage |

---

## What Is Done

### ENR wire support
- `include/discv4/discv4_constants.hpp`
  - Added `kPacketTypeEnrRequest = 0x05`
  - Added `kPacketTypeEnrResponse = 0x06`
- `include/discv4/discv4_enr_request.hpp`
- `src/discv4/discv4_enr_request.cpp`
  - Minimal `ENRRequest` modeled after go-ethereum `v4wire.ENRRequest{Expiration uint64}`
- `include/discv4/discv4_enr_response.hpp`
- `src/discv4/discv4_enr_response.cpp`
  - Minimal `ENRResponse` modeled after go-ethereum `v4wire.ENRResponse{ReplyTok, Record}`
  - `ParseEthForkId()` decodes the ENR `eth` entry into a `ForkId`

### discv4 client flow
- `include/discv4/discv4_client.hpp`
  - Added `request_enr()`
  - Extended `PendingReply`
  - Added `std::optional<ForkId> eth_fork_id` to `DiscoveredPeer`
- `src/discv4/discv4_client.cpp`
  - Dispatches packet types 5 and 6
  - Implements `handle_enr_response()` reply matching via `ReplyTok`
  - In `handle_neighbours()`, the callback path now enriches peers with ENR-derived `eth_fork_id`

### Pre-dial filter
- `include/discv4/dial_scheduler.hpp`
  - Added `FilterFn`
  - Added `DialScheduler::filter_fn`
  - Added `make_fork_id_filter()`
  - `enqueue()` now drops peers that fail the filter before consuming a dial slot
- `examples/discovery/test_discovery.cpp`
  - The scheduler is now configured with the Sepolia ENR filter before enqueueing peers

---

## Verified Tests

The following tests were built and run successfully during this work:

- `./test/discv4/discv4_enr_request_test`
- `./test/discv4/discv4_enr_response_test`
- `./test/discv4/discv4_enr_client_test`
- `./test/discv4/discv4_enr_enrichment_test`
- `./test/discv4/discv4_dial_filter_test`
- `./test/discv4/discv4_client_test`
- `./test/discv4/discv4_dial_scheduler_test`

---

## Latest Live Result

Command run:

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
./examples/discovery/test_discovery --log-level warn --timeout 60
```

Observed result:

- `24733 neighbour peer(s) discovered`
- `dialed: 0`
- `connect_failed: 0`
- `wrong_chain: 0`
- `status_timeout: 0`
- `connected (right chain): 0`

### Meaning of that result
- Discovery itself is active.
- The pre-dial filter is now blocking every candidate before any dial starts.
- Therefore the next bug is **not** “hook up the filter”.
- The next bug is one of these:
  1. live `request_enr()` is not successfully completing for real peers,
  2. live ENR responses are not being parsed into `eth_fork_id`,
  3. the live ENR `eth` entry is absent for most peers,
  4. the Sepolia fork-hash assumption used by the filter is wrong for live ENR data,
  5. the current sequencing differs from go-ethereum in a way that prevents usable ENR data from reaching the callback.

---

## Immediate Next Step

Follow the **actual go-ethereum discv4 ENR flow** and debug the live path end-to-end.

### Reference files
- `go-ethereum/p2p/discover/v4_udp.go`
  - `RequestENR`
  - `ensureBond`
  - `Resolve`
- `go-ethereum/eth/protocols/eth/discovery.go`
  - `NewNodeFilter`

### What the next chat should do
1. Compare `src/discv4/discv4_client.cpp` against go-ethereum’s `RequestENR` flow.
2. Trace the live path to determine why `DiscoveredPeer.eth_fork_id` is not usable before filtering.
3. Verify whether ENR requests are actually sent and matched for live peers.
4. Verify whether live ENR responses contain an `eth` entry and what fork hash they advertise.
5. Only after that, adjust the live filter/hash or sequencing with the smallest possible change.

---

## New Chat Handoff Prompt

Use this to start the next chat:

```text
We already completed the ENRRequest/ENRResponse implementation in the rlp project.

What is already done:
- ENRRequest / ENRResponse wire support is implemented and unit-tested.
- discv4_client now does bond -> request_enr -> ParseEthForkId -> set DiscoveredPeer.eth_fork_id.
- DialScheduler::filter_fn and make_fork_id_filter() are implemented.
- examples/discovery/test_discovery.cpp is already wired to use the ENR pre-dial filter.

Latest live result:
- ./examples/discovery/test_discovery --log-level warn --timeout 60
- discovered peers: 24733
- dialed: 0
- connected (right chain): 0

So the current bug is no longer "missing filter hookup". The filter is rejecting everything because no usable eth_fork_id is reaching the live dial path.

Please compare our current live ENR flow against go-ethereum’s actual flow in:
- go-ethereum/p2p/discover/v4_udp.go
- go-ethereum/eth/protocols/eth/discovery.go

Focus only on the minimal next step: find why no usable eth_fork_id reaches the filter in the live path, and fix that with the smallest possible change.

Relevant project files:
- AgentDocs/CHECKPOINT.md
- examples/discovery/test_discovery.cpp
- src/discv4/discv4_client.cpp
- include/discv4/discv4_client.hpp
- include/discv4/dial_scheduler.hpp
- test/discv4/enr_client_test.cpp
- test/discv4/enr_enrichment_test.cpp
- test/discv4/dial_filter_test.cpp
```

---

## Quick Commands For The Next Chat

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
ninja

./test/discv4/discv4_enr_request_test
./test/discv4/discv4_enr_response_test
./test/discv4/discv4_enr_client_test
./test/discv4/discv4_enr_enrichment_test
./test/discv4/discv4_dial_filter_test
./test/discv4/discv4_client_test
./test/discv4/discv4_dial_scheduler_test

./examples/discovery/test_discovery --log-level warn --timeout 60
./examples/discovery/test_discovery --log-level debug --timeout 60
```


---

## discv5 Implementation — Sprint Checkpoint (2026-03-15)

### What was built

A complete parallel `discv5` peer discovery module was added beside the existing `discv4` stack.  All code lives in new directories; no existing discv4 code was modified.

#### New files

| Path | Purpose |
|---|---|
| `include/discovery/discovered_peer.hpp` | Shared `NodeId`, `ForkId`, `ValidatedPeer` handoff contract (used by both discv4 and discv5) |
| `include/discv5/discv5_constants.hpp` | All domain constants + wire POD structs with `sizeof()`-derived sizes |
| `include/discv5/discv5_error.hpp` | `discv5Error` enum + `to_string()` declaration |
| `include/discv5/discv5_types.hpp` | `EnrRecord`, `Discv5Peer`, `discv5Config`, callback aliases |
| `include/discv5/discv5_enr.hpp` | `EnrParser` – decode/verify ENR URIs |
| `include/discv5/discv5_bootnodes.hpp` | `IBootnodeSource`, `StaticEnrBootnodeSource`, `StaticEnodeBootnodeSource`, `ChainBootnodeRegistry` |
| `include/discv5/discv5_crawler.hpp` | `discv5_crawler` – queued/measured/failed/discovered peer sets |
| `include/discv5/discv5_client.hpp` | `discv5_client` – UDP socket + receive loop + crawler loop |
| `src/discv5/*.cpp` | Implementation files (error, enr, bootnodes, crawler, client) |
| `src/discv5/CMakeLists.txt` | `discv5` static library |
| `test/discv5/discv5_enr_test.cpp` | ENR parser tests using real go-ethereum test vectors |
| `test/discv5/discv5_bootnodes_test.cpp` | Bootnode source and chain registry tests |
| `test/discv5/discv5_crawler_test.cpp` | Deterministic crawler state machine tests |
| `test/discv5/CMakeLists.txt` | Test executables |

#### Supported chains (bootnode registry)

- Ethereum mainnet (ENR from go-ethereum V5Bootnodes) / Sepolia / Holesky
- Polygon mainnet / Amoy testnet
- BSC mainnet / testnet
- Base mainnet / Base Sepolia (OP Stack — seed list populated at runtime)

#### Architecture

```
BootnodeSource / ENR URI
       │
       ▼
discv5_crawler (queued → FINDNODE → discovered)
       │
       │  PeerDiscoveredCallback
       ▼
ValidatedPeer (= discovery::ValidatedPeer)
       │
       ▼
existing DialScheduler / RLPx path (unchanged)
```

### Design rules applied

- **M012**: No bare integer literals — every value has a named `constexpr`.
- **M014**: All wire sizes derived from `sizeof(WireStruct)` — see `StaticHeaderWire`, `IPv4Wire`, `IPv6Wire`, etc.
- **M011**: No `if/else` string dispatch — used `switch(ChainId)` and `unordered_map<uint64_t, ChainId>`.
- **M013**: `co_spawn(io, []() -> awaitable<void> { … }, detached)` wrapping pattern.
- **M018**: `spdlog` via `logger_->info/warn/debug` — no `std::cout`.
- **M015**: All constants inside `namespace discv5`.
- **M017**: Every public declaration has a Doxygen `///` comment.

### Next steps

1. Implement full WHOAREYOU/HANDSHAKE session layer (AES-GCM key derivation) for encrypted message exchange.
2. Decode incoming NODES responses and feed peers back into the crawler queue.
3. Add `examples/discv5_crawl/` live example binary (see task list below).
4. Wire `discv5_client` into `eth_watch` as an alternative to `discv4_client`.

### go-ethereum reference used

```
/tmp/go-ethereum/   (shallow clone for this session)
```

Key files read:
- `p2p/enr/enr.go` — ENR record structure and signature scheme
- `p2p/enode/idscheme.go` — V4ID sign/verify, NodeAddr derivation
- `p2p/enode/node_test.go` — TestPythonInterop and parseNodeTests (test vectors)
- `p2p/enode/urlv4_test.go` — Valid/invalid ENR URI test vectors
- `p2p/discover/v5wire/msg.go` — FINDNODE / NODES message types
- `p2p/discover/v5wire/encoding.go` — StaticHeader wire layout
- `params/bootnodes.go` — Real ENR/enode bootnode strings
