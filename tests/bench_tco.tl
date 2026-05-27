// Benchmark: tail call optimization
// Verifies that TCO prevents C stack overflow for deep recursion.

function println(x) { print(x) print("\n") }

// Factorial with TCO
function fact(n, acc) {
    if n = 0 {
        return acc
    }
    return fact(n - 1, n * acc)
}

// Deep trampoline: count down to 0 via TCO
function countdown(n) {
    if n = 0 {
        return 0
    }
    return countdown(n - 1)
}

// Fibonacci with TCO (two accumulators)
function fib(n, a, b) {
    if n = 0 {
        return a
    }
    return fib(n - 1, b, a + b)
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
