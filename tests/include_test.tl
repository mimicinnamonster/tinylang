// include_test.tl — test include "path"

function println(x) { print(x) print("\n") }

include "include_sub.tl"

// Use function defined in included file
greet("world")

// Use variable defined in included file
println(message)

// Use function that returns a value
println(double(21))

// Defined after include — should work
println("done")
