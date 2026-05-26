// tail-call optimized factorial
function fact_tco(n, acc) {
    if n = 0 {
        return acc
    }
    return fact_tco(n - 1, n * acc)
}

// should handle deep recursion without C stack overflow
print(fact_tco(5, 1))
print(fact_tco(10, 1))
print(fact_tco(100, 1))
print(fact_tco(1000, 1))
