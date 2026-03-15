# Checkpoint — 2026-03-06 (End of Day 8 extended session)

## Build Status
- **`ninja` builds with zero errors, zero warnings** as of end of session.
- **`ctest` 441/441 tests pass** (no regressions). The new `HandshakeVectorsTest` compiles and is registered in CTest.

---

## What Was Accomplished This Session

### Days 2–7 (prior sessions, already complete)
- Full ETH/66+ packet encode/decode (transactions, block bodies, new block, receipts)
- ABI decoder (`eth/abi_decoder.hpp/.cpp`) with keccak256 event signature hashing
- `EthWatchService` — subscribe/unwatch, `process_message`, `process_receipts`, `process_new_block`
- `ChainTracker` — deduplication window, tip tracking
- GNUS.AI contract address constants + unit tests (`gnus_contracts_test`)
- spdlog integration (`src/base/logger.cpp`, `include/base/logger.hpp`) with `--log-level` CLI arg
- `eth_watch` example binary wired end-to-end: discv4 discovery → RLPx connect → ETH status → watch events
- discv4 full bond cycle: PING → wait for PONG → wait for reverse PING → send PONG → send FIND_NODE → parse NEIGHBOURS
- Magic number cleanup across all `src/` and `include/` files; constants extracted to named `constexpr`
- All markdown docs moved to `AgentDocs/`
- `AGENT_MISTAKES.md` created with M001–M016

### Day 8 (this session)
- **RLPx handshake rewrite** based on direct read of `go-ethereum/p2p/rlpx/rlpx.go`:
  - `create_auth_message`: RLP-encode `[sig, pubkey, nonce, version=4]`, append 100 bytes random padding, EIP-8 prefix (uint16-BE of ciphertext length), ECIES encrypt
  - `parse_ack_message`: read 2-byte length prefix, read body, ECIES decrypt, RLP-decode `[eph_pubkey, nonce, version]`
  - `derive_frame_secrets`: exact port of go-ethereum `secrets()` — ECDH → sharedSecret → aesSecret → macSecret → MAC seeds
  - `FrameCipher` rewrite: exact port of go-ethereum `hashMAC` and `sessionState` (AES-256-CTR enc/dec, running-keccak MAC accumulator, `computeHeader`/`computeFrame`)
- **`test/rlpx/handshake_vectors_test.cpp`** — new test validating `derive_frame_secrets()` against go-ethereum `TestHandshakeForwardCompatibility` vectors (Auth₂/Ack₂, responder perspective)
- **`include/rlpx/auth/auth_handshake.hpp`** — `derive_frame_secrets` moved to `public static`; free function `derive_frame_secrets(keys, is_initiator)` added in `auth_handshake.cpp` as test entry point

---

## Current Failure Mode (the problem to solve Monday)

The binary connects to discovered Sepolia peers, completes auth (sends auth, receives ack), derives secrets — but **frame MAC verification fails immediately on the first frame**:

```
[debug][rlpx.auth] execute: ack parsed successfully
[debug][rlpx.frame] decrypt_header: MAC mismatch
Error: Invalid message
```

