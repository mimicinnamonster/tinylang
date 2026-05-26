// test_trace_include.tl — verify stack traces across includes

include "trace_include_sub.tl"

function main_fn() {
    sub_err()
}

// Catch the error with assert and verify it propagated
caught = assert(main_fn())
assert(#caught > 0)

// Verify thispath() still works correctly here
p = thispath()
assert(#p > 0)

print("trace include test ok")
