# Checkpoint — 2026-03-14

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
