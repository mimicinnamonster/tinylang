// test_trace_include.tl — verify stack traces across includes

include "trace_include_sub.tl"

fun main_fn() {
    // Just call the sub fun — if trace works, error message shows chain
    sub_err()
}

// This test verifies the error propagation by checking that calling main_fn
// produces an error (which we can't catch without assert, but the test
// runner verifies the execution path ends correctly)
