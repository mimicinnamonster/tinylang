#!/bin/bash
# TinyLang test suite runner
# Usage: ./run_tests.sh

DIR="$(dirname "$0")"
TINYLANG="$DIR/tinylang"
TESTDIR="$DIR/tests"

if [ ! -f "$TINYLANG" ]; then
    echo "Error: tinylang not found at $TINYLANG"
    echo "Build it first: cc -o tinylang tinylang.c -lm"
    exit 1
fi

echo "============================================"
echo "  TinyLang Test Suite"
echo "============================================"
echo ""

# === Happy-path tests ===
echo "--- Happy-path tests ---"

fail=0

run_pass() {
    local name="$1" file="$2"
    printf "  %-12s ... " "$name"
    if output=$("$TINYLANG" "$file" 2>&1); then
        echo "ok"
    else
        echo "FAIL"
        echo "    $(echo "$output" | tail -1)"
        fail=$((fail + 1))
    fi
}

# Run all non-error .tl files in tests/
for file in "$TESTDIR"/*.tl; do
    [ -f "$file" ] || continue
    basename="$(basename "$file" .tl)"
    # Skip error test files
    case "$basename" in e_*) continue;; esac
    run_pass "$basename" "$file"
done

echo ""

# === Error tests ===
echo "--- Error tests ---"
"$TESTDIR/run_errors.sh"
err=$?
[ "$err" -gt 0 ] && fail=$((fail + err))

echo ""
if [ "$fail" -eq 0 ]; then
    echo "All tests passed!"
else
    echo "$fail test(s) failed."
fi
exit $fail
