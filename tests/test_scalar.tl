// Scalar operations — simplified (no compact types, no bitwise)
// All numbers are doubles, arrays are Value[]

function println(x) { print(x) print("\n") }

println("=== 1. Basic numbers ===")
println(0)
println(255)
println(256)
println(65535)
println(65536)
println(1.5)
println(3.14159265358979)
println(255 * 255)
println(0)

println("=== 2. Arithmetic operations ===")
println(10 + 20)
println(100 - 30)
println(5 * 6)
println(100 / 3)
println(100 % 3)
println(-10 + 5)

println("=== 3. Negation and sign changes ===")
println(-5)
println(-(-5))
println(-(10 - 15))
println(-0)

println("=== 4. Mixed operations ===")
println(10 + 20)
println(200 + 100)
println(200 + (-50))
println(-100 * 3)
println(200 * 1000)
println(100 + 0.5)
println(42 + 1.5)
println(1.5 + 3.14159265358979)

println("=== 5. Comparisons ===")
println(42 == 42)
println(42 == 43)
println(200 > -100)
println(-100 < 50)
println(50 > -100)
println(70000 > 100)

println("=== 6. Function parameters and returns ===")
function double(x) { return x * 2 }
println(double(5))
println(double(-10))
println(double(1.5))

function half(x) { return x / 2 }
println(half(10))
println(half(9))

function negate(x) { return -x }
println(negate(42))
println(negate(-100))

function add_many(a, b, c) {
    a = a + b
    return a + c
}
println(add_many(10, 20, 30))
println(add_many(-5, 10, -3))

function factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
println(factorial(5))
println(factorial(10))

println("=== 7. Loop counters ===")
sum = 0
i = 0
while i < 10 {
    sum = sum + i
    i = i + 1
}
println(sum)

m = 1
i = 0
while i < 10 {
    m = m * 2
    i = i + 1
}
println(m)

// Overflow check — doubles handle large values
big = 1
i = 0
while i < 100 {
    big = big * 2
    i = i + 1
}
println(big > 0)

println("=== 8. Complex expressions ===")
println(1 + 2 * 3)
println((1 + 2) * 3)
println(10 / 2 + 3 * 2)
println((10 / 2 + 3) * 2)
println(10 / (2 + 3) * 2)

println("=== 9. Edge cases ===")
println(0)
println(-0)
println(1 / 3)
println(10 % 3)
println(-10 % 3)
println(10 % -3)

println("=== 10. Sequence of different operations ===")
balance = 1000
balance = balance + 500
balance = balance - 200
balance = balance * 2
println(balance)

rate = 1.5
time = 10
distance = rate * time
println(distance)

arr = [10, 20, 30, 40, 50]
sum = 0
i = 0
while i < #arr {
    sum = sum + arr[i]
    i = i + 1
}
println(sum)

println("done")
