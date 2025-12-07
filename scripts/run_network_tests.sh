#!/bin/bash
# Copyright 2025 GeniusVentures
# SPDX-License-Identifier: Apache-2.0
#
# Local network connectivity test runner for Linux/macOS
# Tests P2P port accessibility to EVM testnets

set -e  # Exit on error

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================"
echo "  RLP Network Connectivity Tests"
echo -e "========================================${NC}"
echo

# Detect platform
PLATFORM=$(uname -s)
if [[ "$PLATFORM" == "Darwin" ]]; then
    BUILD_DIR="build/OSX/Release"
elif [[ "$PLATFORM" == "Linux" ]]; then
    BUILD_DIR="build/Linux/Release"
else
    echo -e "${RED}ERROR: Unsupported platform: $PLATFORM${NC}"
    exit 1
fi

# Check if build exists
if [ ! -f "$BUILD_DIR/network_port_test" ]; then
    echo -e "${RED}ERROR: Tests not built yet!${NC}"
    echo
    echo "Build the tests first:"
    echo "  1. cd $BUILD_DIR"
    echo "  2. cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_INTEGRATION_TESTS=ON ../../.."
    echo "  3. ninja"
    echo
    exit 1
fi

# Run diagnostics first
echo -e "${YELLOW}[STEP 1/3] Running network diagnostics...${NC}"
echo
if [ -f "$BUILD_DIR/network_diagnostics" ]; then
    "$BUILD_DIR/network_diagnostics" --full
    echo
else
    echo -e "${YELLOW}WARNING: network_diagnostics not found, skipping...${NC}"
    echo
fi

# Run port connectivity tests
echo -e "${YELLOW}[STEP 2/3] Testing TCP/UDP port connectivity...${NC}"
echo
"$BUILD_DIR/network_port_test" --gtest_color=yes
TEST_RESULT=$?

echo
echo -e "${BLUE}========================================"
echo "  Test Results Summary"
echo -e "========================================${NC}"
echo

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}[PASS] All network tests passed!${NC}"
    echo
    echo "You can proceed with full P2P integration tests."
else
    echo -e "${YELLOW}[WARN] Some tests failed or were skipped${NC}"
    echo
    echo "Common issues:"
    echo "  - Corporate firewall blocking ports 30303 or 9222"
    echo "  - VPN interference"
    echo "  - iptables/firewalld rules blocking outbound connections"
    echo
    echo "Troubleshooting steps:"
    echo "  1. Check firewall status:"
    echo "     sudo iptables -L -n -v"
    echo "     sudo ufw status"
    echo "  2. Temporarily disable firewall for testing:"
    echo "     sudo ufw disable"
    echo "  3. Try different network (e.g., mobile hotspot)"
    echo "  4. Review dmesg for network errors:"
    echo "     sudo dmesg | grep -i network"
    echo
fi

# Generate test report
echo -e "${YELLOW}[STEP 3/3] Generating test report...${NC}"
echo
echo "Test execution completed at: $(date)"
echo "Build directory: $BUILD_DIR"
echo

# Create results directory
mkdir -p test_results

# Save test output
echo "Saving results to test_results/network_test_results.txt"
"$BUILD_DIR/network_port_test" --gtest_color=no > test_results/network_test_results.txt 2>&1

echo
echo -e "${BLUE}========================================"
echo "Full results saved to: test_results/network_test_results.txt"
echo -e "========================================${NC}"
echo

# Additional system info
echo -e "${YELLOW}System Information:${NC}"
echo "  Platform: $PLATFORM"
echo "  Hostname: $(hostname)"
echo "  Network interfaces:"
if [[ "$PLATFORM" == "Darwin" ]]; then
    ifconfig | grep "inet " | grep -v 127.0.0.1
elif [[ "$PLATFORM" == "Linux" ]]; then
    ip addr show | grep "inet " | grep -v 127.0.0.1
fi
echo

exit $TEST_RESULT
