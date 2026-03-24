#!/bin/bash
#
# examples/test_eth_watch.sh
#
# Functional end-to-end test for eth_watch + GNUS bridge on Ethereum Sepolia.
# Output is GTest-style with timing, pass/fail per test case.
# Debug logs are captured to a timestamped file — not shown unless a test fails.
#
# Setup:
#   Copy examples/.env.example to examples/.env and fill in PRIVATE_KEY.
#   The wallet must hold GNUS on Sepolia.
#   Install cast: brew install foundry
#
# Usage:
#   cd rlp && ./examples/test_eth_watch.sh
#
# Optional env overrides:
#   WATCH_TIMEOUT=180     seconds to wait for events (default 120)
#   RPC_SEPOLIA=https://  override the default public RPC

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}/.."
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_DIR="${SCRIPT_DIR}/logs"
mkdir -p "$LOG_DIR"
DEBUG_LOG="${LOG_DIR}/eth_watch_${TIMESTAMP}.log"
EVENT_LOG=$(mktemp "/tmp/eth_watch_events_XXXXXX")
mv "$EVENT_LOG" "${EVENT_LOG}.log" && EVENT_LOG="${EVENT_LOG}.log"

# ── Colors ────────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; DIM='\033[2m'; NC='\033[0m'

now_ms() { python3 -c "import time; print(int(time.time()*1000))"; }

# ── Test framework ────────────────────────────────────────────────────────────
SUITE_START=$(now_ms)
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
CURRENT_TEST=""
CURRENT_TEST_START=0

suite_header() {
    echo ""
    echo -e "${BOLD}[==========]${NC} eth_watch functional test suite"
    echo -e "${BOLD}[----------]${NC} Global test environment setup"
}

test_start() {
    CURRENT_TEST="$1"
    CURRENT_TEST_START=$(now_ms)
    TESTS_RUN=$((TESTS_RUN + 1))
    echo -e "${BOLD}${GREEN}[ RUN      ]${NC} ${CURRENT_TEST}"
}

test_pass() {
    local elapsed=$(( $(now_ms) - CURRENT_TEST_START ))
    TESTS_PASSED=$((TESTS_PASSED + 1))
    echo -e "${GREEN}[       OK ]${NC} ${CURRENT_TEST} (${elapsed} ms)"
    if [ -n "${1:-}" ]; then
        echo -e "            ${DIM}${1}${NC}"
    fi
}

test_fail() {
    local elapsed=$(( $(now_ms) - CURRENT_TEST_START ))
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo -e "${RED}[  FAILED  ]${NC} ${CURRENT_TEST} (${elapsed} ms)"
    if [ -n "${1:-}" ]; then
        echo -e "            ${RED}${1}${NC}"
    fi
}

test_skip() {
    echo -e "${YELLOW}[ SKIPPED  ]${NC} ${CURRENT_TEST} — ${1:-}"
}

info() { echo -e "            ${DIM}${1}${NC}"; }

suite_footer() {
    local elapsed=$(( $(now_ms) - SUITE_START ))
    echo ""
    echo -e "${BOLD}[==========]${NC} ${TESTS_RUN} test(s) ran (${elapsed} ms total)"
    if [ $TESTS_PASSED -gt 0 ]; then
        echo -e "${GREEN}[  PASSED  ]${NC} ${TESTS_PASSED} test(s)"
    fi
    if [ $TESTS_FAILED -gt 0 ]; then
        echo -e "${RED}[  FAILED  ]${NC} ${TESTS_FAILED} test(s)"
        echo ""
        echo -e "${RED}Debug log:${NC} ${DEBUG_LOG}"
    else
        echo -e "${DIM}Debug log: ${DEBUG_LOG}${NC}"
    fi
    echo ""
}

# ── Load .env ─────────────────────────────────────────────────────────────────
for envfile in "${SCRIPT_DIR}/.env" "${REPO_ROOT}/.env"; do
    if [ -f "$envfile" ]; then
        set -a; source "$envfile"; set +a
        break
    fi
done

# ── Config ────────────────────────────────────────────────────────────────────
WATCH_TIMEOUT=${WATCH_TIMEOUT:-120}
RPC="${RPC_SEPOLIA:-https://ethereum-sepolia-rpc.publicnode.com}"
CONTRACT="0x9af8050220D8C355CA3c6dC00a78B474cd3e3c70"
BRIDGE_AMOUNT="100000000000000000"   # 0.1 GNUS (1e17)
BRIDGE_TOKEN_ID="0"
BRIDGE_DEST_CHAIN="963"              # SuperGenius Testnet
TRANSFER_AMOUNT="1"

