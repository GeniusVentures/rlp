Discov# Project Checkpoint - RLP/RLPx/Discovery Implementation

**Date**: March 5, 2026  
**Project**: GNUS.AI Super Genius Blockchain - Ethereum Protocol Support  
**Status**: Foundation Complete, Advanced Features Required

---

## ✅ COMPLETED - Core Infrastructure

### 1. RLP (Recursive Length Prefix) Encoding/Decoding
**Status**: ✅ Fully functional and tested (336/336 tests passing)

- ✅ Basic types: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `uint256`, booleans, byte arrays
- ✅ Nested lists and complex structures
- ✅ Streaming encoder/decoder for large payloads (`RlpLargeStringEncoder`, `RlpLargeStringDecoder`)
- ✅ Chunked list encoding/decoding (`RlpChunkedListEncoder`, `RlpChunkedListDecoder`)
- ✅ Error handling with `boost::outcome`
- ✅ Type safety with C++20 templates
- ✅ Comprehensive test coverage (15 test suites)

**Files**:
- `include/rlp/rlp_encoder.hpp`, `src/rlp/rlp_encoder.cpp`
- `include/rlp/rlp_decoder.hpp`, `src/rlp/rlp_decoder.cpp`
- `include/rlp/rlp_streaming.hpp`, `src/rlp/rlp_streaming.cpp`

---

### 2. RLPx Protocol (Encrypted Transport Layer)
**Status**: ✅ Fully functional

- ✅ ECIES encryption/decryption (`auth/ecies_cipher`)
- ✅ ECDH key exchange (`crypto/ecdh`)
- ✅ Auth handshake (initiate/respond)
- ✅ Frame cipher (AES-256-CTR with message authentication)
- ✅ Message framing and routing
- ✅ Protocol message handling (Hello, Disconnect, Ping, Pong)
- ✅ Session lifecycle management
- ✅ Boost.Asio async I/O

**Files**:
- `include/rlpx/rlpx_session.hpp`, `src/rlpx/rlpx_session.cpp`
- `include/rlpx/auth/auth_handshake.hpp`
- `include/rlpx/crypto/ecdh.hpp`, `crypto/aes.hpp`, `crypto/kdf.hpp`
- `include/rlpx/framing/frame_cipher.hpp`, `framing/message_stream.hpp`

---

### 3. Discovery v4 Protocol (Peer Discovery)
**Status**: ✅ Fully functional and tested (16/16 tests passing)

- ✅ PING/PONG packet creation and parsing
- ✅ Keccak-256 hashing for packet integrity
- ✅ UDP-based peer discovery
- ✅ Node ID management (64-byte public key hash)
- ✅ Endpoint encoding (IP + UDP/TCP ports)
- ✅ Packet expiration validation
- ✅ Client lifecycle (start/stop)
- ✅ Peer table management
- ✅ Callback system for discovered peers and errors
- ✅ Bootstrap node configuration for major chains

**Files**:
- `include/discv4/discv4_client.hpp`, `src/discv4/discv4_client.cpp`
- `include/discv4/discv4_packet.hpp`, `src/discv4/discv4_packet.cpp`
- `include/discv4/discv4_ping.hpp`, `discv4_pong.hpp`
- `include/discv4/bootnodes.hpp` (Mainnet, Sepolia, Polygon, BSC, Base)

---

### 4. ETH Protocol Messages (Basic)
**Status**: ✅ Partially implemented

**Working**:
- ✅ STATUS message encoding/decoding
- ✅ NEW_BLOCK_HASHES message encoding/decoding
- ✅ NEW_POOLED_TRANSACTION_HASHES encoding/decoding
- ✅ GET_BLOCK_HEADERS encoding/decoding

**Files**:
- `include/eth/messages.hpp`, `src/eth/messages.cpp`
- `include/eth/eth_types.hpp`
- `include/eth/objects.hpp`, `src/eth/objects.cpp`

---

### 5. Example Application (eth_watch)
**Status**: ✅ Basic connectivity working

