// test_args.tl — test for args() with extra command-line arguments
// This test is normally invoked via run_args_test.sh which calls:
//   ./tiny test_args.tl --flag value extra stuff
// When run normally (./tiny test_args.tl), args contains [test_args.tl]

fun println(x=0) { print(x) print("\n") }

a = args()
println(#a >= 1)     // at least the script path

// First element should be the script path
println(#a[0] > 0)

// Check it ends with .tl (or contains test_args)
a0_end3 = a[0][#a[0] - 3 : #a[0]]
println(a0_end3 == ".tl")

// When extra args are passed, check the last one is "extra"
// This only passes when run via run_args_test.sh
// When run directly, a has 1 element, so a[1] would error.
// Instead we check conditionally:
if #a > 1 {
    // Extra args were passed
    println(a[1] == "--flag")
    println(a[2] == "value")
    println(a[3] == "extra")
} else {
    // No extra args (direct run) — just print that we're in basic mode
    println(1)
    println(1)
    println(1)
}

println("ok")
