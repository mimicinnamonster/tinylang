// Fibonacci benchmark — TinyLang version
// Uses TCO (tail call optimization) recursion for fib(N)
// The tail-recursive helper accumulates the result, so TCO kicks in
// and no C stack frame is consumed per iteration — unlimited depth.

fun fib_tco(n=0, a=0, b=1) {
    if n == 0 { ret a }
    ret fib_tco(n - 1, b, a + b)
}

fun fib(n=0) { ret fib_tco(n, 0, 1) }

print(fib(35))
print("\n")