**Capabilities**:
- ✅ Connect to bootstrap nodes using enode URLs
- ✅ Chain presets (mainnet, sepolia, polygon, bsc, base, etc.)
- ✅ RLPx handshake (Hello message)
- ✅ ETH protocol handshake (Status message)
- ✅ Basic message reception (NewBlockHashes)
- ✅ Ping/Pong handler

**Files**:
- `examples/eth_watch/eth_watch.cpp`

---

## 🚧 IN PROGRESS / MISSING - Advanced Features

### 1. Contract Event Watching & Filtering ❌
**Status**: NOT IMPLEMENTED

**What's Needed**:
```cpp
// Missing: Event log filtering system
class EventFilter {
    std::vector<Hash> topics;        // Event signature + indexed parameters
    std::vector<Address> addresses;  // Contract addresses to watch
    BlockNumber from_block;
    BlockNumber to_block;
};

// Missing: Log decoding
struct EventLog {
    Address address;
    std::vector<Hash> topics;
    Bytes data;
    BlockNumber block_number;
    TransactionHash tx_hash;
};

// Missing: ABI decoder for event data
class ABIDecoder {
    // Parse event signature -> topic[0]
    // Decode indexed parameters from topic[1], topic[2], topic[3]
    // Decode non-indexed parameters from data field
};
```

**Required Files** (need creation):
- `include/eth/event_filter.hpp` - Event filtering logic
- `include/eth/log_decoder.hpp` - RLP log decoding
- `include/eth/abi_decoder.hpp` - ABI event decoding
- `src/eth/event_filter.cpp`
- `src/eth/log_decoder.cpp`
- `src/eth/abi_decoder.cpp`

**Missing ETH Protocol Messages**:
- ❌ `GetLogs` request/response
- ❌ `NewBlock` message (full blocks with transactions)
- ❌ `Receipts` message (transaction receipts with logs)
- ❌ `GetBlockBodies` request/response

---

### 2. Transaction Decoding & Inspection ❌
**Status**: Partially implemented (types exist, no full parsing)

**What's Needed**:
```cpp
// Exists but incomplete
struct Transaction {
    uint64_t nonce;
    intx::uint256 gas_price;
    uint64_t gas_limit;
    Address to;              // ✅ exists
    intx::uint256 value;
    Bytes data;              // ❌ No parsing of contract call data
    // Missing: signature (v, r, s)
    // Missing: transaction type (legacy, EIP-1559, EIP-4844)
};

// Missing: Contract call data decoder
class TransactionDecoder {
    // Parse function selector (first 4 bytes)
    // Decode function parameters using ABI
    // Identify contract method being called
};
```

**Required**:
- Complete transaction RLP parsing with all fields
- Function selector identification
- ABI function parameter decoding
- Transaction type handling (legacy, EIP-1559, blob transactions)

---

### 3. Block Processing & Chain Tracking ❌
**Status**: NOT IMPLEMENTED

**What's Needed**:
```cpp
// Missing: Block header tracking
class ChainTracker {
    BlockNumber latest_block;
    Hash latest_hash;
    
    // Track block progression
    void on_new_block(const BlockHeader& header);
    void on_reorg(BlockNumber reorg_depth);
    
    // Query capabilities
    bool is_synced();
    BlockNumber current_height();
};

// Missing: Receipt storage and querying
class ReceiptStore {
    // Store receipts with logs
    void store_receipt(const TransactionReceipt& receipt);
    
    // Query logs by filter
    std::vector<EventLog> get_logs(const EventFilter& filter);
};
```

**Required Files**:
- `include/eth/chain_tracker.hpp` - Track blockchain state
- `include/eth/receipt_store.hpp` - Store and query receipts
- `src/eth/chain_tracker.cpp`
- `src/eth/receipt_store.cpp`

---

### 4. Callback/Action System for Events ❌
**Status**: NOT IMPLEMENTED

