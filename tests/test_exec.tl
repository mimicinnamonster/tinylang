fun println(x=0) { print(x) print("\n") }

println("--- exec ---")

// Basic command
out = exec("echo hello world")
// Remove trailing newline for comparison
println(out == "hello world\n")

// whoami should return a non-empty string
out = exec("whoami")
println(#out > 2)

// Multiple words
out = exec("echo foo bar baz")
println(out == "foo bar baz\n")

// Pipe and redirect work
out = exec("echo hello | tr a-z A-Z")
println(out == "HELLO\n")

// Command with no output
out = exec("true")
println(#out == 0)

// Can run any shell command
out = exec("printf '%s' 'direct'")
println(out == "direct")

println("ok")
