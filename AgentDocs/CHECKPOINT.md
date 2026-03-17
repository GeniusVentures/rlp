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

Follow the **actual go-ethereum discv4 ENR flow** and debug the live path end-to-end.

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

## discv5 Implementation — Sprint Checkpoint (2026-03-16)

### Current implementation state

A parallel `discv5` peer discovery module is present beside the existing `discv4` stack. The current branch reflects the post-merge state with the local build fixes applied.

Most importantly, the current `discv5` code is now aligned with the project's C++17 rule and with the same Boost stackful coroutine style used by `discv4`:

- `cmake/CommonBuildParameters.cmake` sets `CMAKE_CXX_STANDARD 17`
- `src/discv5/CMakeLists.txt` uses `cxx_std_17`
- `src/discv5/CMakeLists.txt` links `Boost::context` and `Boost::coroutine`, matching `src/discv4/CMakeLists.txt`
- `src/discv5/discv5_client.cpp` uses `boost::asio::spawn(...)` and `boost::asio::yield_context`
- No `co_await`, `co_return`, `boost::asio::awaitable`, or `co_spawn` remain in the current `discv5` implementation

This means the earlier native-coroutine description is stale and should not be used as the current mental model for `discv5`.

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
| `examples/discv5_crawl/discv5_crawl.cpp` | C++ live example / functional-harness entry point for discv5 |
| `examples/discv5_crawl/CMakeLists.txt` | Example target wiring |

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

### What is verified today

- ENR URI parsing, base64url decoding, and signature verification are covered by `test/discv5/discv5_enr_test.cpp`
- Per-chain bootnode registry wiring is covered by `test/discv5/discv5_bootnodes_test.cpp`
- Crawler queue / dedup / lifecycle state is covered by `test/discv5/discv5_crawler_test.cpp`
- `examples/CMakeLists.txt` includes `examples/discv5_crawl/`, so the live discv5 example target is part of the examples build
- `examples/discv5_crawl/discv5_crawl.cpp` is the current C++ entry point intended for functional testing of the live discv5 path
- `examples/discovery/test_enr_survey.cpp` is the closest existing example of the intended functional-test shape for a live discovery diagnostic binary

### What is not working yet for functional testing

`examples/discv5_crawl/discv5_crawl.cpp` exists and starts the client, but it is not yet a complete functional discovery test in the way `examples/discovery/test_enr_survey.cpp` is for discv4 diagnostics.

The current gaps, verified from the actual source, are:

1. `src/discv5/discv5_client.cpp::handle_packet()` only logs receipt of packets; it does not yet decode WHOAREYOU, handshake, or NODES messages.
2. `src/discv5/discv5_client.cpp::send_findnode()` currently sends a minimal plaintext FINDNODE datagram, but discv5 needs the real session / handshake path before live peers will treat it as a valid query.
3. `src/discv5/discv5_crawler.cpp::emit_peer()` exists, but the current client receive path does not yet decode incoming peer records and feed them back into the crawler emission path.
4. Because of the above, `examples/discv5_crawl/discv5_crawl.cpp` is currently a live harness / smoke entry point, not yet a full end-to-end functional discovery test.

### Design rules applied

- **M012**: No bare integer literals — every value has a named `constexpr`.
- **M014**: All wire sizes derived from `sizeof(WireStruct)` — see `StaticHeaderWire`, `IPv4Wire`, `IPv6Wire`, etc.
- **M011**: No `if/else` string dispatch — used `switch(ChainId)` and `unordered_map<uint64_t, ChainId>`.
- **M019**: Async flow is written with Boost stackful coroutines (`spawn` + `yield_context`) for C++17 compatibility, matching the project rule and the `discv4` pattern.
- **M018**: `spdlog` via `logger_->info/warn/debug` — no `std::cout`.
- **M015**: All constants inside `namespace discv5`.
- **M017**: Every public declaration has a Doxygen `///` comment.

### Next steps for C++ functional testing

Functional testing for discovery in this repo should follow the same pattern already used by the C++ examples under `examples/`, not shell scripts. The closest working reference is `examples/discovery/test_enr_survey.cpp`.

For `discv5`, the next work should focus on making `examples/discv5_crawl/discv5_crawl.cpp` useful as that same kind of C++ functional test harness.

#### Reference pattern to follow

Use `examples/discovery/test_enr_survey.cpp` as the model:

