// include_test.tl — test include "path"

fun println(x=0) { print(x) print("\n") }

include "include_sub.tl"

// Use fun defined in included file
greet("world")

// Use variable defined in included file
println(message)

// Use fun that returns a value
println(double(21))

// Defined after include — should work
println("done")
