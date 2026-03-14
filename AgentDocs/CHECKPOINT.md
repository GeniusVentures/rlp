# Checkpoint — 2026-03-14

## Current Status
- The **ENRRequest / ENRResponse** wire path and chain pre-filter are fully implemented and unit-tested.
- The bond → ENR → `ForkId` enrichment is wired into `discv4_client::handle_neighbours`.
- `DialScheduler::filter_fn` is implemented and `make_fork_id_filter()` is provided.
- **`test_discovery.cpp` has NOT yet been updated** to hook the ENR filter into the live Sepolia run.
  That is the immediate next step.

---

## Currently Staged Files

| Status | File | Staged purpose |
|---|---|---|
| `M` | `AgentDocs/CHECKPOINT.md` | This checkpoint update |
| `M` | `include/discv4/discv4_constants.hpp` | Added `kPacketTypeEnrRequest=0x05`, `kPacketTypeEnrResponse=0x06` |
| `A` | `include/discv4/discv4_enr_request.hpp` | ENRRequest struct + `RlpPayload()` (mirrors go-ethereum `v4wire.ENRRequest`) |
| `A` | `include/discv4/discv4_enr_response.hpp` | `ForkId` struct + `discv4_enr_response` with `Parse()` and `ParseEthForkId()` |
| `A` | `src/discv4/discv4_enr_request.cpp` | ENRRequest RLP encode implementation |
| `A` | `src/discv4/discv4_enr_response.cpp` | ENRResponse wire parse + ENR record `eth` key-value walk |
| `M` | `src/discv4/CMakeLists.txt` | Added `discv4_enr_request.cpp`, `discv4_enr_response.cpp` |
| `M` | `include/discv4/discv4_client.hpp` | Added `request_enr()`, `handle_enr_request/response()`, extended `PendingReply`, `bound_port()`, `std::optional<ForkId> eth_fork_id` on `DiscoveredPeer` |
| `M` | `src/discv4/discv4_client.cpp` | Dispatch types 5/6, `handle_enr_request` (silent drop), `handle_enr_response` (ReplyTok verify + wake), `request_enr()`, moved `peer_callback_` into coroutine after ENR enrichment |
| `M` | `include/discv4/dial_scheduler.hpp` | Added `FilterFn` type alias, `filter_fn` member, filter check in `enqueue()`, `make_fork_id_filter()` free function |
| `A` | `test/discv4/enr_request_test.cpp` | 5 tests: ENRRequest encode/decode round-trips |
| `A` | `test/discv4/enr_response_test.cpp` | 9 tests: ENRResponse Parse + ParseEthForkId (missing key, multi-key, empty) |
| `A` | `test/discv4/enr_client_test.cpp` | 3 tests: send reaches target, timeout, full loopback |
| `A` | `test/discv4/enr_enrichment_test.cpp` | 3 tests: `DiscoveredPeer.eth_fork_id` population |
| `A` | `test/discv4/dial_filter_test.cpp` | 7 tests: `make_fork_id_filter` + `DialScheduler` accept/reject/no-ENR |
| `M` | `test/discv4/CMakeLists.txt` | Registered all new test targets |

---

## What Changed Since Previous Checkpoint

### ENRRequest / ENRResponse wire support (EIP-868)
- `discv4_enr_request` mirrors `v4wire.ENRRequest{Expiration uint64}`.
  `RlpPayload()` produces `packet-type(0x05) || RLP([expiration])` ready for signing.
- `discv4_enr_response` mirrors `v4wire.ENRResponse{ReplyTok []byte; Record enr.Record}`.
  `Parse()` follows the same pattern as `discv4_pong::Parse()`.

### ForkId extraction from ENR records (EIP-778 + EIP-2124)
- `ParseEthForkId()` walks the ENR record key-value pairs, finds the `"eth"` key,
  decodes the value as `RLP(enrEntry)` = `RLP([ [hash4, next_uint64] ])`.
- `ForkId` struct mirrors `forkid.ID{Hash [4]byte; Next uint64}`.

### request_enr() in discv4_client
- Public method analogous to `ping()` and `find_node()`.
- Sends ENRRequest, registers a `PendingReply` keyed by `kPacketTypeEnrResponse`.
- `handle_enr_response()` verifies `ReplyTok == expected_hash` before waking the coroutine.
- `handle_enr_request()` silently drops inbound requests (no local ENR record yet).

### Bond → ENR enrichment in handle_neighbours
- `peer_callback_` moved inside the spawned coroutine.
- Flow: `ensure_bond` → `request_enr` → `ParseEthForkId` → set `peer.eth_fork_id` → call `peer_callback_`.
- Caller receives a `DiscoveredPeer` with `eth_fork_id` populated when the remote supports EIP-868.

