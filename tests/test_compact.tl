// Compact array tests — full type hierarchy
// Three independent axes: size(8/16/32/64), signedness(unsigned/signed), kind(int/float/value)

print("=== 1. Type detection ===")

// U8: [0, 255]
u8  = [0, 100, 200, 255]
print(u8)

// U16: [256, 65535]
u16 = [256, 1000, 50000, 65535]
print(u16)

// U32: [65536, 4294967295]
u32 = [70000, 100000, 4000000000]
print(u32)

// U64: above 2^32
u64 = [5000000000, 10000000000, 100000000000]
print(u64)

// I8: [-128, 127]
i8  = [-100, -1, 0, 100, 127]
print(i8)

// I16: [-32768, 32767]
i16 = [-30000, -1000, 0, 1000, 30000, 32767]
print(i16)

// I32: [-2147483648, 2147483647]
i32 = [-1000000000, -100000, 0, 100000, 2000000000]
print(i32)

// I64: beyond I32 range
i64 = [-5000000000, -3000000000, 0, 5000000000, 10000000000]
print(i64)

// F32: exactly representable in float
f32 = [1.5, 2.5, 3.25, 0.125, 100.5]
print(f32)

// F64: too precise for float
f64 = [3.14159265358979, 2.718281828459045, 0.1, 0.2]
print(f64)

// VAL: mixed types
mixed = [1, "hello", 3.5, []]
print(mixed)

print("=== 2. Boundary values ===")

// At type edges, should pick the tightest fit
all_unsigned = [127, 128]       // both positive → U8, not I8
print(all_unsigned)

signed_boundary = [-128, 127]    // negative present → I8
print(signed_boundary)

u16_boundary = [255, 256]       // 256 > 255 → U16
print(u16_boundary)

i16_boundary = [128, 32767]     // 128 fits in I8 but 32767 needs I16 → should we get I16?
print(i16_boundary)

i32_vs_i16 = [32768, 100000]    // 32768 > 32767 → I32 (or if all positive → U16? 100000 > 65535 → U32)
print(i32_vs_i16)

// Non-integer with specific boundary
f32_exact = [16777216]           // 2^24, exactly representable in F32... but it's an integer!
// Actually this is U32/I32 since it's a whole integer
print(f32_exact)

f32_nonexact = [16777217]        // 2^24+1, NOT exactly representable in F32, but IS a whole integer
print(f32_nonexact)

print("=== 3. Repetition ===")

