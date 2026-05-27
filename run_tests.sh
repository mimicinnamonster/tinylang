#!/bin/bash
# TinyLang test suite runner
# Usage: ./run_tests.sh

DIR="$(dirname "$0")"
TINYLANG="$DIR/tinylang"
TESTDIR="$DIR/tests"

if [ ! -f "$TINYLANG" ]; then
    echo "Error: tinylang not found at $TINYLANG"
    echo "Build it first: cc -o tiny tinylang.c -lm"
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

# Run all non-error .tl files in tests/ (skip ffi_* tests, run them separately)
for file in "$TESTDIR"/*.tl; do
    [ -f "$file" ] || continue
    basename="$(basename "$file" .tl)"
    case "$basename" in e_*|ffi_*) continue;; esac
    run_pass "$basename" "$file"
done

# FFI tests (requires TL_FFI build)
FFI_BIN="$DIR/tinylang-ffi"
if [ -f "$FFI_BIN" ]; then
    # Build the test shared library
    echo "  (building ffi_lib for tests)"
    case "$(uname)" in
        Darwin) SHLIB="ffi_lib.dylib" ;;
        *)      SHLIB="ffi_lib.so" ;;
    esac
    cc -shared -fPIC -o "$TESTDIR/$SHLIB" "$TESTDIR/ffi_lib.c" 2>/dev/null
    for file in "$TESTDIR"/ffi_*.tl; do
        [ -f "$file" ] || continue
        basename="$(basename "$file" .tl)"
        run_pass_ffi() {
            local name="$1" file="$2"
            printf "  %-12s ... " "$name"
            if output=$("$FFI_BIN" "$file" 2>&1); then
                echo "ok"
            else
                echo "FAIL"
                echo "    $(echo "$output" | tail -1)"
                fail=$((fail + 1))
            fi
        }
        run_pass_ffi "$basename" "$file"
    done
else
    echo "  (ffi tests skipped — build 'tinylang-ffi' to enable)"
fi

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