### DialScheduler pre-dial filter
- `FilterFn = std::function<bool(const DiscoveredPeer&)>` added to `dial_scheduler.hpp`.
- `DialScheduler::filter_fn` applied at the top of `enqueue()` before history/cap checks.
- `make_fork_id_filter(expected_hash)` free function: accepts peers whose `eth_fork_id.hash`
  matches; rejects peers with no `eth_fork_id` (no ENR or no `eth` entry).

### Unit test coverage added
| Test suite | Tests | What it guards |
|---|---|---|
| `EnrRequestEncodeTest` | 5 | ENRRequest encode, packet type byte, round-trip |
| `EnrResponseParseTest` | 5 | ENRResponse Parse: success, hash round-trip, record round-trip, wrong type, too-short |
| `EnrForkIdParseTest` | 4 | ParseEthForkId: round-trip, missing key, skip unrelated keys, empty record |
| `EnrClientTest` | 3 | request_enr: send reaches target, timeout, full loopback reply |
| `EnrEnrichmentTest` | 3 | DiscoveredPeer.eth_fork_id default/set/loopback |
| `ForkIdFilterTest` | 4 | make_fork_id_filter: match, mismatch, no-ENR, ignores next field |
| `DialSchedulerFilterTest` | 3 | enqueue: no filter dials all, filter drops wrong-chain, reject-all yields zero |

---

## Remaining Live Discovery Gap

### test_discovery.cpp does not yet use the ENR filter
`test_discovery.cpp` builds and runs the full discovery + dial flow, but it does not yet set
`scheduler->filter_fn`. Without the filter, the current behaviour is unchanged from the previous
checkpoint — peers are still dialed regardless of their ENR `eth` entry.

### Sepolia fork hash to use in the filter
The Sepolia ENR `eth` entry uses the same CRC32 ForkId as in the ETH Status handshake.
The current Sepolia post-Prague hash (as used in `test_discovery.cpp`) is:

```cpp
// Sepolia: MergeNetsplit@1735371, Shanghai@1677557088, Cancun@1706655072, Prague@1741159776
static const eth::ForkId kSepoliaForkId{ { 0xed, 0x88, 0xb5, 0xfd }, 0 };
```

The `make_fork_id_filter` call for `test_discovery.cpp` should use:
```cpp
scheduler->filter_fn = discv4::make_fork_id_filter( { 0xed, 0x88, 0xb5, 0xfd } );
```

**⚠️ Verify this hash against a live Sepolia peer's ENR before committing.**
The ENR-advertised hash may differ from the ETH Status hash if the node's ENR was not updated
after the Prague fork. Verify by running `test_discovery` with `--log-level debug` and
inspecting the `ParseEthForkId` result before enabling the filter.

---

## Recommended Next Step

1. **Add the ENR filter to `test_discovery.cpp`** — set `scheduler->filter_fn` after the scheduler
   is constructed, using `make_fork_id_filter({ 0xed, 0x88, 0xb5, 0xfd })`.
2. **Run `test_discovery --log-level debug --timeout 60`** and verify:
   - ENR requests are being sent to bonded peers
   - `ParseEthForkId` is returning results for Sepolia peers
   - The filter correctly drops wrong-chain peers before the dial slot is consumed
   - The `connected (right chain)` count improves vs the previous baseline of 0
3. **If the hash needs adjustment** (ENR hash ≠ ETH Status hash), update the filter constant
   and re-run.

---

## How to Run

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
ninja

# All ENR unit tests
./test/discv4/discv4_enr_request_test
./test/discv4/discv4_enr_response_test
./test/discv4/discv4_enr_client_test
./test/discv4/discv4_enr_enrichment_test
./test/discv4/discv4_dial_filter_test

# Regression tests
./test/discv4/discv4_client_test
./test/discv4/discv4_dial_scheduler_test

