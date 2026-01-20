# eth_watch example

## Usage

```text
eth_watch <host> <port> <peer_pubkey_hex> [eth_offset]
eth_watch --config <path_to_json>
eth_watch --chain <mainnet|sepolia>
```

## Arguments

- `host`: peer host/IP to connect to.
- `port`: peer TCP port.
- `peer_pubkey_hex`: 128 hex chars (64-byte uncompressed public key, no 0x04 prefix).
- `eth_offset`: optional subprotocol offset for `eth` (default `0x10`).

## JSON config format

```json
{
  "host": "127.0.0.1",
  "port": 30303,
  "peer_pubkey_hex": "<128 hex chars>",
  "eth_offset": 16
}
```

## Chain presets

The `--chain` flag loads `examples/config/<chain>.json`.
Populate the files with a reachable peer and its public key.

