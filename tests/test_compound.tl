// Compound assignment tests
x = 10
x += 5
print(x)     // 15

x -= 3
print(x)     // 12

x *= 2
print(x)     // 24

x /= 4
print(x)     // 6

// With arrays — indexed
a = [1, 2, 3]
a[0] += 10
print(a[0])  // 11
print(a[1])  // 2
print(a[2])  // 3

// -= *= /= on array elements
a[1] -= 1
print(a[1])  // 1
a[2] *= 3
print(a[2])  // 9
a[1] /= 1
print(a[1])  // 1

// Nested arrays
b = [[1, 2], [3, 4]]
b[0][1] += 5
print(b[0][1])  // 7
b[1][0] *= 2
print(b[1][0])  // 6

// With expression RHS
y = 2
y += 3 * 4
print(y)     // 14

y = 10
y -= 2 * 3
print(y)     // 4

y = 2
y *= 3 + 4
print(y)     // 14

// Inside a function
function test_fn(n) {
    r = n
    r += 10
    r *= 2
    r -= 5
    r /= 3
    return r
}
print(test_fn(5))  // ((5+10)*2-5)/3 = (30-5)/3 = 25/3 = 8.33333? No...
// r=5, r+=10 → 15, r*=2 → 30, r-=5 → 25, r/=3 → 8.33333...
// Let's use integer-friendly values
print(test_fn(0))  // ((0+10)*2-5)/3 = (20-5)/3 = 15/3 = 5

// In a loop
z = 1
i = 0
while i < 5 {
    z *= 2
    i += 1
}
print(z)     // 32

// Push optimization NOT triggered by +=
arr = [1, 2, 3]
arr += [4]
print(#arr)  // 4 (concat, not push — still works)
print(arr[3]) // 4

// Real push still works via explicit = + []
arr2 = []
arr2 = arr2 + [99]
print(arr2[0]) // 99
