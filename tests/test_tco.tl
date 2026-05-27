// tail-call optimized factorial
function println(x=0) { print(x) print("\n") }

function fact_tco(n=0, acc=1) {
    if n == 0 {
        return acc
    }
    return fact_tco(n - 1, n * acc)
}

// should handle deep recursion without C stack overflow
println(fact_tco(5, 1))
println(fact_tco(10, 1))
println(fact_tco(100, 1))
println(fact_tco(1000, 1))
