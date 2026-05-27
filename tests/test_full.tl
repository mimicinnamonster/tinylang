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
print(#z)
print(z[0])
print(z[9])

nils = [] * 5
print(#nils)

pairs = [1, 2] * 3
print(pairs)
print(#pairs)   // 6

// --- 6. Array concatenation ---
print("--- array concat ---")
c = [1, 2] + [3, 4]
print(c)
print(#c)       // 4

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
print(#s)

s2 = "line1\nline2"
print(s2)
print(#s2)

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
print(#noop())    // 0 (returns [])

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

// --- 15. Nested calls ---
print("--- nested calls ---")
print(double(add(3, 4)))   // 14

// --- 17. Value semantics ---
print("--- value semantics ---")
orig = [1, 2, 3]
copy = orig
copy[0] = 99
print(orig[0])    // 1
print(copy[0])    // 99
orig2 = [[1, 2], [3, 4]]
copy2 = orig2
copy2[0][1] = 99
print(orig2[0][1]) // 2 (deep COW: sub-array copied)
print(copy2[0][1]) // 99

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
print(n != [])    // []
print(!n)         // 1

// --- 21. Missing comparisons: <= and >= ---
print("--- <= >= ---")
print(3 <= 5)    // 1
print(5 <= 5)    // 1
print(6 <= 5)    // []
print(5 >= 3)    // 1
print(5 >= 5)    // 1
print(3 >= 5)    // []

// --- 22. Mixed-type comparison ---
print("--- mixed comparison ---")
print(5 = "hello")  // [] (type mismatch = not equal)
print(5 = [])       // []
print([] = "")     // [] (empty array vs string)
print(0 = [])       // []
print(5 != "hello") // 1

// --- 23. Modulo with negatives ---
print("--- negative modulo ---")
print((-5) % 2)  // -1
print(5 % (-2))  // 1
print((-5) % (-2)) // -1

// --- 24. Double negation ---
print("--- double bang ---")
print(!!0)       // 1  (!0=[], ![]=1)
print(!!1)       // 1  (!1=[], ![]=1)
print(!![])      // [] (![]=1, !1=[])

// --- 25. String escape sequences ---
print("--- string escapes ---")
esc_tab = "a\tb"
print(#esc_tab)       // 3 (a, tab, b)
print(esc_tab[0])     // 97 ('a')
print(esc_tab[1])     // 9 (tab)
print(esc_tab[2])     // 98 ('b')
esc_cr = "a\rb"      // \r = 13
print(#esc_cr)        // 3 (a, CR, b)
print(esc_cr[1])      // 13 (CR)
esc_null = "a\0b"    // \0 = 0
print(#esc_null)      // 3 (a, NUL, b)
print(esc_null[1])    // 0 (NUL)
esc_quote = "\""    // \" = 34
print(#esc_quote)     // 1
print(esc_quote[0])   // 34 ('"')
esc_hex = "\x41"     // \x41 = 65 = 'A'
print(#esc_hex)       // 1
print(esc_hex[0])     // 65 ('A')
esc_slash = "a\\b"   // \\ = 92
print(#esc_slash)     // 3 (a, \, b)
print(esc_slash[1])   // 92 ('\\')
empty_str = ""
print(#empty_str)     // 0

// --- 26. Multi-index with 3+ dimensions ---
print("--- deep multi-index ---")
deep = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]]
print(deep[0][0][0])  // 1
print(deep[0,1,1])    // 4 (same as deep[0][1][1])
print(deep[1][0][1])  // 6
print(deep[1,1,0])    // 7

// --- 27. Dynamic chain as lvalue ---
print("--- dynamic chain lvalue ---")
dyn = [[10, 20], [30, 40]]
idx = [0, 1]
dyn[idx] = 99
print(dyn[0][1])      // 99
print(dyn[1][0])      // 30

// --- 28. Function arity flexibility ---
print("--- function arity ---")
function add2(a, b) {
    return a + b
}
print(add2(3, 4))             // 7
print(add2(3, 4, 5, 6))       // 7 (extra args ignored)

function second(a, b) {
    return b
}
print(second(5))              // [] (b defaults to [])
print(second(5, 99))          // 99

function first(a, b) {
    return a
}
print(first(42))              // 42
print(first(42, 99))          // 42

// --- 29. No return (natural fall-through) ---
print("--- no return ---")
function gimme() {
    // no return statement — falls through to return []
}
print(gimme())         // []

function gimme2(n) {
    if n = 0 {
        return 99
    }
    // no else return — falls through on n != 0
}
print(gimme2(0))       // 99
print(gimme2(5))       // []

// --- 30. Nested while ---
print("--- nested while ---")
i = 0
result = []
while i < 3 {
    j = 0
    while j <= i {
        result = result + [i]
        j = j + 1
    }
    i = i + 1
}
print(result)          // [0, 1, 1, 2, 2, 2]
print(#result)         // 6

// --- 31. Array edge cases ---
print("--- array edge cases ---")
zero_repeat = [5] * 0
print(#zero_repeat)   // 0
print(zero_repeat)    // []
neg_repeat = [5] * (-1)
print(#neg_repeat)    // 0
empty_repeat = [] * 3
print(#empty_repeat)  // 0
empty_concat = [] + []
print(#empty_concat)  // 0

// --- 32. # on various types ---
print("--- hash operator ---")
print(#[])           // 0
print(#nil)          // 0 (nil = [])
print(#"abc")        // 3
print(#[[1,2],[3,4]]) // 2

// --- 33. Function calling another function ---
print("--- mutual calls ---")
function inc(x) {
    return x + 1
}
function dec(x) {
    return x - 1
}
function apply_twice(fn, x) {
    // not first-class, so call explicitly
    return x
}
print(inc(dec(5)))  // 5
print(dec(inc(5)))  // 5
print(inc(inc(inc(0)))) // 3

// --- 34. Nested if/elif without else ---
print("--- nested if/elif ---")
function sign(n) {
    if n > 0 {
        return 1
    } elif n < 0 {
        return -1
    }
    return 0
}
print(sign(5))    // 1
print(sign(0))    // 0
print(sign(-3))   // -1

// --- 35. String + number concatenation ---
print("--- string + number concat ---")
print("hello" + 42)          // hello42
print(42 + "hello")          // 42hello
print("zero: " + 0)          // zero: 0
print("neg: " + (-5))        // neg: -5
print("pi: " + 3.14)         // pi: 3.14
print("big: " + 1000000)     // big: 1000000
print(99 + " bottles")       // 99 bottles
print("a" + 1 + "b")         // a1b

function greet(name) {
    return "hello " + name
}
print(greet("world"))        // hello world
print(greet(42))             // hello 42
