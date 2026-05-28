// Test function inlining optimizations

fun println(x=0) { print(x) print("\n") }

// Pattern 0: Constant function (returns number)
fun answer() { ret 42 }
// Pattern 1: Constant function (returns nil)
fun empty() { ret nil }
// Pattern 2: Constant function (returns string)
fun greeting() { ret "hello inline!\n" }

print(greeting())
println(answer())
if empty() == nil {
    println("nil works")
}

// Pattern 3: Accessor with constant index
fun first(obj=[]) { ret obj[0] }
fun second(obj=[]) { ret obj[1] }

arr = [10, 20, 30]
println(first(arr))
println(second(arr))

// Pattern 4: Accessor with param index
fun elem(obj=[], i=0) { ret obj[i] }
println(elem(arr, 2))

// Chain: accessor as argument to another call
println(first(arr) + second(arr))

// Inline + inline: accessor result fed into print
print(elem(arr, 0))
print("\n")
