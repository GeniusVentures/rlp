# eth_watch example

## Usage

```text
eth_watch <host> <port> <peer_pubkey_hex> [eth_offset]
eth_watch --chain <mainnet|sepolia>
```

## Arguments

- `host`: peer host/IP to connect to.
- `port`: peer TCP port.
- `peer_pubkey_hex`: 128 hex chars (64-byte uncompressed public key, no 0x04 prefix).
- `eth_offset`: optional subprotocol offset for `eth` (default `0x10`).

## Chain presets

The `--chain` flag uses bootnodes from:
- `include/rlp/PeerDiscovery/bootnodes.hpp` (mainnet)
- `include/rlp/PeerDiscovery/bootnodes_test.hpp` (sepolia)
