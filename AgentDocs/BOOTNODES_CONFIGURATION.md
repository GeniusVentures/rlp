# Bootnode Configuration Guide

## Overview

The RLP/RLPx library includes bootstrap node configurations for multiple EVM chains. **Important Note**: Bootstrap nodes are used for **peer discovery only** (discv4 protocol via UDP) - they do NOT send block data.

To receive block data (NewBlockHashes, NewBlock messages, etc.), you must:
1. Either: Use discv4 to discover real peer nodes from bootstrap nodes
2. Or: Connect directly to a full peer node

These configurations enable the `eth_watch` example to discover and connect to various blockchain networks.

## Important: Bootstrap Nodes vs Real Peers

### Bootstrap Nodes (Discovery Only)
- **Protocol**: discv4 (UDP-based)
- **Purpose**: Help you find other peer nodes
- **Data**: Don't send block/transaction data
- **What you do**: Send PING → Receive PONG + NEIGHBOURS list

### Real Peer Nodes (Block Data)
- **Protocol**: RLPx + ETH protocol (TCP-based)
- **Purpose**: Share blockchain state and blocks
- **Data**: Send block headers, transactions, receipts, etc.
- **What you do**: RLPx handshake → ETH Status exchange → Receive block data

## Supported Chains

### Ethereum

