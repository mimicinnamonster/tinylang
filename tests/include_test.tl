// include_test.tl — test include "path"
include "include_sub.tl"

// Use function defined in included file
greet("world")

// Use variable defined in included file
print(message)

// Use function that returns a value
print(double(21))

// Defined after include — should work
print("done")