**What's Needed**:
```cpp
// Missing: Event callback registration
class EventWatcher {
    using EventCallback = std::function<void(const EventLog&)>;
    
    // Register callback for specific event signature
    void watch_event(
        const std::string& event_signature,  // e.g., "Transfer(address,address,uint256)"
        const std::vector<Address>& contracts,
        EventCallback callback
    );
    
    // Stop watching
    void unwatch_event(const std::string& event_signature);
    
    // Process incoming logs and trigger callbacks
    void process_logs(const std::vector<EventLog>& logs);
};

// Example usage:
watcher.watch_event(
    "Transfer(address,address,uint256)",
    {usdc_address, usdt_address},
    [](const EventLog& log) {
        // Decode transfer event
        Address from = decode_address(log.topics[1]);
        Address to = decode_address(log.topics[2]);
        uint256 amount = decode_uint256(log.data);
        
        std::cout << "Transfer: " << from << " -> " << to 
                  << " amount: " << amount << "\n";
        
        // Call custom function based on event
        if (amount > threshold) {
            handle_large_transfer(from, to, amount);
        }
    }
);
```

**Required Files**:
- `include/eth/event_watcher.hpp` - Event callback system
- `src/eth/event_watcher.cpp`

---

### 5. Contract State Querying ❌
**Status**: NOT IMPLEMENTED

**What's Needed**:
```cpp
// Missing: eth_call support
class ContractCaller {
    // Call contract view/pure function
    Result<Bytes> call(
        const Address& contract,
        const Bytes& call_data,
        BlockNumber block = BlockNumber::Latest
    );
    
    // Get storage slot value
    Result<Hash> get_storage_at(
        const Address& contract,
        const intx::uint256& slot,
        BlockNumber block = BlockNumber::Latest
    );
};

// Missing: JSON-RPC client (if connecting to full node RPC)
class JsonRpcClient {
    // eth_call, eth_getLogs, eth_getBlockByNumber, etc.
};
```

**Required Files**:
- `include/eth/contract_caller.hpp` - Contract read operations
- `include/eth/json_rpc_client.hpp` - Optional RPC client
- `src/eth/contract_caller.cpp`
- `src/eth/json_rpc_client.cpp`

---

### 6. Discovery v5 Protocol ❌
**Status**: NOT IMPLEMENTED (only v4 exists)

**What's Needed**:
- Discovery v5 packet encoding/decoding
- ENR (Ethereum Node Records) support
- Topic-based discovery
- Better NAT traversal

**Note**: Discovery v4 is sufficient for basic peer discovery. v5 is optional upgrade.

---

## 📋 PRIORITY TODO LIST

### HIGH PRIORITY (Required for basic contract watching)

1. **Implement Receipt/Log Decoding** (2-3 days)
   - [ ] Add `GetReceipts` message encoding/decoding
   - [ ] Add `Receipts` message encoding/decoding  
   - [ ] Parse transaction receipts with logs
   - [ ] Extract event logs (address, topics, data)

2. **Implement Event Filtering** (2-3 days)
   - [ ] Create `EventFilter` class
   - [ ] Topic matching logic (AND/OR conditions)
   - [ ] Address filtering
   - [ ] Block range filtering

3. **Implement Basic ABI Decoding** (3-4 days)
   - [ ] Event signature to topic[0] hash (Keccak256)
   - [ ] Decode indexed parameters from topics
   - [ ] Decode non-indexed parameters from data field
   - [ ] Support basic types: address, uint256, bytes32, string, bytes

4. **Implement EventWatcher Callback System** (2-3 days)
   - [ ] Event signature registration
   - [ ] Callback trigger on matching logs
   - [ ] Example handlers for common events (Transfer, Approval, etc.)

5. **Enhance eth_watch Example** (1-2 days)
   - [ ] Add `--watch-contract <address>` flag
   - [ ] Add `--watch-event <signature>` flag
   - [ ] Display decoded events in real-time
   - [ ] Example: Watch USDC/USDT transfers

---

### MEDIUM PRIORITY (Enhanced functionality)

6. **Implement Block Tracking** (2-3 days)
   - [ ] Track latest block number and hash
   - [ ] Handle block reorganizations
   - [ ] Query historical blocks

