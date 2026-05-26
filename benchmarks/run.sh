#!/bin/bash
# TinyLang vs C vs Node.js — Performance Benchmark Runner
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TINYLANG="$SCRIPT_DIR/../tinylang"
NODE="$(which node)"
C_BIN="$SCRIPT_DIR/c_bin"
C_SRC="$SCRIPT_DIR/c_src"
JS_SRC="$SCRIPT_DIR/js_src"
TL_SRC="$SCRIPT_DIR/tl_src"

echo "=============================================="
echo "  TinyLang vs C vs Node.js — Benchmarks"
echo "  Hardware: Apple M1, macOS 15.7"
echo "  Node: $($NODE --version 2>/dev/null)"
echo "=============================================="
echo ""

# Ensure C binaries exist
if [ ! -f "$C_BIN/spectral-norm" ]; then
    echo "Building C benchmarks..."
    mkdir -p "$C_BIN"
    cd "$C_SRC"
    cc -Wall -Wextra -O2 -lm -o "$C_BIN/spectral-norm" spectral-norm.c
    cc -Wall -Wextra -O2 -lm -o "$C_BIN/nbody" nbody.c
    cc -Wall -Wextra -O2 -lm -o "$C_BIN/mandelbrot" mandelbrot.c
    cc -Wall -Wextra -O2 -lm -o "$C_BIN/fasta" fasta.c
    echo "  Done."
    echo ""
fi

run_one() {
    local label="$1" desc="$2" c_cmd="$3" js_cmd="$4" tl_cmd="$5"
    echo "=== $label ==="
    echo "  $desc"
    echo ""

    echo "  [C]"
    eval "/usr/bin/time -l $c_cmd 2>&1 1>/dev/null" || true
    echo ""
    echo "  [Node.js]"
    eval "/usr/bin/time -l node $js_cmd 2>&1 1>/dev/null" || true
    echo ""
    echo "  [TinyLang]"
    eval "/usr/bin/time -l $TINYLANG $tl_cmd 2>&1 1>/dev/null" || true
    echo ""
}

echo "1. Spectral-Norm (matrix eigenvalue)"
echo "   C/JS: N=5500 (standard), TL: N=100"
run_one "spectral-norm" "" \
    "$C_BIN/spectral-norm 5500" \
    "$JS_SRC/spectral-norm.js 5500" \
    "$TL_SRC/spectral_norm.tl"

echo "2. N-Body (solar system, 5 bodies)"
echo "   C/JS: N=5M steps, TL: N=5000 steps"
run_one "nbody" "" \
    "$C_BIN/nbody 5000000" \
    "$JS_SRC/nbody.js 5000000" \
    "$TL_SRC/nbody.tl"

echo "3. Mandelbrot (200x200, 50 iter/pixel)"
run_one "mandelbrot" "" \
    "$C_BIN/mandelbrot 200" \
    "$JS_SRC/mandelbrot.js 200" \
    "$TL_SRC/mandelbrot.tl"

echo "4. Fasta (DNA sequence, N=25000)"
run_one "fasta" "" \
    "$C_BIN/fasta 25000" \
    "$JS_SRC/fasta.js 25000" \
    "$TL_SRC/fasta.tl"

echo "=============================================="
echo "  Done! See REPORT.md for full analysis."
echo "=============================================="
