#!/bin/bash
# TinyLang performance benchmarks
# Measures execution time for each benchmark (3 runs).
# Usage: ./run_benchmarks.sh [bench_file]

DIR="$(dirname "$0")"
TINYLANG="$DIR/../tinylang"

if [ ! -f "$TINYLANG" ]; then
    echo "Error: tinylang not found at $TINYLANG"
    exit 1
fi

run_bench() {
    local name="$1" file="$2"
    echo "=== $name ==="
    printf "  %-6s %8s %8s %8s\n" "run" "real" "user" "maxmem"
    for iter in 1 2 3; do
        # time output goes to stderr; program output goes to stdout (captured separately)
        raw=$({ /usr/bin/time -l "$TINYLANG" "$file" 1>/dev/null; } 2>&1)
        real=$(echo "$raw" | grep real | awk '{print $1}')
        user=$(echo "$raw" | grep user | awk '{print $1}')
        mem=$(echo "$raw" | grep "peak memory" | awk '{print $1}')
        printf "  %-6s %8s %8s %8s\n" "$iter" "${real}s" "${user}s" "${mem}KB"
    done
    echo ""
}

if [ $# -ge 1 ]; then
    file="$1"
    [ ! -f "$file" ] && file="$DIR/$1"
    [ ! -f "$file" ] && file="$DIR/$1.tl"
    name="$(basename "$file" .tl)"
    run_bench "$name" "$file"
else
    echo "============================================"
    echo "  TinyLang Performance Benchmarks"
    echo "============================================"
    echo ""

    for file in "$DIR"/bench_*.tl; do
        [ -f "$file" ] || continue
        name="$(basename "$file" .tl)"
        run_bench "$name" "$file"
    done
fi
