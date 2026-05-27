// test_include_expr.tl — test include with path expressions

fun println(x=0) { print(x) print("\n") }

// Include using a string literal
include "include_sub.tl"

// Use fun defined in included file (should work same as normal include)
greet("world")

// Use variable defined in included file
println(message)

// Use fun that returns a value
println(double(21))

println("ok")