7. **Implement Full Transaction Decoding** (2-3 days)
   - [ ] Parse all transaction fields (v, r, s, type)
   - [ ] Support EIP-1559 transactions
   - [ ] Decode function calls (selector + params)

8. **Add Receipt Storage** (1-2 days)
   - [ ] In-memory receipt cache
   - [ ] Query logs by filter from stored receipts
   - [ ] Optional: Persistent storage (SQLite/RocksDB)

---

### LOW PRIORITY (Nice to have)

9. **Contract State Queries** (3-4 days)
   - [ ] eth_call support via P2P or RPC
   - [ ] Storage slot reading
   - [ ] Balance queries

10. **JSON-RPC Client** (Optional, 2-3 days)
    - [ ] Connect to Infura/Alchemy/local node RPC
    - [ ] eth_getLogs, eth_getBlockByNumber
    - [ ] Fallback when P2P doesn't provide data

11. **Discovery v5** (Optional, 5-7 days)
    - [ ] ENR support
    - [ ] Topic-based peer discovery
    - [ ] Better for resource-constrained nodes

---

## 🎯 MINIMAL VIABLE PRODUCT (MVP)

**Goal**: Watch for specific contract events and trigger actions

**Required Components**:
1. ✅ RLP encoding/decoding (DONE)
2. ✅ RLPx session establishment (DONE)
3. ✅ Discovery v4 peer finding (DONE)
4. ✅ ETH protocol STATUS exchange (DONE)
5. ❌ **GetReceipts request/response** (TODO)
6. ❌ **Receipt/Log decoding** (TODO)
7. ❌ **Event filtering** (TODO)
8. ❌ **Basic ABI event decoding** (TODO)
9. ❌ **Callback system** (TODO)

**Estimated Time to MVP**: 10-15 days of focused development

---

## 🔧 RECOMMENDED NEXT STEPS

### Step 1: Implement Receipt Messages (Day 1-2)
```bash
# Create new files
touch include/eth/receipts.hpp
touch src/eth/receipts.cpp

# Add to messages.hpp:
# - encode_get_receipts()
# - decode_get_receipts()
# - encode_receipts()
# - decode_receipts()
```

### Step 2: Implement Event Filtering (Day 3-4)
```bash
touch include/eth/event_filter.hpp
touch src/eth/event_filter.cpp

# Features:
# - Topic matching
# - Address filtering
# - Block range
```

### Step 3: Implement ABI Decoder (Day 5-7)
```bash
touch include/eth/abi_decoder.hpp
touch src/eth/abi_decoder.cpp

# Features:
# - Keccak256 event signature hash
# - Indexed parameter decoding
# - Data field decoding
# - Support common types
```

### Step 4: Implement Callback System (Day 8-9)
```bash
touch include/eth/event_watcher.hpp
touch src/eth/event_watcher.cpp

# Features:
# - Register event handlers
# - Match logs to handlers
# - Trigger callbacks
```

### Step 5: Enhance eth_watch (Day 10)
```bash
# Update examples/eth_watch/eth_watch.cpp
# Add command-line options:
# --watch-contract <address>
# --watch-event <signature>
# --on-event <command>
```

---

## 📚 DOCUMENTATION GAPS

### Missing Documentation:
- ❌ Event watching API guide
- ❌ ABI encoding/decoding examples
- ❌ Contract interaction patterns
- ❌ Filter configuration guide
- ❌ Callback registration examples

### Existing Documentation:
- ✅ RLP encoding/decoding (README.md)
- ✅ RLPx protocol (docs/rlpx/README.md)
- ✅ Discovery v4 (BOOTNODES_CONFIGURATION.md)
- ✅ Bootstrap nodes (PUBLIC_NODES_FOR_TESTING.md)
- ✅ Build instructions (README.md)

---

## 🏗️ ARCHITECTURE RECOMMENDATIONS