# Live Sepolia discovery run (next step: verify ENR filter works)
./examples/discovery/test_discovery --log-level debug --timeout 60
```

## Current Status
- This checkpoint reflects the **currently staged git files** as of 2026-03-14.
- The earlier “all dial slots wait the full 5 seconds” diagnosis is now **only partially true**.
  The staged fixes improved slot recycling and connection turnover, but live Sepolia discovery
  is still failing to reach the target validated connections.
- The most recent observed live run of `./examples/discovery/test_discovery --timeout 30 --log-level warn`
  reported:
  - `dialed: 239`
  - `connect_failed: 191`
  - `wrong_chain: 25`
  - `too_many_peers: 12`
  - `status_timeout: 4`
  - `connected (right chain): 0`

### Current interpretation
1. **Throughput improved** after the staged socket-timeout and socket-close fixes.
2. **Wrong-chain peers are now visible in stats**, so the issue is no longer just slot starvation.
3. The remaining major gap is still **chain pre-filtering before dial**. go-ethereum solves that with
   ENR-based filtering before outbound RLPx dials.

---

## Currently Staged Files

| Status | File | Staged purpose |
|---|---|---|
| `M` | `AgentDocs/CHECKPOINT.md` | Update checkpoint to current staged work |
| `M` | `AgentDocs/CLAUDE.md` | Agent guidance update |
| `M` | `examples/CMakeLists.txt` | Build integration for examples |
| `A` | `examples/discovery/CMakeLists.txt` | Build target for live discovery example |
| `A` | `examples/discovery/test_discovery.cpp` | New live Sepolia discovery + ETH Status functional test |
| `M` | `examples/eth_watch/eth_watch.cpp` | Switched to shared dial scheduler path, removed noisy stdout, tightened handshake accounting |
| `A` | `include/discv4/dial_scheduler.hpp` | Shared `WatcherPool` / `DialScheduler` extracted for discv4-based outbound dialing |
| `M` | `include/discv4/discv4_client.hpp` | Public discv4 client API updates to support reply matching and recursive discovery flow |
| `M` | `include/rlpx/framing/message_stream.hpp` | Added stream close hook |
| `M` | `src/discv4/discv4_client.cpp` | Pending-reply tracking, unsolicited-reply rejection, recursive bond/findnode flow hardening |
| `M` | `src/discv4/discv4_pong.cpp` | PONG parsing adjustments |
| `M` | `src/discv4/packet_factory.cpp` | Packet-factory cleanup for current discv4 path |
| `M` | `src/rlpx/framing/message_stream.cpp` | Implemented stream close forwarding |
| `M` | `src/rlpx/protocol/messages.cpp` | RLPx protocol message handling updates |
| `M` | `src/rlpx/rlpx_session.cpp` | Advertise `eth/68` + `eth/69`; close underlying stream on disconnect; reduce noisy disconnect logging |
| `M` | `src/rlpx/socket/socket_transport.cpp` | Enforced real TCP connect timeout; mapped cancelled reads to connection failure |
| `M` | `test/discv4/CMakeLists.txt` | Register new discv4 test target(s) |
| `A` | `test/discv4/dial_scheduler_test.cpp` | Regression test for fast-fail slot recycling |
| `M` | `test/discv4/discv4_client_test.cpp` | Lifetime and recursive bonding UDP-delivery tests |
| `M` | `test/rlpx/frame_cipher_test.cpp` | Real-handshake-secret round-trip regression coverage |

---

## What Changed in the Staged Set

### 1. New live Sepolia discovery harness
- `examples/discovery/test_discovery.cpp` is a new functional CLI that:
  - bonds with Sepolia bootnodes via discv4,
  - recursively discovers peers,
  - dials peers through `DialScheduler`,
  - performs RLPx + ETH Status handshake validation,
  - reports GTest-style pass/fail output and dial statistics.

### 2. Shared discv4 dial scheduler extracted
- `include/discv4/dial_scheduler.hpp` now holds the reusable outbound dial scheduler logic.
- `examples/eth_watch/eth_watch.cpp` was updated to consume the shared scheduler instead of carrying
  its own embedded version.

### 3. Slot turnover and connection cleanup were fixed
- `src/rlpx/socket/socket_transport.cpp` now actually honors the connect timeout by cancelling the socket.
- `src/rlpx/rlpx_session.cpp` now closes the underlying stream in `disconnect()`, preventing zombie sessions
  from keeping sockets and pending reads alive.
- These changes match the observed jump from roughly ~138 dials / 30s to ~239 dials / 30s in the live test.

### 4. ETH handshake compatibility was widened
- `src/rlpx/rlpx_session.cpp` now advertises both `eth/68` and `eth/69` instead of only `eth/69`.
- This aligns better with mixed live-peer capability sets and removes one possible early disconnect cause.

### 5. Logging was de-noised for normal runs
- `examples/eth_watch/eth_watch.cpp` moved noisy `std::cout` diagnostics to logger-based output.
- Pre-HELLO disconnect logging in `src/rlpx/rlpx_session.cpp` was reduced from warning-level noise to debug-level detail.

---

## Tests Added or Strengthened in the Staged Set

| Test | File | What it guards |
|---|---|---|
| `DialSchedulerTest.FastFailReleasesSlotForNextPeer` | `test/discv4/dial_scheduler_test.cpp` | Confirms fast-fail dials immediately recycle slots and drain the queue |
| `DiscoveryClientLifetimeTest.ClientAlive_PacketReachesListener` | `test/discv4/discv4_client_test.cpp` | Confirms a live discv4 client can send UDP packets while kept alive across `io.run()` |
| `DiscoveryClientLifetimeTest.ClientOuterScope_MultiPingReachesListeners` | same | Guards the outer-scope lifetime pattern used by callers |
| `RecursiveBondingTest.FindNodeSentToPeer_PacketReachesListener` | same | Verifies `find_node()` packet delivery |
| `RecursiveBondingTest.PingToNewPeer_PacketReachesListener` | same | Verifies `ping()` packet delivery to newly seen peers |
| `RecursiveBondingTest.FindNodeSentToMultiplePeers_AllReceivePackets` | same | Verifies multi-peer recursive findnode packet delivery |
| `FrameCipherVectorTest.InitiatorToResponderRoundTrip` | `test/rlpx/frame_cipher_test.cpp` | Uses real handshake-derived secrets to prove initiator→responder frame decrypt works |

---

## Remaining Live Discovery Failure

The current live failure is **not** best explained by frame-cipher corruption anymore.

### What the staged work has already improved
- Faster dial turnover
- Explicit dial-failure accounting
- Better ETH capability negotiation
- Cleaner disconnect handling

### What still appears to be missing
- **ENR-based chain filtering before dial**

go-ethereum does not blindly dial every discv4-discovered node. For discv4, it requests the remote
ENR first and filters nodes by the `eth` ENR entry / ForkID before handing them to the dialer.
Our current staged code still discovers chain-agnostic peers and rejects wrong-chain nodes later,
after TCP and RLPx work has already been spent.

---

## Recommended Next Step

Implement the **minimal discv4 + ENRRequest path** modeled after go-ethereum:

1. Add `ENRREQUEST` / `ENRRESPONSE` packet support in the discv4 wire path.
2. Request ENR for newly bonded peers before enqueueing them for RLPx dialing.
3. Extract the `eth` ENR entry and filter by Sepolia fork hash before calling `DialScheduler::enqueue()`.
4. Add small unit tests mirroring the go-ethereum discv4 ENR request/response path where practical.

This remains the shortest path to fixing the current Sepolia discovery failure without taking on a full discv5 implementation.

### ENRREQUEST implementation notes from this conversation

- **Reference files in go-ethereum**:
  - `go-ethereum/p2p/discover/v4wire/v4wire.go`
  - `go-ethereum/p2p/discover/v4_udp.go`
  - `go-ethereum/eth/protocols/eth/discovery.go`
  - `go-ethereum/eth/backend.go`
- **Important protocol fact**: discv4 and discv5 are both UDP-based. The reason to prefer discv4 here is
  not transport simplicity, but implementation size: `discv4 + ENRRequest` is a small extension of the
  current code, whereas discv5 is a much larger encrypted-session protocol.
- **Minimal scope boundary**: do **not** rewrite discovery, scheduler, RLPx, or ETH handshake flow. The only
  new behavior should be: `bond -> request ENR -> read eth entry -> filter -> enqueue dial`.
- **Current data gap**: `DiscoveredPeer` currently carries only `node_id`, `ip`, `udp_port`, `tcp_port`, and
  `last_seen`. It does not yet carry ENR-derived chain metadata.
- **First code step**: mirror go-ethereum packet support by adding discv4 packet types 5 and 6
  (`ENRREQUEST`, `ENRRESPONSE`) and the minimal encode/decode support for those payloads.
- **Second code step**: add a request/reply path in `discv4_client` analogous to the existing `PING/PONG` and
  `FIND_NODE/NEIGHBOURS` matching, then request ENR after a peer is bonded.
- **Third code step**: decode just enough of the ENR record to read the `eth` entry / fork identifier needed
  for Sepolia filtering before the peer reaches `DialScheduler::enqueue()`.
- **First tests to add**:
  1. wire encode/decode coverage for `ENRREQUEST` / `ENRRESPONSE`,
  2. reply-matching coverage for the new request/response path,
  3. a small filtering test proving a wrong-chain ENR is dropped before dialing.

---

## How to Run

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
ninja
ctest --output-on-failure

# Focused tests touched by the current staged work
./test/discv4/discv4_dial_scheduler_test
./test/discv4/discv4_client_test
./test/rlpx/rlpx_frame_cipher_tests

# Live Sepolia discovery run
./examples/discovery/test_discovery --log-level warn --timeout 30
```
