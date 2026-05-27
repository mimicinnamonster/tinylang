// Benchmark: tail call optimization
// Verifies that TCO prevents C stack overflow for deep recursion.

fun println(x=0) { print(x) print("\n") }

// Factorial with TCO
fun fact(n=0, acc=1) {
    if n == 0 {
        ret acc
    }
    ret fact(n - 1, n * acc)
}

// Deep trampoline: count down to 0 via TCO
fun countdown(n=0) {
    if n == 0 {
        ret 0
    }
    ret countdown(n - 1)
}

// Fibonacci with TCO (two accumulators)
fun fib(n=0, a=0, b=1) {
    if n == 0 {
        ret a
    }
    ret fib(n - 1, b, a + b)
}

// Test 1: deep countdown (no result accumulation, just recursion depth)
println("tco countdown:")
println(countdown(50000))

// Test 2: factorial with large n
println("tco fact:")
println(fact(100, 1))

// Test 3: fibonacci
println("tco fib:")
println(fib(30, 0, 1))
