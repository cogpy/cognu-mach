#!/bin/bash
# Static analysis script for GNU Mach - Phase 1.1 Code Quality
# This script runs various static analysis tools to identify potential issues

set -e

echo "=== GNU Mach Static Analysis ==="
echo "Running static analysis tools to identify code quality issues..."
echo

# Check if tools are available
check_tool() {
    if ! command -v $1 &> /dev/null; then
        echo "Warning: $1 not found. Please install it for complete analysis."
        return 1
    fi
    return 0
}

# Ensure configure exists
if [ ! -x ./configure ]; then
    echo "'configure' not found. Bootstrapping autotools (autoreconf)..."
    if check_tool autoreconf; then
        autoreconf -fi || true
    else
        echo "Warning: autoreconf not available; skipping configure generation."
    fi
fi

# Run cppcheck if available
if check_tool cppcheck; then
    echo "=== Running cppcheck ==="
    cppcheck --enable=all --error-exitcode=0 \
             --suppress=missingIncludeSystem \
             --suppress=unmatchedSuppression \
             --inline-suppr \
             -I include -I i386/include -I x86_64/include \
             kern/*.c ipc/*.c vm/*.c device/*.c 2>&1 | tee cppcheck-report.txt
    echo "Cppcheck report saved to cppcheck-report.txt"
    echo
fi

# Run clang static analyzer if available
if check_tool clang; then
    echo "=== Running clang static analyzer ==="
    # Create a build directory for scan-build
    mkdir -p build-analyze
    cd build-analyze
    
    if check_tool scan-build; then
        if [ -x ../configure ]; then
            scan-build ../configure --host=i686-gnu || true
            scan-build -o scan-results make -j$(nproc) || true
            echo "Clang static analyzer results saved in build-analyze/scan-results/"
        else
            echo "Skipping scan-build configure step (no configure script)."
        fi
    else
        echo "scan-build not found. Install clang-tools for static analysis."
    fi
    cd ..
    echo
fi

# Run compiler with extra warnings to identify issues
echo "=== Checking for compiler warnings ==="
echo "Building with -Werror to identify all warnings..."
mkdir -p build-warnings
cd build-warnings
if [ -x ../configure ]; then
    ../configure --host=i686-gnu CFLAGS="-g -O2 -Werror" || true
    make -j$(nproc) 2>&1 | tee ../compiler-warnings.txt || true
else
    echo "No configure script; skipping build with warnings."
    echo "No configure script; skipping build with warnings." > ../compiler-warnings.txt
fi
cd ..
echo "Compiler warnings saved to compiler-warnings.txt"
echo

echo "=== Static analysis complete ==="
echo "Review the following files for issues:"
echo "  - cppcheck-report.txt"
echo "  - compiler-warnings.txt"
echo "  - build-analyze/scan-results/ (if clang analyzer was run)"