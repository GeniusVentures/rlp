#!/bin/bash
#
# test_eth_watch.sh - Automated test script for eth_watch
# Gets a live peer from public RPC and connects
#
# Usage: ./test_eth_watch.sh [mainnet|sepolia]
#

set -e

# Configuration
CHAIN=${1:-sepolia}
TIMEOUT=15
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ETH_WATCH_BIN="${SCRIPT_DIR}/build/OSX/Debug/eth_watch"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[*]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[!]${NC} $1"
}

# Check if eth_watch binary exists
if [ ! -f "$ETH_WATCH_BIN" ]; then
    print_error "eth_watch binary not found at: $ETH_WATCH_BIN"
    print_status "Building eth_watch..."
    cd "${SCRIPT_DIR}/build/OSX/Debug"
    ninja eth_watch
    cd - > /dev/null
fi

# Select RPC endpoint based on chain
case "$CHAIN" in
    mainnet|main|eth)
        RPC="https://eth.llamarpc.com"
        CHAIN_NAME="Ethereum Mainnet"
        ;;
    sepolia|sept)
        RPC="https://sepolia.llamarpc.com"
        CHAIN_NAME="Ethereum Sepolia Testnet"
        ;;
    *)
        print_error "Unknown chain: $CHAIN"
        echo "Usage: $0 [mainnet|sepolia]"
        exit 1
        ;;
esac

print_status "Testing eth_watch with $CHAIN_NAME"
echo ""

# Get peer from RPC
print_status "Querying RPC for active peers..."
RESPONSE=$(curl -s -X POST "$RPC" \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"admin_peers","params":[],"id":1}' \
  --max-time 10)

# Check if request succeeded
if [ -z "$RESPONSE" ]; then
    print_error "No response from RPC endpoint"
    exit 1
fi

# Extract enode
ENODE=$(echo "$RESPONSE" | jq -r '.result[0].enode' 2>/dev/null || echo "null")

if [ -z "$ENODE" ] || [ "$ENODE" = "null" ]; then
    print_error "Failed to get peer enode from RPC"
    print_status "RPC response:"
    echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"
    exit 1
fi

print_success "Got peer: $ENODE"
echo ""

# Parse enode://PUBKEY@HOST:PORT
PUBKEY=$(echo "$ENODE" | sed 's/enode:\/\/\([^@]*\)@.*/\1/')
HOST=$(echo "$ENODE" | sed 's/.*@\([^:]*\):.*/\1/')
PORT=$(echo "$ENODE" | sed 's/.*:\([0-9]*\)$/\1/')

# Validate parsing
if [ -z "$PUBKEY" ] || [ -z "$HOST" ] || [ -z "$PORT" ]; then
    print_error "Failed to parse enode"
    print_status "Parsed values:"
    echo "  Pubkey: $PUBKEY"
    echo "  Host: $HOST"
    echo "  Port: $PORT"
    exit 1
fi

# Display connection info
print_success "Parsed enode successfully"
echo ""
print_status "Connection details:"
echo "  Chain: $CHAIN_NAME"
echo "  Host: $HOST"
echo "  Port: $PORT"
echo "  Pubkey: ${PUBKEY:0:32}..."
echo ""

# Connect with eth_watch
print_status "Connecting to peer (timeout: ${TIMEOUT}s)..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if timeout $TIMEOUT "$ETH_WATCH_BIN" "$HOST" "$PORT" "$PUBKEY" 2>&1; then
    RESULT=$?
else
    RESULT=$?
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if [ $RESULT -eq 124 ]; then
    print_success "Timeout reached (expected - waiting for messages)"
    echo ""
    print_status "Test interpretation:"
    echo "  If you saw 'Connected. Waiting for messages...' → Working! ✓"
    echo "  If you saw 'HELLO from peer' → Protocol negotiation worked! ✓"
    echo "  If no messages arrived → This peer isn't actively sending (normal)"
    exit 0
elif [ $RESULT -eq 0 ]; then
    print_success "Test completed successfully"
    exit 0
else
    print_error "Test failed with exit code $RESULT"
    exit $RESULT
fi

