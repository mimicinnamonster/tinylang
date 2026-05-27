// test_include_expr.tl — test include with thispath() expressions

fun println(x=0) { print(x) print("\n") }

// Include using thispath() + relative filename
// thispath() inside include returns the directory of the current file
include thispath() + "include_sub.tl"

// Use fun defined in included file (should work same as normal include)
greet("world")

// Use variable defined in included file
println(message)

// Use fun that returns a value
println(double(21))

// Verify plain string literal includes still work
// (use a different file to exercise the old code path)
include "thispath_sub.tl"

sub = get_sub_path()
println(#sub)

println("ok")
