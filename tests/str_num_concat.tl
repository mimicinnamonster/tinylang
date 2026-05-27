// Test string + number concatenation

// --- Basic string + number ---
print("--- string + number ---")
print("hello" + 42)          // hello42
print(#("hello" + 42))       // 7 (5 + 2)

print("--- number + string ---")
print(42 + "hello")          // 42hello
print(#(42 + "hello"))       // 7

// String + 0
print("--- string + 0 ---")
print("zero: " + 0)          // zero: 0

// String + negative number
print("--- string + negative ---")
print("neg: " + (-5))        // neg: -5

// String + float
print("--- string + float ---")
print("pi: " + 3.14)         // pi: 3.14

// String + large number
print("--- string + large ---")
print("big: " + 1000000)     // big: 1000000

// In expressions
print("--- in expressions ---")
s = "value: "
n = 123
print(s + n)                 // value: 123
print(99 + " bottles")       // 99 bottles
print("a" + 1 + "b")         // a1b

// With function return values
print("--- with functions ---")
function greet(name) {
    return "hello " + name
}
print(greet("world"))        // hello world
print(greet(42))             // hello 42

// --- Verify type errors still fire for non-string arrays ---
print("--- type errors ---")

// Generic array + number should fail
x = [1, 2, 3]
// print(x + 42)  // '+' type mismatch — uncomment to verify

// Number + generic array should fail
// print(42 + [1, 2, 3])  // '+' type mismatch — uncomment to verify

// --- Verify normal operations still work ---
print("--- normal ops still work ---")
print("abc" + "def")        // abcdef
print(10 + 20)              // 30
print([1, 2] + [3, 4])      // [1, 2, 3, 4]

// Array repeat
print([0] * 5)              // [0, 0, 0, 0, 0]
