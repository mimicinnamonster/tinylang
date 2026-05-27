#!/bin/bash
# Run the args() test with extra command-line arguments
# Tests that args() correctly captures all arguments

DIR="$(dirname "$0")"
TINYLANG="$DIR/../tiny"
TESTFILE="$DIR/test_args.tl"

if [ ! -f "$TINYLANG" ]; then
    echo "Error: tiny not found"
    exit 1
fi

echo "--- args test ---"

# Run with extra args
output=$("$TINYLANG" "$TESTFILE" --flag value extra 2>&1)
if [ $? -eq 0 ]; then
    # Check output: we expect the "extra args" branch to fire
    # It prints: #a >= 1, script path check, .tl check, then --flag value extra checks
    count=$(echo "$output" | wc -l)
    # Should print 5 truthy values then "ok"
    result=$(echo "$output" | tail -1)
    if [ "$result" = "ok" ]; then
        echo "ok"
    else
        echo "FAIL (expected 'ok' at end, got '$result')"
        echo "  output:"
        echo "$output" | sed 's/^/    /'
        exit 1
    fi
else
    echo "FAIL (tiny exited with error)"
    echo "$output"
    exit 1
fi
