// Test move-semantics optimization (COW avoidance for x = f(x, ...))

fun println(x=0) { print(x) print("\n") }

// Function that mutates a passed array and returns it
fun mutate_first(f=[], v=0) {
    f[0] = v
    ret f
}

// Test: x = mutate_first(x, 99) — should avoid COW deep copy
x = [1, 2, 3]
x = mutate_first(x, 99)
println(x[0])   // should be 99

// Test without move optimization (different variable)
y = [10, 20]
z = mutate_first(y, 30)
println(y[0])   // y should still be [10, 20] (value semantics preserved)
println(z[0])   // z should be [30, 20]

// Test: mutation in function on x = f(x) pattern
fun set_first(f=[], v=0) {
    f[0] = v
    ret f
}

arr = [100, 200, 300]
arr = set_first(arr, 999)
println(arr[0])   // should be 999

// Test with accessor pattern (inlining)
fun first(obj=[]) { ret obj[0] }
println(first(arr))  // should be 999

// Test constant function (inlining)
fun const_num() { ret 42 }
println(const_num())  // should be 42

// Test that normal shared mutation still COWs correctly
shared = [1, 2, 3]
alias = shared        // alias now references same array
shared = set_first(shared, 99)
println(alias[0])     // should still be 1 (value semantics preserved!)
println(shared[0])    // should be 99