// [5] * 5 → U8 (positive, fits in 8 bits)
r_u8 = [5] * 5
print(r_u8)
print(#r_u8)

// [257] * 3 → U16
r_u16 = [257] * 3
print(r_u16)

// [70000] * 4 → U32
r_u32 = [70000] * 4
print(r_u32)

// [5000000000] * 2 → U64
r_u64 = [5000000000] * 2
print(r_u64)

// [-5] * 4 → I8
r_i8 = [-5] * 4
print(r_i8)

// [-30000] * 3 → I16
r_i16 = [-30000] * 3
print(r_i16)

// [-1000000000] * 2 → I32
r_i32 = [-1000000000] * 2
print(r_i32)

// [-5000000000] * 2 → I64
r_i64 = [-5000000000] * 2
print(r_i64)

// [5.5] * 3 → F32
r_f32 = [5.5] * 3
print(r_f32)

// [3.14159265358979] * 2 → F64
r_f64 = [3.14159265358979] * 2
print(r_f64)

// Single element compact arrays
r_one = [42] * 1
print(r_one)
print(#r_one)

// Zero and negative repeat
r_zero = [5] * 0
print(#r_zero)
print(r_zero)          // []

r_neg = [5] * (-3)
print(#r_neg)           // 0

print("=== 4. Concat / promotion ===")

// Same kind: kind preserved
print([1, 2] + [3, 4])          // U8 + U8 → U8

// Unsigned + signed → wider signed
print([100, 200] + [-50, -100]) // U8 + I8 → I16
print([50000] + [-100])         // U16 + I8 → I32

// Wider unsigned + narrower unsigned → wider
print([100] + [1000])           // U8 + U16 → U16
print([100] + [70000])          // U8 + U32 → U32
print([100] + [5000000000])     // U8 + U64 → U64

// Signed + wider signed → wider signed
print([-100, 100] + [-30000, 30000])     // I8 + I16 → I16
print([-100, 100] + [-1000000000, 1000000000]) // I8 + I32 → I32

// Integer + float → F64 (if I32 doesn't fit in F32 mantissa)
print([100, 200] + [1.5, 2.5])          // small int + F32 → F32 (fits in F32 mantissa)
print([100000, 200000] + [1.5, 2.5])    // larger int + F32 → F64 (I32 doesn't fit F32 exactly)

// F32 + F64 → F64
print([1.5, 2.5] + [3.14159265358979])

// Empty + non-empty
print([] + [1, 2, 3])
print([1, 2, 3] + [])

print("=== 5. Index reads ===")

// Read from U8
a = [10, 20, 30]
print(a[0])
print(a[2])

// Read from U16
b = [256, 1000, 65535]
print(b[1])

// Read from U32
c = [70000, 4000000000]
print(c[0])

// Read from U64
d = [5000000000, 10000000000]
print(d[1])

// Read from I8
e = [-100, 0, 100]
print(e[0])
print(e[2])

// Read from I16
f = [-30000, 30000]
print(f[1])

// Read from I32
g = [-100000, 2000000000]
print(g[1])

// Read from I64
h = [-5000000000, 10000000000]
print(h[0])

// Read from F32
i = [1.5, 2.5]
print(i[1])

// Read from F64
j = [3.14159265358979]
print(j[0])

// Out of bounds error (need a variable since literal[idx] isn't valid syntax)
oob_arr = [1, 2, 3]
e_oob_result = assert(oob_arr[5])
print(#e_oob_result)    // should be > 0 (error message length)

print("=== 6. Mutation (promotes to VAL) ===")

// Write to U8 → promotes to VAL
val_u8 = [1, 2, 3, 4, 5]
val_u8[0] = 99
print(val_u8)
val_u8[2] = 5.5
print(val_u8)

// Write to I16 → promotes to VAL
val_i16 = [100, 200, 300, 400]
val_i16[1] = -50
print(val_i16)

// Write to F64 → promotes to VAL
val_f64 = [3.14159, 2.71828]
val_f64[0] = 1.0
print(val_f64)

// Nested write (inner compact array promotes)
nested = [[1, 2], [3, 4]]
nested[0][1] = 99
print(nested[0][1])     // 99
print(nested[0][0])     // 1 (unchanged)

// COW: writing to copy doesn't affect original
orig = [10, 20, 30]
copy = orig
copy[0] = 99
print(orig[0])          // 10
print(copy[0])          // 99

print("=== 7. Push optimization ===")

// Push to existing array
p = [1, 2, 3]
p = p + [4]
p = p + [5]
print(p)
print(#p)

// Push to nil
q = []
q = q + [1]
q = q + [2]
q = q + [3]
print(q)
print(#q)

// Push in a loop
r = []
i = 0
while i < 20 {
    r = r + [i]
    i = i + 1
}
print(r)
print(#r)

// Push large values (promotes)
s = [1, 2]
s = s + [1000000]    // element doesn't fit in U8... but detects from literal [1000000]
print(s)

print("=== 8. Equality ===")

// Same kind
print([1, 2, 3] = [1, 2, 3])       // 1
print([1, 2, 3] = [1, 2, 99])      // []

// Different kinds, same values (should be equal)
print([100, 200] = [100, 200])     // 1 (U8 or I8? both positive, so U8=U8)

// Mixed kinds in concat result
a1 = [100, 200] + [300, 400]       // U8 + U8 → U8
a2 = [100, 200] + [300, 400]       // same
print(a1 = a2)                     // 1

// Deep equality with different types
deep1 = [[1, 2], [3, 4]]
deep2 = [[1, 2], [3, 4]]
deep3 = [[1, 2], [3, 99]]
print(deep1 = deep2)               // 1
print(deep1 = deep3)               // []

// Empty array equality
print([] = [])                     // 1

// String equality (strings are U8 arrays)
print("hello" = "hello")           // 1
print("hello" = "world")           // []

print("=== 9. String operations ===")

// Strings are stored as U8/I8
str = "hello world"
print(str)
print(#str)          // 11
print(str[0])        // 104 ('h')
print(str[4])        // 111 ('o')
print(str[10])       // 100 ('d')

// Escape sequences
tab = "a\tb"
print(#tab)          // 3
print(tab[1])        // 9

newline = "line1\nline2"
print(newline[4])    // 49 ('1')
print(newline[5])    // 10 ('\n')
print(newline[6])    // 108 ('l')

// Empty string
empty = ""
print(#empty)        // 0

print("=== 10. Multi-index / dynamic chain ===")

// Multi-index on compact arrays
mat = [[1, 2], [3, 4]]
print(mat[0][1])      // 2
print(mat[1][0])      // 3
print(mat[0, 1])      // 2 (comma syntax)

// Dynamic chain index
mat2 = [[10, 20], [30, 40]]
idx = [0, 1]
print(mat2[idx])      // 20

// Dynamic chain lvalue
mat3 = [[1, 2], [3, 4]]
dyn_idx = [1, 0]
mat3[dyn_idx] = 99
print(mat3[1][0])     // 99
print(mat3[0][0])     // 1

// 3D
deep = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]]
print(deep[0][0][0])  // 1
print(deep[0, 1, 1])  // 4
print(deep[1][0][1])  // 6

print("=== 11. Hash operator (#) ===")

print(#u8)            // 4
print(#u16)           // 4
print(#u32)           // 3
print(#u64)           // 3
print(#i8)            // 5
print(#i16)           // 6
print(#i32)           // 5
print(#i64)           // 5
print(#f32)           // 5
print(#f64)           // 4
print(#mixed)         // 4
print(#[])            // 0

print("=== 12. Truthiness ===")

print(![])            // 1
print(![0])           // []
print(![5])           // []
print(![[]])          // []
print(![[1, 2]])      // []

print("=== 13. The manual heap pattern ===")

// This is the original use case that motivated compact arrays
nodes = [[0, -1]] * 100
print(#nodes)          // 100

// Write into the heap
nodes[0][0] = 10
nodes[0][1] = -1
nodes[1][0] = 20
nodes[1][1] = 0
nodes[2][0] = 30
nodes[2][1] = 1

// Read back
print(nodes[0][0])     // 10
print(nodes[2][1])     // 1
print(nodes[1][0])     // 20

// Traverse
ptr = 2
sum = 0
while ptr != -1 {
    sum = sum + nodes[ptr][0]
    ptr = nodes[ptr][1]
}
print(sum)             // 30 + 20 + 10 = 60

print("=== 14. Large arrays (>64 elements) ===")

// Arrays > 64 elements from repetition still use compact types (kind from source)
big = [1] * 100
print(#big)            // 100
print(big[0])          // 1
print(big[99])         // 1

// Large U32 array
big_u32 = [1000000] * 50
print(#big_u32)
print(big_u32[49])

// Large F32 array
big_f32 = [1.5] * 75
print(#big_f32)
print(big_f32[74])

// Large mixed array (forces VAL since > 64 elements in literal... wait, repetition is different)
big_arr = [1, 2, 3, 4] * 25
print(#big_arr)
print(big_arr[99])

print("=== 15. Functions with compact arrays ===")

function first(arr) {
    return arr[0]
}
print(first([10, 20, 30]))           // 10
print(first([1000, 2000, 3000]))     // 1000
print(first([1.5, 2.5]))            // 1.5

function sum_arr(arr) {
    i = 0
    total = 0
    while i < #arr {
        total = total + arr[i]
        i = i + 1
    }
    return total
}
print(sum_arr([1, 2, 3, 4, 5]))     // 15
print(sum_arr([10, 20, 30]))         // 60

function mk_array(n) {
    result = []
    i = 0
    while i < n {
        result = result + [i]
        i = i + 1
    }
    return result
}
print(sum_arr(mk_array(10)))         // 45

print("=== 16. Compact arrays in conditionals ===")

if [1, 2, 3] {
    print(1)    // truthy
}
if [] {
    print(2)    // never reached
} else {
    print(3)    // falsy, reaches here
}

// Compact array as comparison target
if [1, 2, 3] = [1, 2, 3] {
    print(4)
}

print("=== 17. Nested function returns ===")

// Return compact array from function
function make_range(n) {
    r = []
    i = 0
    while i < n {
        r = r + [i]
        i = i + 1
    }
    return r
}
rng = make_range(5)
print(rng)
print(#rng)

// Return compact via literal
function identity(x) {
    return x
}
print(identity([1, 2, 3]))
print(identity([1.5, 2.5]))

print("=== 18. assert returns compact strings ===")

// assert with truthy expression returns nil
print(assert(1) = [])    // 1

// assert with falsy expression returns error string
err = assert([])
print(#err)               // > 0
print(err[0])             // 97 ('a')

print("done")