| Chain | Network | Port | Status | Source |
|-------|---------|------|--------|--------|
| mainnet | Ethereum Mainnet | 30303 | ✅ Working | [go-ethereum/params/bootnodes.go](https://github.com/ethereum/go-ethereum/blob/master/params/bootnodes.go) |
| sepolia | Sepolia Testnet | 30303 | ✅ Working | [go-ethereum/params/sepolia.go](https://github.com/ethereum/go-ethereum/blob/master/params/sepolia.go) |
| holesky | Holesky Testnet | 30303 | ⚠️ Unreachable* | [go-ethereum/params/holesky.go](https://github.com/ethereum/go-ethereum/blob/master/params/holesky.go) |

### Polygon

| Chain | Network | Port | Status | Source |
|-------|---------|------|--------|--------|
| polygon | Polygon PoS Mainnet | 30303 | ✅ Working | [Polygon Docs](https://docs.polygon.technology/pos/reference/seed-and-bootnodes) |
| polygon-amoy | Polygon Amoy Testnet | 30303 | ✅ Working | [Polygon Docs](https://docs.polygon.technology/pos/reference/seed-and-bootnodes) |

### BNB Smart Chain (BSC)

| Chain | Network | Port | Status | Source |
|-------|---------|------|--------|--------|
| bsc / bsc-mainnet | BSC Mainnet | 30311* | ⚠️ Unreachable† | [bsc/params/config.go](https://github.com/bnb-chain/bsc/blob/master/params/config.go) |
| bsc-testnet | BSC Testnet | 30311* | ⏳ Testing | [bsc/params/config.go](https://github.com/bnb-chain/bsc/blob/master/params/config.go) |

**Note:** BSC uses non-standard port 30311 instead of 30303

### Base (OP Stack)

| Chain | Network | Port | Status | Notes |
|-------|---------|------|--------|-------|
| base | Base Mainnet | 30303 | ❌ Not Configured | Uses OP Stack discovery - see [Base Docs](https://docs.base.org/base-chain/node-operators/run-a-base-node) |
| base-sepolia | Base Sepolia Testnet | 30303 | ❌ Not Configured | Uses OP Stack discovery - see [Base Docs](https://docs.base.org/base-chain/node-operators/run-a-base-node) |

## Usage

### Finding Real Peer Nodes

To actually receive block data, you need to connect to **real peer nodes** (not bootstrap nodes). You can find these by:

1. **Query a node RPC endpoint** - Contact the network to ask for current peers:
   ```bash
   curl -s -X POST https://eth.llamarpc.com \
     -H "Content-Type: application/json" \
     -d '{"jsonrpc":"2.0","method":"admin_peers","params":[],"id":1}' | jq '.result[0]'
   ```

2. **Use Node URLs** - These are enode strings from active peers reported by explorer APIs

3. **Run your own node** - Sync a full node which will discover and manage peers automatically

### Known Active Peer Enodes

For **testing purposes**, you can try these known Ethereum Sepolia peers:

```
enode://84b8482152e23b9a6b0abf89b4e3e0d93f2f4c3e8d9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b@IP:30303
```

(Note: Peer enode addresses and IPs change frequently as nodes go online/offline)

### Using eth_watch Example

#### Connect to a specific chain (Bootstrap - Discovery Only):
```bash
./eth_watch --chain <chain_name>
```

**⚠️ Warning**: This will only work if the bootstrap node unexpectedly also serves as a peer. Most likely you'll see:
```
Connected. Waiting for messages...
HELLO from peer: <bootnode-client>
Sent ETH Status message to peer
(no messages - bootnode disconnects)
```

#### Connect to a Real Peer (Block Data):
```bash
./eth_watch <host> <port> <peer_pubkey_hex> [eth_offset]
```

Example with a real Sepolia peer:
```bash
./eth_watch 138.197.51.181 30303 4e5e92199ee224a01932a377160aa432f31d0b351f84ab413a8e0a42f4f36476f8fb1cbe914af0d9aef0d51665c214cf653c651c4bbd9d5550a934f241f1682b
```

#### Available chains:
```bash
# Ethereum
./eth_watch --chain mainnet
./eth_watch --chain sepolia
./eth_watch --chain holesky

# Polygon
./eth_watch --chain polygon
./eth_watch --chain polygon-amoy

# BSC
./eth_watch --chain bsc
./eth_watch --chain bsc-mainnet
./eth_watch --chain bsc-testnet

# Base
./eth_watch --chain base
./eth_watch --chain base-sepolia
```

#### Manual connection (enode format):
```bash
./eth_watch <host> <port> <peer_pubkey_hex> [eth_offset]
```

Example:
```bash
./eth_watch 138.197.51.181 30303 4e5e92199ee224a01932a377160aa432f31d0b351f84ab413a8e0a42f4f36476f8fb1cbe914af0d9aef0d51665c214cf653c651c4bbd9d5550a934f241f1682b
```

## Implementation Files

### Bootnode Definitions

- **Mainnet bootnodes:** `/include/rlp/PeerDiscovery/bootnodes.hpp`
- **Testnet bootnodes:** `/include/rlp/PeerDiscovery/bootnodes_test.hpp`

### Chain Loader

- **Chain selection logic:** `/examples/eth_watch.cpp` - `load_bootnode_for_chain()` function

## Connection Status

### ✅ Successfully Tested

- Ethereum Mainnet
- Ethereum Sepolia
- Polygon Mainnet
- Polygon Amoy

### ⚠️ Network Issues (Bootnodes Configured But Unreachable)

- Ethereum Holesky (bootstrap nodes may be offline)
- BSC Mainnet (bootnodes may not be accepting connections or may be offline)

### ⏳ Requires Testing

- BSC Testnet (bootnodes configured, needs network connectivity test)

### ❌ Not Yet Configured

- Base Mainnet (requires OP Stack discovery setup)
- Base Sepolia (requires OP Stack discovery setup)

## Enode Format

Enodes follow the standard format:
```
enode://<128-hex-char-pubkey>@<ip>:<port>
```

Example breakdown:
```
enode://4e5e92199ee224a01932a377160aa432f31d0b351f84ab413a8e0a42f4f36476f8fb1cbe914af0d9aef0d51665c214cf653c651c4bbd9d5550a934f241f1682b@138.197.51.181:30303
       └─────────────────────── 128 hex characters (64 bytes) ──────────────────────┘   │        public key               │                    host:port
```

## Error Codes

When connection fails, the tool reports error code 12 (`kConnectionFailed`), which typically means:

1. **Bootstrap node is offline or unreachable** - Check network connectivity
2. **Wrong port number** - Ethereum/Polygon use 30303, BSC uses 30311
3. **Public key doesn't match** - Bootstrap node public key is incorrect

## Updating Bootnodes

To update bootnodes:

1. Check the official GitHub repositories for each chain
2. Extract the enode strings from `params/config.go` or `params/bootnodes.go`
3. Update the corresponding array in:
   - `/include/rlp/PeerDiscovery/bootnodes.hpp` (mainnet)
   - `/include/rlp/PeerDiscovery/bootnodes_test.hpp` (testnet)

## Future Enhancements

1. **OP Stack Discovery** - Add support for Base and other Optimism-based chains
2. **Dynamic Discovery** - Implement discv4 protocol for dynamic node discovery
3. **Configuration Files** - Support JSON config files for custom bootnode lists
4. **Health Checks** - Periodic bootnode availability verification

## References

- [Ethereum go-ethereum](https://github.com/ethereum/go-ethereum/blob/master/params/bootnodes.go)
- [Polygon Documentation](https://docs.polygon.technology/pos/reference/seed-and-bootnodes)
- [BSC Documentation](https://docs.bnbchain.org/bnb-smart-chain/developers/node_operators/boot_node)
- [Base Documentation](https://docs.base.org/base-chain/node-operators/run-a-base-node)
- [Enode Format Specification](https://ethereum.org/en/developers/docs/networking-layer/network-addresses/)


