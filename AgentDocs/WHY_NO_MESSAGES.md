# Why eth_watch Doesn't Receive Messages from Bootstrap Nodes

## The Issue

When you run `./eth_watch --chain sepolia`, it connects and prints "Connected. Waiting for messages..." but **no messages arrive**. This is **expected behavior** because:

### Bootstrap Nodes are Discovery-Only

Bootstrap nodes serve **exclusively for peer discovery** via the **discv4 protocol** (UDP). They:
- ❌ Do NOT send block data
- ❌ Do NOT exchange HELLO/Status messages
- ❌ Do NOT participate in the RLPx/ETH protocol
- ✅ Only respond to PING/PONG discovery packets

### Real Peer Nodes Send Block Data

Real Ethereum nodes provide block data via the **RLPx + ETH protocol** (TCP). They:
- ✅ Exchange HELLO messages
- ✅ Exchange ETH Status messages
- ✅ Send NewBlockHashes, NewBlock, Transactions, etc.
- ✅ Allow you to actually see blockchain data

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Your Client (eth_watch)                  │
└─────────────────────────────────────────────────────────────┘
                              ↓
                    ┌─────────────────────┐
                    │ Need Peer Discovery │
                    └─────────────────────┘
                              ↓
         ┌────────────────────────────────────────┐
         │                                        │
         ↓                                        ↓
    ┌─────────────┐  PING/PONG (UDP)        ┌──────────────┐
    │  Bootstrap  │ ←────────────────────→ │ discv4 Node  │
    │   Node 1    │   discv4 Protocol       │   Discovery  │
    └─────────────┘                         └──────────────┘
         ↓                                        ↓
    NEIGHBOURS list (returns other peers)   Real Peer Info
         ↓                                        ↓
    ┌──────────────────────────────────────────────────┐
    │ Find Real Peer Nodes in NEIGHBOURS Response      │
    └──────────────────────────────────────────────────┘
         ↓
    ┌────────────────────────────────────────┐
    │                                        │
    ↓                                        ↓
┌─────────────────┐  HELLO/Status (TCP)  ┌──────────────┐
│    Your RLPx    │ ←───────────────────→ │  Real Peer   │
│   eth_watch     │   RLPx + ETH Protocol │    Node      │
└─────────────────┘                       └──────────────┘
                              ↓
                    Block Data Arrives! ✅
                    (NewBlockHashes, NewBlock, etc.)
```

## Current Limitations

### What Works
- ✅ Bootnode configurations for 8 EVM chains
- ✅ RLPx connection to any node
- ✅ ETH protocol Status message exchange
- ✅ Generic message parsing for ETH protocol

### What's Missing
- ❌ Full discv4 implementation (partially implemented, not complete)
- ❌ K-Bucket routing for peer discovery
- ❌ NEIGHBOURS message parsing
- ❌ Automatic peer discovery from bootstrap nodes

## Solution: Connect to Real Peer Nodes

### Option 1: Use Known Public Peers

Get active peer enodes from RPC:
```bash
curl -s -X POST https://eth.llamarpc.com \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"admin_peers","params":[],"id":1}' | jq '.result[0]'
```

Then connect directly:
```bash
./eth_watch <peer_host> <peer_port> <peer_pubkey_hex>
```

### Option 2: Run Your Own Full Node

Run Geth/Lighthouse/etc locally, which discovers peers automatically:
```bash
geth --http --http.api admin,web3,eth,net --http.addr 127.0.0.1
```

Then query for its discovered peers and connect to them.

### Option 3: Use the maintained discovery harnesses

The maintained discovery implementation lives under the current `discv4_client` / `DialScheduler` path, with live harnesses under:
```
examples/discovery/test_discovery.cpp
examples/discovery/test_enr_survey.cpp
```

Use those binaries to exercise automatic peer discovery from bootstrap nodes instead of the old `discovery.hpp` sketch.

## Next Steps

1. **Short-term**: Use real peer node enodes for testing
2. **Medium-term**: Continue improving the existing `discv4_client` + scheduler discovery flow
3. **Long-term**: Add peer caching, K-Bucket routing, persistence

## References

- [Ethereum devp2p Specification](https://github.com/ethereum/devp2p)
- [discv4 Protocol](https://github.com/ethereum/devp2p/blob/master/discv4.md)
- [RLPx Transport Protocol](https://github.com/ethereum/devp2p/blob/master/rlpx.md)
- [eth Protocol Specification](https://github.com/ethereum/devp2p/blob/master/caps/eth.md)