- it is a standalone C++ example target under `examples/`
- it is wired from the examples CMake tree like the other discovery example binaries
- it drives the live protocol from inside C++
- it collects counters and diagnostic results in memory
- it prints a structured end-of-run report for manual inspection
- it does not depend on shell wrappers to perform the functional test itself

#### Minimal remaining work for `examples/discv5_crawl/discv5_crawl.cpp`

1. Implement the minimal discv5 WHOAREYOU / handshake path needed for live peers to accept the query flow.
2. Decode incoming NODES replies in `src/discv5/discv5_client.cpp`.
3. Convert decoded peer records into `ValidatedPeer` values and feed them into the crawler path.
4. Wire successful peer emission to the existing `PeerDiscoveredCallback` so the example can observe real discoveries.
5. Keep the functional test in C++ under `examples/`.

#### Recommended example-style testing shape

Once the packet path above exists, `examples/discv5_crawl/discv5_crawl.cpp` should behave as a functional survey binary similar in spirit to `examples/discovery/test_enr_survey.cpp`:

- start the `discv5_client`
- seed from `ChainBootnodeRegistry`
- run for a bounded timeout inside `boost::asio::io_context`
- count packets received, peers decoded, peers emitted, and failures/timeouts
- print a final summary from inside C++

That gives the repo a real `discv5` functional test entry point under `examples/` without depending on shell-driven orchestration.

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

---

## discv5 Repair Checkpoint (2026-03-16, current build-blocker)

### Current state

- `src/discv5/discv5_client.cpp` is currently build-broken after a large in-progress edit.
- The file contains literal patch markers (`+`) in source around `parse_handshake_auth(...)` and around `handshake_packet_count()/nodes_packet_count()`.
- There are container type mismatches in `make_local_enr_record(...)` where `RlpEncoder::MoveBytes()` values (`rlp::Bytes`) are assigned/returned as `std::vector<uint8_t>` without conversion.
- The failure is localized to `src/discv5/discv5_client.cpp.o`; this must be repaired in place with tiny edits only.

### Known compiler errors (from latest failed build)

- `no viable conversion from 'std::basic_string<unsigned char>' to 'std::vector<uint8_t>'` (around lines ~767 and ~821)
- `expected expression` and `expected external declaration` caused by stray `+` markers (around lines ~997, ~1011, ~1154)

### Required repair approach (next chat)

1. Edit `src/discv5/discv5_client.cpp` in place; do not rewrite or replace the file.
2. Remove only stray literal diff markers and duplicate fragment residue.
3. Fix only the `rlp::Bytes`/`std::vector<uint8_t>` boundaries with explicit conversions.
4. Rebuild immediately after each small fix until `src/discv5/discv5_client.cpp.o` compiles.
5. After compile recovers, run current discv5 tests:
   - `test/discv5/discv5_enr_test`
   - `test/discv5/discv5_bootnodes_test`
   - `test/discv5/discv5_crawler_test`
   - `test/discv5/discv5_client_test`

### Scope guard

- No refactor, rename, architecture changes, or broad cleanup.
- Keep behavior unchanged except what is required to restore compile/test health.

---

## discv5 Functional Checkpoint (2026-03-16, post-repair)

### Current state

- The previous `src/discv5/discv5_client.cpp` build breakage is resolved.
- `discv5_client` and `discv5_crawl` now build and run in `build/OSX/Debug`.
- `test/discv5/discv5_client_test` is green after the in-place repairs and test expectation alignment.
- The `discv5_crawl` live harness now reaches callback peer emissions from decoded `NODES` responses.

### Key technical fix that unlocked live discovery

- Outbound encrypted packet construction had an AAD/header mismatch bug:
  - code previously encoded a header to produce AAD,
  - encrypted against that AAD,
  - then re-encoded a new header before send.
- This was fixed by appending ciphertext to the originally encoded header packet (same AAD/header bytes), without re-encoding.
- The fix was applied in:
  - session `FINDNODE` message send path,
  - handshake send path,
  - `NODES` response send path.

### Additional parity/diagnostic updates applied

- `discv5` target links `rlpx` (required for `rlpx::crypto::Ecdh::generate_ephemeral_keypair()`).
- `WHOAREYOU` `record_seq` is now sent as `0`.
- Handshake ENR attachment is conditional on remote `record_seq` state.
- `discv5_crawl` now initializes local discv5 keypair (`cfg.private_key` / `cfg.public_key`) before start.
- Detailed handshake/message diagnostics are now gated behind `--log-level trace`.
- Per-peer discovery callback logs in `discv5_crawl` are reduced from `info` to `debug`.

### Latest observed functional outcome

