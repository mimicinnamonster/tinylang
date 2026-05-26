// Compact scalar number tests

print("=== 1. Type boundary detection ===")

// U8: [0, 255]
print(0)
print(255)

// U16: [256, 65535]
print(256)
print(65535)

// U32: [65536, 4294967295]
print(65536)
print(4294967295)

// U64: above 2^32
print(4294967296)
print(5000000000)

// I8: [-128, 127]
print(-128)
print(127)

// I16: [-32768, 32767]
print(-32768)
print(32767)

// I32: [-2147483648, 2147483647]
print(-2147483648)
print(2147483647)

// I64: beyond I32 range
print(-3000000000)
print(3000000000)

// F32 exact in float
print(0.125)
print(1.5)
print(16777216)       // 2^24, exact in F32... but integer so stored as integer type

// F64 only (too precise for F32)
pi = 3.14159265358979
print(pi)

print("=== 2. Arithmetic operations ===")

// Addition with same type
a = 100
b = 50
print(a + b)

// Addition with wider result
print(200 + 100)       // still U8 (300 > 255? No, 300 is in range... actually 300 > 255 → U16)
print(255 + 1)         // 256, crosses U8 boundary
print(65535 + 1)       // 65536, crosses U16 boundary

// Subtraction can go negative
print(5 - 3)           // 2, U8
print(3 - 5)           // -2, I8 (went negative)
print(100 - 200)       // -100, I8
print(-100 - 100)      // -200, I16 (crossed I8 boundary)

// Multiplication
print(100 * 2)         // 200, U8... actually U8 max is 255, so 200 fits!
print(200 * 2)         // 400, U16 (crossed U8)
print(1000 * 1000)     // 1000000, I32... wait, 1000000 in U32

// Division
print(10 / 3)          // 3.333..., F32 or F64? Has fractional part → F32 or F64
print(100 / 2)         // 50, integer → U8

// Modulo
print(10 % 3)          // 1

print("=== 3. Negation and sign changes ===")

// Unary minus
print(-5)              // -5, I8
print(-(-5))           // 5, U8 (back to positive)
print(-128)            // -128, I8
print(-(-128))         // 128, U8? Actually 128 > 127 so... U8

// Negating positive gives negative
x = 42
print(-x)              // -42, I8

// Negating large values
print(-70000)          // -70000, I32

print("=== 4. Mixed-type operations ===")

// U8 + I8 → I16
print(200 + (-50))     // 150, I16

// U8 * I16 → I32? Let's see: 200 * 1000 = 200000, I32
print(200 * 1000)

// I16 + U32 → I64 since neither fits in the other
a = -1000
b = 70000
print(a + b)           // 69000

// Integer + float → F64 (or F32 if small enough)
print(100 + 0.5)       // 100.5, has fractional → F32/F64
print(42 + 1.5)        // 43.5

// F32 + F64 → F64
print(1.5 + 3.14159265358979)

print("=== 5. Comparisons across NumKinds ===")

print(42 = 42)         // 1
print(42 = 43)         // []
print(-100 = -100)     // 1
print(70000 = 70000)   // 1
print(5000000000 = 5000000000)  // 1
print(1.5 = 1.5)       // 1
print(3.14159 = 3.14159)       // 1

// Cross-type comparisons
print(42 = 42.0)       // 1 (number equality ignores storage kind)
print(100 = 100.0)     // 1
print(5 = 5.0)         // 1

print(42 < 100)        // 1
print(100 < 42)        // []
print(-100 < 50)       // 1
print(50 > -100)       // 1
print(70000 > 100)     // 1
print(5000000000 > 100) // 1

print("=== 6. Function parameters and returns ===")

function double(x) {
    return x * 2
}
print(double(5))       // 10
print(double(-10))     // -20
print(double(1.5))     // 3.0

function half(x) {
    return x / 2
}
print(half(10))        // 5 (integer division result)
print(half(9))         // 4.5

function negate(x) {
    return -x
}
print(negate(42))      // -42
print(negate(-100))    // 100

function add_many(a, b, c) {
    a = a + b
return a + c
}
print(add_many(1, 2, 3))       // 6
print(add_many(100, 200, 300)) // 600

// Recursion with compact numbers
function fact(n) {
    if n = 0 {
        return 1
    }
    return n * fact(n - 1)
}
print(fact(5))         // 120
print(fact(10))        // 3628800

print("=== 7. Loop counters ===")

// Small loop
i = 0
while i < 5 {
    i = i + 1
}
print(i)               // 5

// Medium loop (crosses U8 boundary)
j = 250
while j < 260 {
    j = j + 1
}
print(j)               // 260

// Negative loop
k = -5
while k < 0 {
    k = k + 1
}
print(k)               // 0

// Loop with larger values
m = 65530
while m < 65540 {
    m = m + 1
}
print(m)               // 65540

print("=== 8. Bitwise operations ===")

// Small ints
print(5 & 3)           // 1
print(5 | 3)           // 7
print(5 ^ 3)           // 6
print(1 @ 4)           // 16

// Larger values
print(255 & 15)        // 15
print(65535 & 255)     // 255

// Combined with arithmetic
x = 5
print(x @ 2)           // 20

print("=== 9. Complex expressions ===")

// Nested arithmetic
print((5 + 3) * 2)     // 16

// Mix of operations with different type widths
a = 100
b = 200
c = -50
s = a + b
print(s + c)       // 250

// Large chain
print((((1 + 2) + 3) + 4) + 5)  // 15

print("=== 10. Edge cases ===")

// Zero
print(0)
print(-0)              // 0

// Max and min I8
print(-128)
print(127)

// Max U16
print(65535)

// Negation of min I8 (becomes positive)
x = -128
print(-x)              // 128

// In-place mutation
x = 42
x = x + 1
print(x)               // 43
x = x * 10
print(x)               // 430

// Chained through variables
a = 100
b = 200
c = 300
sum = a + b
sum = sum + c
print(sum)             // 600

// Reassignment changes type
n = 5
print(n)               // 5, U8
n = 70000
print(n)               // 70000, U32
n = -1000
print(n)               // -1000, I16
n = 1.5
print(n)               // 1.5, F32
n = 3.14159265358979
print(n)               // pi, F64

print("=== 11. assert with numbers ===")

// assert with truthy number
print(assert(1) = [])   // 1 (assert returned [])

// assert with falsy (0 is truthy! only [] is falsy)
print(assert([]) = [])  // [] (assert returned error string, not [])

// assert catches type errors
print(#assert([] + 5))  // > 0 (error message from '+' type mismatch)

print("=== 12. Sequence of different operations ===")

// Simulating a multi-step computation
balance = 1000
balance = balance + 500
balance = balance - 200
balance = balance * 2
print(balance)          // 2600

rate = 1.5
time = 10
distance = rate * time
print(distance)         // 15.0

// Array access yields compact numbers
arr = [10, 20, 30, 40, 50]
sum = 0
i = 0
while i < 5 {
    sum = sum + arr[i]
    i = i + 1
}
print(sum)             // 150

print("done")
