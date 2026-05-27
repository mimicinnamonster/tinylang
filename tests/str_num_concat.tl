// Test string + number concatenation

function println(x=0) { print(x) print("\n") }

// --- Basic string + number ---
println("--- string + number ---")
println("hello" + 42)          // hello42
println(#("hello" + 42))       // 7 (5 + 2)

println("--- number + string ---")
println(42 + "hello")          // 42hello
println(#(42 + "hello"))       // 7

// String + 0
println("--- string + 0 ---")
println("zero: " + 0)          // zero: 0

// String + negative number
println("--- string + negative ---")
println("neg: " + (-5))        // neg: -5

// String + float
println("--- string + float ---")
println("pi: " + 3.14)         // pi: 3.14

// String + large number
println("--- string + large ---")
println("big: " + 1000000)     // big: 1000000

// In expressions
println("--- in expressions ---")
s = "value: "
n = 123
println(s + n)                 // value: 123
println(99 + " bottles")       // 99 bottles
println("a" + 1 + "b")         // a1b

// With function return values
println("--- with functions ---")
function greet(name="") {
    return "hello " + name
}
println(greet("world"))        // hello world
println(greet(42))             // hello 42

// --- Verify type errors still fire for non-string arrays ---
println("--- type errors ---")

// Generic array + number should fail
x = [1, 2, 3]
// print(x + 42)  // '+' type mismatch — uncomment to verify

// Number + generic array should fail
// print(42 + [1, 2, 3])  // '+' type mismatch — uncomment to verify

// --- Verify normal operations still work ---
println("--- normal ops still work ---")
println("abc" + "def")        // abcdef
println(10 + 20)              // 30
println([1, 2] + [3, 4])      // [1, 2, 3, 4]

// Array repeat
println([0] * 5)              // [0, 0, 0, 0, 0]