- `discv5_crawl --chain ethereum --timeout 3 --log-level info` shows:
  - non-zero `callback discoveries`,
  - non-zero `nodes packets`,
  - `run status: callback_emissions_seen`.

This confirms the current discv5 harness performs real live discovery and peer emission.

### Next steps

1. Add/extend a Sepolia functional connect harness that uses `discv5` discovery and proves at least 3 right-chain connections.
2. Keep `eth_watch` unchanged until the Sepolia connect milestone is stable.
3. After that, add an opt-in discv5 discovery mode to `examples/eth_watch/eth_watch.cpp` and validate event flow with a sent transaction.

---

## discv5 Sepolia Connect Checkpoint (2026-03-17)

### Commands run and observed outcomes

1. Pure callback mode, no fork filter:

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
ninja test_discv5_connect
./examples/discovery/test_discv5_connect --timeout 20 --connections 1 --log-level debug --seeded off --require-fork off --enqueue-bootstrap-candidates off
```

Observed result:

- `dialed: 73`
- `connect failed: 69`
- `connected (discv5): 0`
- `filtered bad peers: 11`
- `candidates seen: 84`
- `discovered peers: 73`

The failures were almost entirely pre-ETH and happened during RLPx auth / ack reception:

- `read_exact(ack length prefix) failed`
- `ack length 4911 exceeds EIP-8 max 2048`

2. Fork-filtered mode:

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
ninja test_discv5_connect
./examples/discovery/test_discv5_connect --timeout 20 --connections 1 --log-level debug --seeded off --require-fork on --enqueue-bootstrap-candidates off
```

Observed result:

- `dialed: 0`
- `connect failed: 0`
- `candidates seen: 0`
- `discovered peers: 0`

This confirms the `--require-fork on` failure is upstream of dialing.

### Verified Sepolia fork-hash status

- `examples/chains.json` contains `"sepolia": "268956b6"`.
- `examples/chain_config.hpp` loads this value from `chains.json` and falls back only if the file/key is missing.
- `examples/discovery/test_discv5_connect.cpp` uses fallback `{ 0x26, 0x89, 0x56, 0xb6 }`.
- `AgentDocs/SEPOLIA_TEST_PARAMS.md` documents current Sepolia fork hash as `26 89 56 b6`.

Live confirmation from the existing ENR survey harness:

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
ninja test_enr_survey
./examples/discovery/test_enr_survey --timeout 20 --log-level info
```

Observed result:

- `Peers WITH eth_fork_id: 522`
- Sepolia expected hash `26 89 56 b6` was present in live ENR data

Conclusion: the current Sepolia fork hash used by the harness is correct.

### Current discv5-specific failure identified

The current `discv5` path is discovering peers, but the discovered callback path is not surfacing `eth_fork_id`.

Verified with:

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
./examples/discv5_crawl/discv5_crawl --chain sepolia --timeout 20 --log-level debug
```

Observed result:

- `callback discoveries   : 84`
- `discovered  : 84`
- `wrong_chain : 0`
- `no_eth_entry: 0`
- every debug discovery line printed `eth_fork=no`

Example lines from the run:

- `Discovered peer 1  150.241.96.23:9222  eth_fork=no`
- `Discovered peer 2  65.21.79.59:13000  eth_fork=no`
- `Discovered peer 53  138.68.123.152:30303  eth_fork=no`

This means:

1. the current Sepolia fork hash is not the reason `--require-fork on` yields zero peers,
2. the current `discv5` connect path is effectively filtering on missing fork metadata,
3. the next bug is why the current `discv5` discovery path produces peers with no `eth_fork_id`, even though the discv4 ENR survey proves Sepolia `eth` entries do exist live.

### devp2p cross-checks from failing tuples

The exact failing tuples from `test_discv5_connect` were checked with workspace `go-ethereum` `devp2p rlpx ping`.

Observed examples:

- `65.21.79.59:13000` → `message too big`
- `185.159.108.216:4001` → `message too big`
- `150.241.96.23:9222` → `connection reset by peer`

This confirms that at least some pure-mode failing tuples are genuinely bad RLPx targets as dialed, not uniquely rejected by the local client.

### Next step for the next chat

Do not chase the Sepolia fork hash any further.

Focus on the actual current gap:

1. trace why the `discv5` discovered peers all show `eth_fork=no`,
2. determine whether the incoming discv5 ENRs truly lack the `eth` entry or whether `discv5` ENR decoding is not surfacing it,
3. only after that, revisit fork-filtered dialing.

