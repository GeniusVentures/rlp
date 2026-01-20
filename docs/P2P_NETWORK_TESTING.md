# P2P Network Testing Guide

## Overview

This guide explains how to test network connectivity to EVM testnets (Sepolia, Polygon Amoy, Base Sepolia) before attempting full P2P integration. These tests verify that your local environment or CI/CD pipeline can reach bootnode endpoints.

---

## Quick Start

### Windows

```cmd
cd h:\code\genius\rlp
scripts\run_network_tests.bat
```

### Linux/macOS

```bash
cd /path/to/rlp
chmod +x scripts/run_network_tests.sh
./scripts/run_network_tests.sh
```

---

## What Gets Tested

### 1. **Network Diagnostics** ([network_diagnostics.cpp](../test/integration/network_diagnostics.cpp))
- Local network interface enumeration
- DNS resolution to bootnode addresses
- Basic outbound TCP connectivity
- Firewall detection hints

### 2. **Port Connectivity Tests** ([network_port_test.cpp](../test/integration/network_port_test.cpp))
- **TCP connectivity** to ports 30303 (Sepolia/Amoy) and 9222 (Base Sepolia)
- **UDP connectivity** to the same ports (for Discv4 peer discovery)
- Latency measurements
- Firewall impact analysis

### 3. **Test Networks**

| Network | Chain ID | Host | TCP Port | UDP Port |
|---------|----------|------|----------|----------|
| **Sepolia-NYC** | 11155111 | 138.197.51.181 | 30303 | 30303 |
| **Sepolia-SFO** | 11155111 | 146.190.1.103 | 30303 | 30303 |
| **Sepolia-Sydney** | 11155111 | 170.64.250.88 | 30303 | 30303 |
| **Amoy-GCP-1** | 80002 | 35.197.249.21 | 30303 | 30303 |
| **Amoy-GCP-2** | 80002 | 34.89.39.114 | 30303 | 30303 |
| **Base-Sepolia** | 84532 | op-sepolia-bootnode-1.optimism.io | 9222 | 9222 |

---

## Building the Tests

### Prerequisites

- CMake 3.20+
- Ninja or Visual Studio 2022
- Boost.Asio (included in dependencies)
- C++17 compiler

### Build Steps

#### Windows (Visual Studio)

```cmd
cd build\Windows\Release
cmake -G "Visual Studio 17 2022" -A x64 -DENABLE_INTEGRATION_TESTS=ON -S ..
cmake --build . --config Release
```

#### Linux

```bash
cd build/Linux/Release
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_INTEGRATION_TESTS=ON -S ../../..
ninja
```

#### macOS

```bash
cd build/OSX/Release
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_INTEGRATION_TESTS=ON -S ../../..
ninja
```

---

## Running Tests Manually

### Run All Tests

```bash
# Linux/macOS
./build/Linux/Release/network_port_test

# Windows
build\Windows\Release\network_port_test.exe
```

### Run Specific Network Test

```bash
# Test only Sepolia TCP connectivity
./network_port_test --gtest_filter=*Sepolia*TcpConnectivity

# Test only UDP connectivity
./network_port_test --gtest_filter=*UdpConnectivity
```

### Run Diagnostics Tool

```bash
# Linux/macOS
./build/Linux/Release/network_diagnostics --full

# Windows
build\Windows\Release\network_diagnostics.exe --full
```

---

## Interpreting Results

### Success Output

```
=== Testing TCP connectivity to Sepolia-NYC (138.197.51.181:30303) ===
TCP connection ✓ SUCCESS in 45ms

=== Testing UDP connectivity to Sepolia-NYC (138.197.51.181:30303) ===
UDP send ✓ SUCCESS in 23ms
```

**Meaning**: Your environment can reach the bootnode. P2P integration should work.

### Failure Output

```
=== Testing TCP connectivity to Sepolia-NYC (138.197.51.181:30303) ===
TCP connection ✗ FAILED after 5012ms
```

**Meaning**: Firewall or network issue blocking access. See troubleshooting below.

### Partial Success (Common)

```
TCP connection ✓ SUCCESS in 45ms
UDP send ✗ FAILED after 5000ms
⚠ Note: UDP may be blocked by firewall (common in corporate/CI environments)
```

**Meaning**: TCP works, but UDP discovery is blocked. You can still use P2P with TCP-only fallback mode (skip Discv4, connect directly to bootnodes via enode URLs).

---

## Troubleshooting

### Issue: All TCP Tests Fail

**Possible Causes**:
- Corporate firewall blocking outbound connections
- VPN interfering with routing
- DNS resolution failure
- No internet connectivity

**Solutions**:
1. **Check firewall status**:
   ```bash
   # Windows
   netsh advfirewall show allprofiles state

   # Linux
   sudo iptables -L -n -v
   sudo ufw status
   ```

2. **Temporarily disable firewall** (for testing only):
   ```bash
   # Windows (run as admin)
   netsh advfirewall set allprofiles state off

   # Linux
   sudo ufw disable
   ```

3. **Test with different network** (e.g., mobile hotspot)

4. **Check DNS resolution**:
   ```bash
   nslookup 138.197.51.181
   ping 138.197.51.181
   ```

### Issue: UDP Tests Fail (TCP Works)

**Possible Causes**:
- Corporate firewall blocking UDP (very common)
- NAT/router configuration
- ISP filtering

**Solutions**:
1. **Use TCP-only mode**: P2P can work without UDP discovery by connecting directly to bootnode TCP endpoints
2. **Verify UDP not blocked**:
   ```bash
   # Linux - send test UDP packet
   nc -u 138.197.51.181 30303
   ```
