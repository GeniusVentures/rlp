#!/bin/bash
#
# test_eth_watch.sh - Live connection test for eth_watch
#
# Uses discv4 for peer discovery (bootnodes → full nodes) and RLPx for
# the encrypted ETH connection. No RPC required.
#
# Usage: ./test_eth_watch.sh [sepolia|mainnet|gnus-sepolia|gnus-mainnet]
#

set -e

CHAIN=${1:-sepolia}
EXTRA_ARGS="${@:2}"
TIMEOUT=30
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ETH_WATCH_BIN="${SCRIPT_DIR}/build/OSX/Debug/examples/eth_watch/eth_watch"

# GNUS.AI contract addresses
GNUS_SEPOLIA="0x9af8050220D8C355CA3c6dC00a78B474cd3e3c70"
GNUS_MAINNET="0x614577036F0a024DBC1C88BA616b394DD65d105a"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status()  { echo -e "${BLUE}[*]${NC} $1"; }
print_success() { echo -e "${GREEN}[✓]${NC} $1"; }
print_error()   { echo -e "${RED}[✗]${NC} $1"; }

# Check binary
if [ ! -f "$ETH_WATCH_BIN" ]; then
    print_error "eth_watch binary not found at: $ETH_WATCH_BIN"
    print_status "Building eth_watch..."
    cd "${SCRIPT_DIR}/build/OSX/Debug"
    ninja eth_watch
    cd - > /dev/null
fi

# Map mode to --chain arg and optional watch flags
WATCH_FLAGS=""
case "$CHAIN" in
    mainnet|main|eth)
        CHAIN_ARG="mainnet"
        CHAIN_NAME="Ethereum Mainnet"
        ;;
    sepolia)
        CHAIN_ARG="sepolia"
        CHAIN_NAME="Ethereum Sepolia"
        ;;
    gnus-sepolia)
        CHAIN_ARG="sepolia"
        CHAIN_NAME="Ethereum Sepolia — GNUS contract"
        WATCH_FLAGS="--watch-contract ${GNUS_SEPOLIA} --watch-event Transfer(address,address,uint256) \
                     --watch-contract ${GNUS_SEPOLIA} --watch-event Approval(address,address,uint256)"
        ;;
    gnus-mainnet)
        CHAIN_ARG="mainnet"
        CHAIN_NAME="Ethereum Mainnet — GNUS contract"
        WATCH_FLAGS="--watch-contract ${GNUS_MAINNET} --watch-event Transfer(address,address,uint256) \
                     --watch-contract ${GNUS_MAINNET} --watch-event Approval(address,address,uint256)"
        ;;
    *)
        print_error "Unknown chain: $CHAIN"
        echo "Usage: $0 [sepolia|mainnet|gnus-sepolia|gnus-mainnet]"
        exit 1
        ;;
esac

print_status "$CHAIN_NAME"
print_status "Connecting via discv4 + RLPx (timeout: ${TIMEOUT}s)..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# shellcheck disable=SC2086
if timeout $TIMEOUT "$ETH_WATCH_BIN" --chain "$CHAIN_ARG" $WATCH_FLAGS $EXTRA_ARGS 2>&1; then
    RESULT=$?
else
    RESULT=$?
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if [ $RESULT -eq 124 ]; then
    print_success "Timeout reached — connection was live"
    echo ""
    print_status "What to look for:"
    echo "  'HELLO from peer: Geth/...'         → RLPx handshake worked"
    echo "  'ETH STATUS: network_id=11155111'   → ETH protocol accepted (Sepolia)"
    echo "  'ETH STATUS: network_id=1'          → ETH protocol accepted (Mainnet)"
    echo "  'NewBlockHashes: N hashes'          → Peer is sending block data"
    echo "  'Transfer(...) at block N'          → GNUS event decoded successfully"
    exit 0
elif [ $RESULT -eq 0 ]; then
    print_success "Completed"
    exit 0
else
    print_error "Exited with code $RESULT"
    exit $RESULT
fi

