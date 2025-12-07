@echo off
REM Standalone builder for integration tests
REM This bypasses the main build system for quick testing

setlocal

set BUILD_DIR=build_integration
set SOURCE_DIR=%CD%\test\integration

echo ========================================
echo  Building Integration Tests
echo ========================================
echo.

REM Clean previous build
if exist "%BUILD_DIR%" (
    echo Cleaning previous build...
    rmdir /s /q "%BUILD_DIR%"
)

mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

echo Configuring CMake...
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBoost_DIR="H:/code/genius/thirdparty/build/Windows/Release/boost/build/lib/cmake/Boost-1.85.0" ^
    -DGTest_DIR="H:/code/genius/thirdparty/build/Windows/Release/GTest/lib/cmake/GTest" ^
    "%SOURCE_DIR%"

if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo Building...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Build Complete!
echo ========================================
echo.
echo Executables:
dir /b Release\*.exe
echo.
echo To run tests:
echo   cd %BUILD_DIR%\Release
echo   network_port_test.exe
echo   network_diagnostics.exe
echo.

pause
