// Comprehensive test of all TinyLang features

// --- 1. Numbers ---
print("--- numbers ---")
x = 42
print(x)
y = 3.14
print(y)
print(x + y)
print(x - y)
print(x * y)
print(x / 2)
print(x % 7)
print(-5)

// --- 2. Comparisons ---
print("--- comparisons ---")
print(5 = 5)     // 1
print(5 = 6)     // []
print(5 != 6)    // 1
print(5 < 6)     // 1
print(5 > 6)     // []
print(5 > 3)     // 1

// --- 3. Truthiness ---
print("--- truthiness ---")
print(![])      // 1
print(!0)       // []
print(!1)       // []
print(!5)       // []
print(!!5)      // 1

// --- 4. Arrays ---
print("--- arrays ---")
a = [1, 2, 3]
print(a)
e = []
print(e)
mix = [1, "hello", [], [5, 6]]
print(mix)

// array index
print(a[0])
print(a[2])

// multi-index
m = [[1, 2], [3, 4]]
print(m[0][1])
print(m[1][0])
print(m[0, 1])    // same as m[0][1]

// lvalue assignment
a[0] = 99
print(a)

// nested lvalue
m[0][1] = 99
print(m)

// --- 5. Array repetition ---
print("--- array repetition ---")
z = [0] * 10
print(len(z))
print(z[0])
print(z[9])

nils = [] * 5
print(len(nils))

pairs = [1, 2] * 3
print(pairs)
print(len(pairs))   // 6

// --- 6. Array concatenation ---
print("--- array concat ---")
c = [1, 2] + [3, 4]
print(c)
print(len(c))       // 4

// --- 7. Array equality ---
print("--- array equality ---")
print([1, 2] = [1, 2])     // 1 (deep equal)
print([1, 2] = [1, 3])     // [] (not equal)
print([] = [])              // 1 (both empty)

// --- 8. Strings ---
print("--- strings ---")
s = "hello world"
print(s)
print(s[0])     // 104 'h'
print(s[4])     // 111 'o'
print(len(s))

s2 = "line1\nline2"
print(s2)
print(len(s2))

// --- 9. Functions ---
print("--- functions ---")
function double(x) {
    return x * 2
}
print(double(5))
print(double(double(5)))

function add(a, b) {
    return a + b
}
print(add(3, 4))

function noop() {
}
print(len(noop()))    // 0 (returns [])

// --- 10. Recursion ---
print("--- recursion ---")
function fact(n) {
    if n = 0 {
        return 1
    }
    return n * fact(n - 1)
}
print(fact(0))   // 1
print(fact(5))   // 120
print(fact(10))  // 3628800

// --- 11. if/elif/else ---
print("--- if/elif/else ---")
function classify(n) {
    if n < 0 {
        return -1
    } elif n = 0 {
        return 0
    } else {
        return 1
    }
}
print(classify(-5))
print(classify(0))
print(classify(42))

// --- 12. while ---
print("--- while ---")
i = 0
while i < 5 {
    i = i + 1
}
print(i)          // 5

function fact_while(n) {
    result = 1
    i = 1
    while i <= n {
        result = result * i
        i = i + 1
    }
    return result
}
print(fact_while(5))   // 120

// --- 13. Linked list (nested arrays) ---
print("--- linked list ---")
list = [1, [2, [3, []]]]
print(list)

function sum_list(lst) {
    if lst = [] {
        return 0
    }
    return lst[0] + sum_list(lst[1])
}
print(sum_list(list))   // 6

// --- 14. Manual heap (inline, no functions) ---
print("--- manual heap ---")
nodes = [[0, -1]] * 5
ptr = 0
nodes[0][0] = 10
nodes[0][1] = -1
nodes[1][0] = 20
nodes[1][1] = 0
nodes[2][0] = 30
nodes[2][1] = 1
print(nodes[2][0])    // 30
print(nodes[2][1])    // 1

// traverse with while
ptr = 2
while ptr != -1 {
    print(nodes[ptr][0])
    ptr = nodes[ptr][1]
}

// --- 15. Bitwise ops ---
print("--- bitwise ---")
print(5 & 3)     // 1
print(5 | 3)     // 7
print(5 ^ 3)     // 6
print(1 @ 4)     // 16 (left shift by 4)
print(16 @ -2)   // 4 (right shift by 2)

// --- 16. Nested calls ---
print("--- nested calls ---")
print(double(add(3, 4)))   // 14

// --- 17. Value semantics ---
print("--- value semantics ---")
orig = [1, 2, 3]
copy = orig
copy[0] = 99
print(orig[0])    // 1
print(copy[0])    // 99

// --- 18. Multi-index lvalue ---
print("--- multi-index lvalue ---")
mat = [[10, 20], [30, 40]]
mat[0, 1] = 99
print(mat[0][1])   // 99
print(mat[1][0])   // 30

// --- 19. Dynamic chain index ---
print("--- dynamic chain ---")
mat2 = [[1, 2], [3, 4]]
idx = [0, 1]
print(mat2[idx])   // 2

// --- 20. nil ---
print("--- nil ---")
n = nil
print(n = [])     // 1