# ── Find eth_watch binary ─────────────────────────────────────────────────────
ETH_WATCH_BIN=""
for build_type in Debug Release RelWithDebInfo; do
    candidate="${REPO_ROOT}/build/OSX/${build_type}/examples/eth_watch/eth_watch"
    if [ -f "$candidate" ]; then
        ETH_WATCH_BIN="$candidate"
        ETH_WATCH_BUILD="$build_type"
        break
    fi
done

WATCH_PID=""
cleanup() {
    if [ -n "$WATCH_PID" ] && kill -0 "$WATCH_PID" 2>/dev/null; then
        kill "$WATCH_PID" 2>/dev/null
    fi
    rm -f "$EVENT_LOG"
    suite_footer
}
trap cleanup EXIT INT TERM

# ── Event formatter ───────────────────────────────────────────────────────────
format_event_block() {
    awk '
    function chain_name(id,    n) {
        n = id + 0
        if (n == 1)        return "Ethereum Mainnet"
        if (n == 11155111) return "Sepolia"
        if (n == 137)      return "Polygon"
        if (n == 56)       return "BSC"
        if (n == 8453)     return "Base"
        if (n == 80002)    return "Polygon Amoy"
        if (n == 97)       return "BSC Testnet"
        if (n == 84532)    return "Base Sepolia"
        if (n == 369)      return "SuperGenius Mainnet"
        if (n == 963)      return "SuperGenius Testnet"
        if (n == 144)      return "SuperGenius Devnet"
        return "chain " id
    }
    /Transfer\(address,address,uint256\) at block/ {
        split("from,to,value", fields, ","); in_event=1; field_count=3; print; next
    }
    /BridgeSourceBurned\(address,uint256,uint256,uint256,uint256\) at block/ {
        split("sender,id,amount,srcChainID,destChainID", fields, ","); in_event=1; field_count=5; print; next
    }
    in_event && /[[:space:]]*\[[0-9]+\]/ {
        match($0, /\[([0-9]+)\]/, m)
        idx = m[1] + 1
        fname = (idx <= field_count) ? fields[idx] : "?"
        if (fname == "srcChainID" || fname == "destChainID") {
            match($0, /uint256: ([0-9]+)/, v)
            sub(/\[[0-9]+\]/, "[" fname "]")
            print $0 "  (" chain_name(v[1]) ")"
        } else {
            sub(/\[[0-9]+\]/, "[" fname "]")
            print
        }
        next
    }
    { in_event=0; print }
    '
}

# ═════════════════════════════════════════════════════════════════════════════
suite_header
echo -e "${BOLD}[----------]${NC} 5 tests from EthWatchSepoliaTest"
echo ""

# ── Test 1: Preflight ─────────────────────────────────────────────────────────
test_start "EthWatchSepoliaTest.Preflight"
preflight_ok=true

if [ -z "$ETH_WATCH_BIN" ]; then
    test_fail "eth_watch binary not found under build/OSX/*/examples/eth_watch/"
    info "Build: cd build/OSX/Debug && cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug && ninja"
    preflight_ok=false
fi
if ! command -v cast &>/dev/null; then
    test_fail "cast (Foundry) not found — install: brew install foundry"
    preflight_ok=false
fi
if [ -z "${PRIVATE_KEY:-}" ] || ! echo "$PRIVATE_KEY" | grep -qE '^0x[0-9a-fA-F]{64}$'; then
    test_fail "PRIVATE_KEY not set or invalid in examples/.env"
    preflight_ok=false
fi

if $preflight_ok; then
    SENDER=$(cast wallet address "$PRIVATE_KEY" 2>/dev/null)
    test_pass "binary=${ETH_WATCH_BUILD}  sender=${SENDER}"
    info "Contract: ${CONTRACT}"
    info "RPC:      ${RPC}"
else
    exit 1
fi

echo ""

# ── Test 2: Peer Connection ───────────────────────────────────────────────────
test_start "EthWatchSepoliaTest.PeerConnection"

ASAN_OPTIONS=halt_on_error=0:replace_intrin=0:detect_stack_use_after_return=0:poison_heap=0 stdbuf -oL "$ETH_WATCH_BIN" \
    --chain sepolia \
    --log-level error \
    --watch-contract "$CONTRACT" --watch-event "Transfer(address,address,uint256)" \
    --watch-contract "$CONTRACT" --watch-event "BridgeSourceBurned(address,uint256,uint256,uint256,uint256)" \
    >> "$DEBUG_LOG" 2>&1 &
WATCH_PID=$!

# Also tee event lines to EVENT_LOG for detection
tail -f "$DEBUG_LOG" 2>/dev/null | grep -E "^(Transfer|BridgeSourceBurned|Approval)" >> "$EVENT_LOG" &
TAIL_PID=$!