### Suggested Module Structure:
```
include/eth/
├── messages.hpp          [✅ EXISTS - basic messages]
├── receipts.hpp          [❌ CREATE - receipt handling]
├── event_filter.hpp      [❌ CREATE - filtering logic]
├── event_watcher.hpp     [❌ CREATE - callback system]
├── abi_decoder.hpp       [❌ CREATE - ABI parsing]
├── chain_tracker.hpp     [❌ CREATE - block tracking]
└── contract_caller.hpp   [❌ CREATE - state queries]

src/eth/
├── messages.cpp          [✅ EXISTS]
├── receipts.cpp          [❌ CREATE]
├── event_filter.cpp      [❌ CREATE]
├── event_watcher.cpp     [❌ CREATE]
├── abi_decoder.cpp       [❌ CREATE]
├── chain_tracker.cpp     [❌ CREATE]
└── contract_caller.cpp   [❌ CREATE]

examples/
├── eth_watch/            [✅ EXISTS - basic version]
├── contract_watcher/     [❌ CREATE - advanced version]
└── event_logger/         [❌ CREATE - log all events]

test/eth/
├── receipts_test.cpp     [❌ CREATE]
├── event_filter_test.cpp [❌ CREATE]
├── abi_decoder_test.cpp  [❌ CREATE]
└── event_watcher_test.cpp[❌ CREATE]
```

---

## ⚠️ KNOWN LIMITATIONS

1. **Bootstrap Nodes**: Only provide peer discovery, NOT block data
   - Must discover and connect to full nodes for chain data
   - Current eth_watch connects to bootstrap nodes (limited functionality)

2. **P2P vs RPC**: 
   - P2P protocol requires full handshake and syncing
   - May be easier to use JSON-RPC for historical queries
   - Consider hybrid approach

3. **Chain Sync**: 
   - No block synchronization implemented
   - No state trie access
   - Limited to real-time event watching (not historical)

4. **Memory**: 
   - No persistent storage yet
   - Events only stored in memory
   - Will lose data on restart

---

## 💡 EXAMPLE USE CASE

**Scenario**: Watch USDC transfers and trigger alert for large transfers

**What Exists Now**:
```cpp
// ✅ Can connect to peer
// ✅ Can establish RLPx session
// ✅ Can exchange ETH Status
// ❌ CANNOT request receipts
// ❌ CANNOT decode event logs
// ❌ CANNOT register callbacks
```

**What's Needed**:
```cpp
EventWatcher watcher;

// Watch USDC Transfer events
watcher.watch_event(
    "Transfer(address,address,uint256)",
    {USDC_ADDRESS},
    [](const EventLog& log) {
        auto [from, to, amount] = decode_transfer(log);
        if (amount > 1000000 * 1e6) { // > 1M USDC
            send_alert("Large USDC transfer detected!");
            log_to_database(from, to, amount);
        }
    }
);

watcher.start(); // Begin watching
```

---

## 📊 SUMMARY

| Component | Status | Tests | Priority |
|-----------|--------|-------|----------|
| RLP Core | ✅ Complete | 336/336 | - |
| RLPx Protocol | ✅ Complete | All passing | - |
| Discovery v4 | ✅ Complete | 16/16 | - |
| ETH Messages (Basic) | ⚠️ Partial | 3/3 | - |
| Receipt Handling | ❌ Missing | 0 | HIGH |
| Event Filtering | ❌ Missing | 0 | HIGH |
| ABI Decoding | ❌ Missing | 0 | HIGH |
| Event Callbacks | ❌ Missing | 0 | HIGH |
| Block Tracking | ❌ Missing | 0 | MEDIUM |
| Contract Calls | ❌ Missing | 0 | LOW |

**Overall Progress**: ~40% complete  
**To MVP**: ~60% remaining (10-15 days estimated)

---

## 🚀 QUICK START FOR NEXT DEVELOPER

```bash
# 1. Current state verification
cd build/OSX/Debug
ctest  # Should show 336/336 passing

# 2. Test current eth_watch
./examples/eth_watch/eth_watch --chain sepolia

# 3. Start implementing receipts
cd ../../..
mkdir -p include/eth test/eth
touch include/eth/receipts.hpp src/eth/receipts.cpp
touch test/eth/receipts_test.cpp

# 4. Follow priority TODO list above
```

---

**End of Checkpoint**

