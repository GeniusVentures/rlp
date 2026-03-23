# Quick Test Guide: Getting Real Peer Data

## Summary

I've created two new resources to help you test with real Ethereum peers:

### 1. **PUBLIC_NODES_FOR_TESTING.md**
- Lists public RPC endpoints for Mainnet and Sepolia
- Shows how to query for live peer information
- Provides examples and scripts

### 2. **test_eth_watch.sh** (Automated!)
Located in: `/Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/`

Usage:
```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp
./test_eth_watch.sh sepolia    # Test Sepolia testnet
./test_eth_watch.sh mainnet    # Test Ethereum mainnet
```

The script will:
1. ✅ Query a public RPC for active peers
2. ✅ Parse the enode string
3. ✅ Connect to the peer with eth_watch
4. ✅ Show connection results

## Quick Manual Test

If you want to test manually:

```bash
# 1. Get a live peer (these commands may take a few seconds)
PEER=$(curl -s -X POST https://sepolia.llamarpc.com \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"admin_peers","params":[],"id":1}' \
  | jq -r '.result[0].enode')

# 2. Extract components
PUBKEY=$(echo "$PEER" | sed 's/enode:\/\/\([^@]*\)@.*/\1/')
HOST=$(echo "$PEER" | sed 's/.*@\([^:]*\):.*/\1/')
PORT=$(echo "$PEER" | sed 's/.*:\([0-9]*\)$/\1/')

# 3. Connect
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/build/OSX/Debug
./eth_watch "$HOST" "$PORT" "$PUBKEY"
```

## Public RPC Endpoints (No Auth Required)

**Sepolia:**
- `https://sepolia.llamarpc.com`
- `https://1rpc.io/sepolia`

**Mainnet:**
- `https://eth.llamarpc.com`
- `https://1rpc.io/eth`

## Expected Output When Connecting to Real Peer

```
Connected. Waiting for messages...

⚠️  Note: Bootstrap nodes are for DISCOVERY ONLY (discv4 protocol)
    They will NOT send block data. To receive blocks:
    1. Use discv4 to discover real peer nodes, OR
    2. Connect directly to a full node (not a bootstrap node)

HELLO from peer: Geth/v1.13.0-...
Sent ETH Status message to peer
NewBlockHashes: 2 hashes
NewBlockHashes: 1 hash
...
```

## Files Created/Updated

```
 /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/SuperGenius/rlp/
├── PUBLIC_NODES_FOR_TESTING.md        (NEW - Complete reference guide)
├── test_eth_watch.sh                  (NEW - Automated test script)
├── WHY_NO_MESSAGES.md                 (Created earlier - explains bootstrap vs peers)
├── BOOTNODES_CONFIGURATION.md         (Updated - added peer discovery info)
└── build/OSX/Debug/eth_watch          (Binary ready to use)
```

## What Each File Does

- **PUBLIC_NODES_FOR_TESTING.md**: Reference for finding public nodes and peers
- **test_eth_watch.sh**: One-command test that does everything automatically
- **WHY_NO_MESSAGES.md**: Explains why bootstrap nodes don't send messages
- **BOOTNODES_CONFIGURATION.md**: Bootnode configs with clarifications

## Next Steps

1. **For Quick Testing**: Run `./test_eth_watch.sh sepolia`
2. **For Discovery Debugging**: Use the maintained C++ discovery harnesses under `examples/discovery/` (for example `test_discovery.cpp` and `test_enr_survey.cpp`)
3. **For Development**: Use a local Geth node with `--http --http.api admin,web3,eth,net`

---

All the infrastructure is now in place to connect to real peers and receive block data! 🚀

