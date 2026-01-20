@echo off
REM Copyright 2025 GeniusVentures
REM SPDX-License-Identifier: Apache-2.0
REM
REM Local network connectivity test runner for Windows
REM Tests P2P port accessibility to EVM testnets

setlocal enabledelayedexpansion

echo ========================================
echo   RLP Network Connectivity Tests
echo ========================================
echo.

REM Check if build exists
set BUILD_DIR=build\Windows\Release
if not exist "%BUILD_DIR%\network_port_test.exe" (
    echo ERROR: Tests not built yet!
    echo.
    echo Build the tests first:
    echo   1. cd build\Windows\Release
    echo   2. cmake -G "Visual Studio 17 2022" -A x64 -DENABLE_INTEGRATION_TESTS=ON ..
    echo   3. cmake --build . --config Release
    echo.
    pause
    exit /b 1
)

REM Run diagnostics first
echo [STEP 1/3] Running network diagnostics...
echo.
if exist "%BUILD_DIR%\network_diagnostics.exe" (
    "%BUILD_DIR%\network_diagnostics.exe" --full
    echo.
) else (
    echo WARNING: network_diagnostics.exe not found, skipping...
    echo.
)

REM Run port connectivity tests
echo [STEP 2/3] Testing TCP/UDP port connectivity...
echo.
"%BUILD_DIR%\network_port_test.exe" --gtest_color=yes
set TEST_RESULT=%ERRORLEVEL%

echo.
echo ========================================
echo   Test Results Summary
echo ========================================
echo.

if %TEST_RESULT% equ 0 (
    echo [PASS] All network tests passed!
    echo.
    echo You can proceed with full P2P integration tests.
) else (
    echo [WARN] Some tests failed or were skipped
    echo.
    echo Common issues:
    echo   - Corporate firewall blocking ports 30303 or 9222
    echo   - VPN interference
    echo   - Windows Firewall blocking outbound connections
    echo.
    echo Troubleshooting steps:
    echo   1. Temporarily disable Windows Firewall for testing
    echo   2. Check VPN settings
    echo   3. Review firewall logs in Event Viewer
    echo   4. Try different network (e.g., mobile hotspot)
    echo.
)

echo [STEP 3/3] Generating test report...
echo.
echo Test execution completed at: %date% %time%
echo Build directory: %BUILD_DIR%
echo.

REM Create results directory
if not exist "test_results\" mkdir test_results

REM Save test output
echo Saving results to test_results\network_test_results.txt
"%BUILD_DIR%\network_port_test.exe" --gtest_color=no > test_results\network_test_results.txt 2>&1

echo.
echo ========================================
echo Full results saved to: test_results\network_test_results.txt
echo ========================================
echo.

pause
exit /b %TEST_RESULT%