ELAPSED=0
while [ $ELAPSED -lt 60 ]; do
    if grep -q "Connected\. Watching" "$DEBUG_LOG" 2>/dev/null; then
        peer=$(grep "HELLO from peer:" "$DEBUG_LOG" | tail -1 | sed 's/HELLO from peer: //')
        test_pass "$peer"
        break
    fi
    if ! kill -0 "$WATCH_PID" 2>/dev/null; then
        test_fail "eth_watch exited before connecting"
        kill "$TAIL_PID" 2>/dev/null || true
        exit 1
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done
if [ $ELAPSED -ge 60 ]; then
    test_fail "Timed out waiting for peer (60s)"
    kill "$TAIL_PID" 2>/dev/null || true
    exit 1
fi
kill "$TAIL_PID" 2>/dev/null || true

echo ""

# ── Test 3: ERC-20 Transfer TX ───────────────────────────────────────────────
test_start "EthWatchSepoliaTest.SendERC20Transfer"

transfer_out=$(cast send "$CONTRACT" \
    "transfer(address,uint256)" \
    "$SENDER" "$TRANSFER_AMOUNT" \
    --private-key "$PRIVATE_KEY" \
    --rpc-url "$RPC" \
    --timeout 30 2>&1 || true)
transfer_tx=$(echo "$transfer_out" | grep -o '0x[0-9a-fA-F]\{64\}' | head -1 || true)

if [ -n "$transfer_tx" ]; then
    test_pass "https://sepolia.etherscan.io/tx/${transfer_tx}"
else
    test_fail "$(echo "$transfer_out" | tail -2)"
fi

echo ""

# ── Test 4: Bridge TX ─────────────────────────────────────────────────────────
test_start "EthWatchSepoliaTest.SendBridgeOut"

bridge_out=$(cast send "$CONTRACT" \
    "bridgeOut(uint256,uint256,uint256)" \
    "$BRIDGE_AMOUNT" "$BRIDGE_TOKEN_ID" "$BRIDGE_DEST_CHAIN" \
    --private-key "$PRIVATE_KEY" \
    --rpc-url "$RPC" \
    --timeout 30 2>&1 || true)
bridge_tx=$(echo "$bridge_out" | grep -o '0x[0-9a-fA-F]\{64\}' | head -1 || true)

if [ -n "$bridge_tx" ]; then
    test_pass "https://sepolia.etherscan.io/tx/${bridge_tx}"
    info "0.1 GNUS (id=0) → SuperGenius Testnet (chain 963)"
else
    test_fail "$(echo "$bridge_out" | tail -2)"
fi

echo ""

# ── Test 5: Event Detection ───────────────────────────────────────────────────
test_start "EthWatchSepoliaTest.EventsDetected"
info "Waiting up to ${WATCH_TIMEOUT}s for on-chain confirmation and propagation..."

TRANSFER_DETECTED=false
BRIDGE_DETECTED=false
ELAPSED=0

while [ $ELAPSED -lt $WATCH_TIMEOUT ]; do
    if ! $TRANSFER_DETECTED && grep -q "^Transfer(address,address,uint256) at block" "$DEBUG_LOG" 2>/dev/null; then
        TRANSFER_DETECTED=true
        info "✓ Transfer event received:"
        grep -A4 "^Transfer(address,address,uint256) at block" "$DEBUG_LOG" | head -5 \
            | format_event_block | sed 's/^/              /'
    fi
    if ! $BRIDGE_DETECTED && grep -q "^BridgeSourceBurned(address,uint256,uint256,uint256,uint256) at block" "$DEBUG_LOG" 2>/dev/null; then
        BRIDGE_DETECTED=true
        info "✓ BridgeSourceBurned event received:"
        grep -A6 "^BridgeSourceBurned(address,uint256,uint256,uint256,uint256) at block" "$DEBUG_LOG" | head -7 \
            | format_event_block | sed 's/^/              /'
    fi
    $TRANSFER_DETECTED && $BRIDGE_DETECTED && break
    if ! kill -0 "$WATCH_PID" 2>/dev/null; then
        break
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

if $TRANSFER_DETECTED && $BRIDGE_DETECTED; then
    test_pass "Transfer ✓  BridgeSourceBurned ✓"
elif $TRANSFER_DETECTED; then
    test_fail "Transfer ✓  BridgeSourceBurned ✗ (not seen within ${WATCH_TIMEOUT}s — TX may still confirm)"
elif $BRIDGE_DETECTED; then
    test_fail "Transfer ✗  BridgeSourceBurned ✓"
else
    test_fail "No events detected within ${WATCH_TIMEOUT}s"
fi

# ── Exit code ─────────────────────────────────────────────────────────────────
[ $TESTS_FAILED -eq 0 ] && EXIT_CODE=0 || EXIT_CODE=1

# disable trap so suite_footer runs cleanly from here
trap - EXIT
cleanup
exit $EXIT_CODE
