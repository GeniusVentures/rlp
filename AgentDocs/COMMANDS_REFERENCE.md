# Quick Commands Reference

## Manual Steps

### Step 1: Get a Live Peer
```bash
# Sepolia
curl -s https://sepolia.llamarpc.com -X POST \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"admin_peers","params":[],"id":1}' \
  | jq '.result[0]'
```

### Step 2: Parse the Enode
```bash
# Set PEER to the enode string from above, then:
PEER="enode://PUBKEY@IP:PORT"

PUBKEY=$(echo "$PEER" | sed 's/enode:\/\/\([^@]*\)@.*/\1/')
HOST=$(echo "$PEER" | sed 's/.*@\([^:]*\):.*/\1/')
PORT=$(echo "$PEER" | sed 's/.*:\([0-9]*\)$/\1/')

echo "Host: $HOST, Port: $PORT, Pubkey: $PUBKEY"
```

### Step 3: Connect
```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/rlp/build/OSX/Debug
./eth_watch "$HOST" "$PORT" "$PUBKEY"
```

## Useful Commands

### Clean build
```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/rlp/build/OSX/Debug
ninja clean && ninja eth_watch
```

### Run all tests
```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/rlp/build/OSX/Debug
ninja test
```

### Run specific test
```bash
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/rlp/build/OSX/Debug
./rlp_decoder_tests
```

### Using bootstrap nodes (for reference)
```bash
# These won't send block data, but will connect
cd /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/rlp/build/OSX/Debug
./eth_watch --chain sepolia    # Uses bootstrap node (no messages)
./eth_watch --chain mainnet    # Uses bootstrap node (no messages)
./eth_watch --chain polygon    # Uses bootstrap node (no messages)
```

## Common Issues

### "Failed to connect"
- Bootstrap nodes were used
- Try using `./test_eth_watch.sh` to get a real peer instead

### "Connected but no messages"
- Check if using a bootstrap node with `--chain` flag
- Use real peer enodes from `./test_eth_watch.sh`

### "HELLO from peer but still no messages"
- Some peers may not actively broadcast blocks
- Try different peer with `./test_eth_watch.sh`

## File Locations

```
Project Root: /Users/Shared/SSDevelopment/Development/GeniusVentures/GeniusNetwork/rlp/

Key Files:
- ./test_eth_watch.sh               (Automated test)
- ./QUICK_TEST_GUIDE.md             (This file)
- ./PUBLIC_NODES_FOR_TESTING.md     (RPC endpoints & peer getting)
- ./WHY_NO_MESSAGES.md              (Bootstrap vs real peers explanation)
- ./build/OSX/Debug/eth_watch       (Executable binary)

Source Code:
- ./include/eth/messages.hpp        (ETH protocol messages)
- ./include/eth/eth_types.hpp       (Message types)
- ./include/rlpx/                   (RLPx protocol)
- ./examples/eth_watch.cpp          (eth_watch source)
```

## Resources

- **Ethereum Execution Spec**: https://github.com/ethereum/execution-specs
- **devp2p Specs**: https://github.com/ethereum/devp2p
- **RLPx**: https://github.com/ethereum/devp2p/blob/master/rlpx.md
- **ETH Protocol**: https://github.com/ethereum/devp2p/blob/master/caps/eth.md
- **discv4**: https://github.com/ethereum/devp2p/blob/master/discv4.md


