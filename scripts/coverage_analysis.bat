@echo off
setlocal enabledelayedexpansion

REM RLP Code Coverage Analysis Script (Windows)
REM This script builds the project and runs all tests for coverage analysis

echo =====================================================================
echo RLP CODE COVERAGE ANALYSIS (Windows)
echo =====================================================================

REM Configuration
set BUILD_DIR=build_coverage
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set COVERAGE_DIR=%PROJECT_ROOT%\coverage_report

echo Project root: %PROJECT_ROOT%
echo Build directory: %BUILD_DIR%
echo Coverage report directory: %COVERAGE_DIR%

REM Clean previous build
echo Cleaning previous build...
if exist "build\Windows\Debug" rmdir /s /q "build\Windows\Debug"
if exist "%COVERAGE_DIR%" rmdir /s /q "%COVERAGE_DIR%"
mkdir "%COVERAGE_DIR%"

REM Build project
echo Building project...
cd /d "%PROJECT_ROOT%"

REM Use the existing Windows build system
cd /d "build\Windows"
if not exist "Debug" mkdir Debug
cd Debug

cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

cmake --build . --config Debug
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

REM Run all tests
echo =====================
echo Running test suite...
echo =====================

set TOTAL_TESTS=0
set PASSED_TESTS=0

REM Test executables to run
set TESTS=rlp_decoder_tests rlp_edge_cases rlp_comprehensive_tests rlp_property_tests

for %%t in (%TESTS%) do (
    set /a TOTAL_TESTS+=1
    echo Running %%t...
    
    REM Check if executable exists
    if exist "Debug\%%t.exe" (
        Debug\%%t.exe
        if !errorlevel! equ 0 (
            echo ✓ %%t PASSED
            set /a PASSED_TESTS+=1
        ) else (
            echo ✗ %%t FAILED
        )
    ) else (
        echo ✗ %%t NOT FOUND
    )
    echo.
)

echo Test Results: %PASSED_TESTS%/%TOTAL_TESTS% tests passed
echo.

REM Summary
echo =====================================================================
echo TEST EXECUTION SUMMARY
echo =====================================================================
echo Total tests run: %TOTAL_TESTS%
echo Tests passed: %PASSED_TESTS%
echo Tests failed: %TOTAL_FAILED%

if %PASSED_TESTS% equ %TOTAL_TESTS% (
    echo 🎉 ALL TESTS PASSED!
    set EXIT_CODE=0
) else (
    echo ⚠️  SOME TESTS FAILED
    set EXIT_CODE=1
)

echo.
echo Note: For detailed code coverage analysis on Windows, consider using:
echo - Visual Studio Code Coverage (with Enterprise edition)
echo - OpenCppCoverage (https://github.com/OpenCppCoverage/OpenCppCoverage)
echo - gcov with MinGW/MSYS2 environment
echo.

echo =====================================================================
echo ANALYSIS COMPLETE
echo =====================================================================

cd /d "%PROJECT_ROOT%"
exit /b %EXIT_CODE%