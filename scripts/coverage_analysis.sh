#!/bin/bash

# RLP Code Coverage Analysis Script
# This script builds the project with coverage instrumentation and runs all tests

set -e

echo "====================================================================="
echo "RLP CODE COVERAGE ANALYSIS"
echo "====================================================================="

# Check if required tools are available
command -v gcov >/dev/null 2>&1 || { echo "gcov is required but not installed. Aborting." >&2; exit 1; }
command -v lcov >/dev/null 2>&1 || { echo "lcov is required but not installed. Install with: apt-get install lcov" >&2; exit 1; }

# Configuration
BUILD_DIR="build_coverage"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COVERAGE_DIR="${PROJECT_ROOT}/coverage_report"

echo "Project root: ${PROJECT_ROOT}"
echo "Build directory: ${BUILD_DIR}"
echo "Coverage report directory: ${COVERAGE_DIR}"

# Clean previous build and coverage data
echo "Cleaning previous build and coverage data..."
rm -rf "${BUILD_DIR}"
rm -rf "${COVERAGE_DIR}"
mkdir -p "${BUILD_DIR}"
mkdir -p "${COVERAGE_DIR}"

# Build with coverage flags
echo "Building with coverage instrumentation..."
cd "${BUILD_DIR}"

cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage -g -O0 -fprofile-arcs -ftest-coverage" \
    -DCMAKE_C_FLAGS="--coverage -g -O0 -fprofile-arcs -ftest-coverage" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage"

make -j$(nproc)

# Initialize coverage data
echo "Initializing coverage data..."
lcov --directory . --zerocounters

# Run all tests
echo "Running test suite..."
echo "====================="

# Define test executables
TESTS=(
    "rlp_decoder_tests"
    "rlp_edge_cases" 
    "rlp_comprehensive_tests"
    "rlp_property_tests"
    "rlp_benchmark_tests"
)

TOTAL_TESTS=${#TESTS[@]}
PASSED_TESTS=0

for test in "${TESTS[@]}"; do
    echo "Running ${test}..."
    if ./"${test}"; then
        echo "✓ ${test} PASSED"
        ((PASSED_TESTS++))
    else
        echo "✗ ${test} FAILED"
    fi
    echo ""
done

echo "Test Results: ${PASSED_TESTS}/${TOTAL_TESTS} tests passed"
echo ""

# Capture coverage data
echo "Capturing coverage data..."
lcov --directory . --capture --output-file coverage.info

# Filter out system headers and test files
echo "Filtering coverage data..."
lcov --remove coverage.info '/usr/*' --output-file coverage_filtered.info
lcov --remove coverage_filtered.info '*/test/*' --output-file coverage_final.info
lcov --remove coverage_final.info '*/build/*' --output-file coverage_clean.info

# Generate HTML report
echo "Generating HTML coverage report..."
genhtml coverage_clean.info --output-directory "${COVERAGE_DIR}"

# Display summary
echo "====================================================================="
echo "COVERAGE SUMMARY"
echo "====================================================================="
lcov --summary coverage_clean.info

# Calculate coverage percentage
COVERAGE_PERCENTAGE=$(lcov --summary coverage_clean.info 2>&1 | grep "lines" | grep -o '[0-9.]*%' | head -1)
echo ""
echo "Total Line Coverage: ${COVERAGE_PERCENTAGE}"

# Check if we met our >95% target
COVERAGE_NUM=$(echo "${COVERAGE_PERCENTAGE}" | sed 's/%//')
if (( $(echo "${COVERAGE_NUM} >= 95.0" | bc -l) )); then
    echo "🎉 SUCCESS: Coverage target of >95% achieved!"
    EXIT_CODE=0
else
    echo "⚠️  WARNING: Coverage target of >95% not met. Current: ${COVERAGE_PERCENTAGE}"
    EXIT_CODE=1
fi

echo ""
echo "Detailed report available at: file://${COVERAGE_DIR}/index.html"
echo "To view: open ${COVERAGE_DIR}/index.html"
echo ""

echo "====================================================================="
echo "COVERAGE ANALYSIS COMPLETE"
echo "====================================================================="

cd "${PROJECT_ROOT}"
exit ${EXIT_CODE}