### Root Cause Hypothesis
The `FrameCipher::HashMAC` model stores all bytes written and recomputes `keccak256(all_written)` on each `sum()` call. This is correct for the **seed initialisation** phase (go-ethereum's `mac.Write(xor(MAC,nonce)); mac.Write(auth)`) but the `computeHeader` / `computeFrame` operations in go-ethereum update the *running* keccak accumulator in-place — they do NOT restart from the seed bytes.

Specifically, `computeHeader` in go-ethereum does:
```go
sum1 := m.hash.Sum(m.hashBuffer[:0])   // peek at current state WITHOUT resetting
return m.compute(sum1, header)          // then write aesBuffer back into hash
```
And `m.hash` is a `keccak.NewLegacyKeccak256()` that was seeded once and **continues accumulating** — it is NOT re-hashed from scratch on every call.

Our `HashMAC::sum()` correctly recomputes `keccak256(written)` which equals `hash.Sum()` only because keccak is deterministic. **BUT** `compute()` then calls `m.hash.Write(aesBuffer)` which appends 16 bytes to the running accumulator. Our `HashMAC::compute_header` / `compute_frame` must also append those 16 `aesBuffer` bytes to `written` after every call, otherwise `sum()` diverges from go-ethereum's `hash.Sum()` after the first frame.

### The Exact Fix Needed Monday

In `src/rlpx/framing/frame_cipher.cpp`, `HashMAC::compute()` must append the `aesBuffer` XOR result back into `written`:

```cpp
// go-ethereum: m.hash.Write(m.aesBuffer[:])
write(aes_buf.data(), aes_buf.size());  // keep accumulator in sync
```

This single line is almost certainly the MAC mismatch root cause. The `HandshakeVectorsTest` currently only validates key derivation (AES secret, MAC secret, ingress seed hash) — it does NOT yet exercise `computeHeader`/`computeFrame`. A new `FrameCipherMacTest` with go-ethereum's known frame vectors should be written to verify this fix before live testing.

---

## Key Files

| File | Purpose |
|------|---------|
| `src/rlpx/auth/auth_handshake.cpp` | Handshake: create_auth, parse_ack, derive_frame_secrets |
| `src/rlpx/auth/ecies_cipher.cpp` | ECIES encrypt/decrypt (OpenSSL) |
| `src/rlpx/crypto/ecdh.cpp` | secp256k1 ECDH, key generation |
| `src/rlpx/framing/frame_cipher.cpp` | HashMAC + AES-CTR frame enc/dec — **has the bug above** |
| `include/rlpx/auth/auth_keys.hpp` | `AuthKeyMaterial`, `FrameSecrets` structs |
| `include/rlpx/framing/frame_cipher.hpp` | `FrameCipher` public interface |
| `include/rlpx/rlpx_types.hpp` | All `constexpr` size constants |
| `test/rlpx/handshake_vectors_test.cpp` | go-ethereum vector test for key derivation |
| `test/rlpx/frame_cipher_test.cpp` | Round-trip frame enc/dec test (does NOT use go-ethereum vectors yet) |
| `examples/eth_watch/eth_watch.cpp` | Live CLI tool: `./eth_watch --chain sepolia --log-level debug` |
| `AgentDocs/AGENT_MISTAKES.md` | Agent error log — **read before writing any code** |

---

## How to Run

```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/rlp/build/OSX/Debug
ninja                        # build
ctest --output-on-failure    # run all 441 tests

# Run new vector test only
./test/rlpx/rlpx_handshake_vectors_tests

# Live Sepolia test
./examples/eth_watch/eth_watch --chain sepolia --log-level debug
```

---

## Monday Task List (priority order)

1. **Fix `HashMAC::compute()` in `frame_cipher.cpp`** — append `aesBuffer` bytes into `written` after every `compute()` call. This is the single most likely cause of the MAC mismatch.

2. **Write `FrameCipherMacTest` using go-ethereum frame vectors** — go-ethereum `TestFrameRW` in `p2p/rlpx/rlpx_test.go` has known plaintexts and expected ciphertexts. Use those to verify `encrypt_frame` / `decrypt_frame` produce identical output before retrying live connection.

3. **Re-run live test** — after #1 and #2 pass, `./eth_watch --chain sepolia --log-level debug` should reach `HELLO from peer: Geth/...`.

4. **ETH STATUS handling** — after HELLO, send ETH Status message (message id 0x10, network_id=11155111, genesis hash, fork id). Currently `EthWatchService::process_message` dispatches on message ids but the STATUS exchange is not fully wired in `rlpx_session.cpp`.

5. **NewBlockHashes → GetBlockBodies → GetReceipts pipeline** — once STATUS succeeds, implement the receipt-fetching loop in `EthWatchService`.

---

## go-ethereum Reference
The local copy of go-ethereum is at:
```
/Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/rlp/rlp/  (go-ethereum source)
```
Key files to read:
- `p2p/rlpx/rlpx.go` — frame cipher, handshake (already read this session)
- `p2p/rlpx/rlpx_test.go` — `TestFrameRW`, `TestHandshakeForwardCompatibility` vectors
- `eth/protocols/eth/handler.go` — ETH STATUS, NewBlockHashes dispatch


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
