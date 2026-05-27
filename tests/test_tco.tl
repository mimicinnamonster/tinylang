// tail-call optimized factorial
fun println(x=0) { print(x) print("\n") }

fun fact_tco(n=0, acc=1) {
    if n == 0 {
        ret acc
    }
    ret fact_tco(n - 1, n * acc)
}

// should handle deep recursion without C stack overflow
println(fact_tco(5, 1))
println(fact_tco(10, 1))
println(fact_tco(100, 1))
println(fact_tco(1000, 1))