3. **GitHub Actions**: This is expected - many CI environments block UDP. Use TCP fallback.

### Issue: Base Sepolia Fails (Others Work)

**Possible Causes**:
- DNS resolution failure for `op-sepolia-bootnode-1.optimism.io`
- Port 9222 blocked (different from standard 30303)

**Solutions**:
1. **Check DNS**:
   ```bash
   nslookup op-sepolia-bootnode-1.optimism.io
   ```
2. **Try alternative port**: Base Sepolia uses OP Stack P2P (port 9222). Ensure firewall allows this port.

### Issue: Intermittent Failures

**Possible Causes**:
- Bootnode temporarily down
- Network congestion
- Timeout too aggressive (5 seconds default)

**Solutions**:
1. **Increase timeout** in [network_port_test.cpp](../test/integration/network_port_test.cpp):
   ```cpp
   std::chrono::seconds timeout{10};  // Increase from 5 to 10 seconds
   ```
2. **Retry with different bootnode**: Tests automatically try multiple bootnodes per network

---

## GitHub Actions Integration

### Expected Behavior in CI

**Typical CI environment**:
- ✅ TCP outbound: Works (ports 30303, 9222)
- ⚠️ UDP outbound: Often blocked
- ✅ DNS resolution: Works
- ⚠️ ICMP ping: Often blocked

### CI-Specific Configuration

The tests are designed to be CI-friendly:
- **Graceful degradation**: Skips UDP tests if blocked (doesn't fail build)
- **Multiple bootnode fallback**: Tries 3-5 bootnodes per network
- **Timeout handling**: 5-second timeout prevents CI hangs
- **Non-blocking failures**: Uses `GTEST_SKIP()` instead of `EXPECT_TRUE()` for network issues

### Sample GitHub Actions Workflow

```yaml
name: Network Port Tests

on: [push, pull_request]

jobs:
  network-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Build tests
        run: |
          cmake -B build/Linux/Release -G Ninja \
            -DENABLE_INTEGRATION_TESTS=ON
          ninja -C build/Linux/Release

      - name: Run network tests
        run: |
          cd build/Linux/Release
          ./network_port_test --gtest_color=yes
```

---

## Advanced Usage

### Custom Bootnode Testing

Edit [network_port_test.cpp](../test/integration/network_port_test.cpp) to add your own bootnodes:

```cpp
const std::vector<NetworkEndpoint> kTestNetworks = {
    {"MyCustomNode", "1.2.3.4", 30303, 30303, std::chrono::seconds(5)},
    // ... existing networks
};
```

### Detailed Logging

Enable verbose output:

```bash
./network_port_test --gtest_color=yes 2>&1 | tee network_test_verbose.log
```

### Performance Benchmarking

The tests measure connection latency. To track performance over time:

```bash
# Extract timing data
grep "SUCCESS in" test_results/network_test_results.txt

# Example output:
# TCP connection ✓ SUCCESS in 45ms
# UDP send ✓ SUCCESS in 23ms
```

---

## What's Next

After verifying network connectivity:

1. **Proceed to P2P integration tests**: Full RLPx handshake and HELLO message exchange
2. **Review [Architecture.md](../Architecture.md)**: Understand the full P2P stack
3. **Check existing implementation**:
   - [rlpx::RlpxSession](../include/rlpx/rlpx_session.hpp) - RLPx protocol
   - [rlp::PeerDiscovery](../include/rlp/PeerDiscovery/discovery.hpp) - Discv4 discovery

---

## Test File Reference

| File | Purpose | Executable |
|------|---------|------------|
| [network_port_test.cpp](../test/integration/network_port_test.cpp) | Parameterized tests for TCP/UDP connectivity | `network_port_test` |
| [network_diagnostics.cpp](../test/integration/network_diagnostics.cpp) | System diagnostics tool | `network_diagnostics` |
| [run_network_tests.sh](../scripts/run_network_tests.sh) | Linux/macOS test runner | Shell script |
| [run_network_tests.bat](../scripts/run_network_tests.bat) | Windows test runner | Batch script |

---

## FAQ

**Q: Why test network connectivity separately from P2P integration?**
A: Network issues (firewall, DNS) often cause P2P failures. These simple tests quickly identify network problems before debugging complex protocol issues.

**Q: Can I run these tests on GitHub Actions?**
A: Yes! The tests are designed to work in CI environments with graceful degradation for UDP blocking.

**Q: What if all tests fail?**
A: Check internet connectivity, firewall settings, and VPN configuration. Try running from a different network (e.g., home vs. corporate).

**Q: Do I need to run these tests every time?**
A: No. Run once to verify your environment. Re-run if you change networks, firewall settings, or experience P2P connection issues.

**Q: What's the difference between TCP and UDP tests?**
A: TCP is used for RLPx encrypted sessions (required). UDP is used for Discv4 peer discovery (optional - can use TCP-only fallback).

---

## Support

For issues or questions:
1. Check [GitHub Issues](https://github.com/GeniusVentures/rlp/issues)
2. Review [Architecture.md](../Architecture.md) for P2P design details
3. Run `network_diagnostics --full` for detailed system info
4. Create issue with test output and system diagnostics

---

**Last Updated**: 2025-12-07
**Related**: [Architecture.md](../Architecture.md), [Issue #2 - EVM Bridging](https://github.com/GeniusVentures/rlp/issues/2)
