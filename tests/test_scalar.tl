// Scalar operations — simplified (no compact types, no bitwise)
// All numbers are doubles, arrays are Value[]

print("=== 1. Basic numbers ===")
print(0)
print(255)
print(256)
print(65535)
print(65536)
print(1.5)
print(3.14159265358979)
print(255 * 255)
print(0)

print("=== 2. Arithmetic operations ===")
print(10 + 20)
print(100 - 30)
print(5 * 6)
print(100 / 3)
print(100 % 3)
print(-10 + 5)

print("=== 3. Negation and sign changes ===")
print(-5)
print(-(-5))
print(-(10 - 15))
print(-0)

print("=== 4. Mixed operations ===")
print(10 + 20)
print(200 + 100)
print(200 + (-50))
print(-100 * 3)
print(200 * 1000)
print(100 + 0.5)
print(42 + 1.5)
print(1.5 + 3.14159265358979)

print("=== 5. Comparisons ===")
print(42 = 42)
print(42 = 43)
print(200 > -100)
print(-100 < 50)
print(50 > -100)
print(70000 > 100)

print("=== 6. Function parameters and returns ===")
function double(x) { return x * 2 }
print(double(5))
print(double(-10))
print(double(1.5))

function half(x) { return x / 2 }
print(half(10))
print(half(9))

function negate(x) { return -x }
print(negate(42))
print(negate(-100))

function add_many(a, b, c) {
    a = a + b
    return a + c
}
print(add_many(10, 20, 30))
print(add_many(-5, 10, -3))

function factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
print(factorial(5))
print(factorial(10))

print("=== 7. Loop counters ===")
sum = 0
i = 0
while i < 10 {
    sum = sum + i
    i = i + 1
}
print(sum)

m = 1
i = 0
while i < 10 {
    m = m * 2
    i = i + 1
}
print(m)

// Overflow check — doubles handle large values
big = 1
i = 0
while i < 100 {
    big = big * 2
    i = i + 1
}
print(big > 0)

print("=== 8. Complex expressions ===")
print(1 + 2 * 3)
print((1 + 2) * 3)
print(10 / 2 + 3 * 2)
print((10 / 2 + 3) * 2)
print(10 / (2 + 3) * 2)

print("=== 9. Edge cases ===")
print(0)
print(-0)
print(1 / 3)
print(10 % 3)
print(-10 % 3)
print(10 % -3)

print("=== 10. Sequence of different operations ===")
balance = 1000
balance = balance + 500
balance = balance - 200
balance = balance * 2
print(balance)

rate = 1.5
time = 10
distance = rate * time
print(distance)

arr = [10, 20, 30, 40, 50]
sum = 0
i = 0
while i < #arr {
    sum = sum + arr[i]
    i = i + 1
}
print(sum)

print("done")
