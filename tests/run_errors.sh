#!/bin/bash
# Error test runner for TinyLang
# Runs each test file individually (errors halt the interpreter)
# Usage: ./run_errors.sh [test_file.tl]

DIR="$(dirname "$0")"
TINYLANG="$DIR/../tiny"
PASS=0
FAIL=0
TOTAL=0

if [ ! -f "$TINYLANG" ]; then
    echo "Error: tiny not found at $TINYLANG"
    echo "Build it first: cc -o tiny tinylang.c -lm"
    exit 1
fi

run_test() {
    local file="$1"
    local basename="$(basename "$file" .tl)"
    local expected_msg=""

    # Determine expected error message from filename/comment
    case "$basename" in
        e_div_zero)       expected_msg="division by zero";;
        e_mod_zero)       expected_msg="modulo by zero";;
        e_oob)            expected_msg="index out of bounds";;
        e_type_plus)      expected_msg="'+' type mismatch";;
        e_type_minus)     expected_msg="'-' requires numbers";;
        e_type_star)      expected_msg="'*' type mismatch";;
        e_type_slash)     expected_msg="'/' requires numbers";;
        e_bitwise)        expected_msg="bitwise requires numbers";;
        e_lt)             expected_msg="'<' requires numbers";;
        e_ge)             expected_msg="'>=' requires numbers";;
        e_undef_var)      expected_msg="undefined";;
        e_undef_fn)       expected_msg="undefined function";;
        e_index_nonarr)   expected_msg="cannot index into non-array";;
        e_index_type)     expected_msg="unterminated string";;
        e_minus_nonnum)   expected_msg="minus on non-number";;
        e_hash_nonarr)    expected_msg="# requires array";;
        e_hash_index)      expected_msg="# requires array";;
        e_print_noargs)   expected_msg="print needs 1 arg";;
        e_fn_redef)       expected_msg="already defined";;
        e_include_missing) expected_msg="cannot include";;
        e_invalid_binary) expected_msg="invalid binary literal";;
        e_invalid_hex)   expected_msg="invalid hex literal";;
        e_bad_octal)     expected_msg="invalid digit in octal literal";;
        e_percent_nonnum) expected_msg="'%' requires numbers";;
        e_ret_type_mismatch) expected_msg="inconsistent return type";;
        e_destructure_nonarr) expected_msg="destructure requires array";;
        e_default_after_required) expected_msg="must have a default value";;
        *)                expected_msg="error:";;
    esac

    TOTAL=$((TOTAL + 1))

    # Run the test, capture stderr and exit code
    stderr_output=$("$TINYLANG" "$file" 2>&1 1>/dev/null)
    exit_code=$?

    # Check exit code is non-zero
    if [ "$exit_code" -eq 0 ]; then
        echo "FAIL: $basename — expected non-zero exit code, got 0"
        FAIL=$((FAIL + 1))
        return
    fi

    # Check stderr contains expected message
    if echo "$stderr_output" | grep -q "$expected_msg"; then
        echo "PASS: $basename"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $basename — expected stderr to contain '$expected_msg'"
        echo "       got: $stderr_output"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== TinyLang Error Tests ==="
echo ""

# If argument given, run only that test
if [ $# -ge 1 ]; then
    test_file="$1"
    if [ -f "$test_file" ]; then
        run_test "$test_file"
    elif [ -f "$DIR/$test_file" ]; then
        run_test "$DIR/$test_file"
    elif [ -f "$DIR/$test_file.tl" ]; then
        run_test "$DIR/$test_file.tl"
    else
        echo "Test file not found: $test_file"
        exit 1
    fi
else
    for file in "$DIR"/e_*.tl; do
        [ -f "$file" ] || continue
        run_test "$file"
    done
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ($TOTAL total) ==="
exit $FAIL
