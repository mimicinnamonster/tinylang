// Comprehensive test of all TinyLang features

function println(x) { print(x) print("\n") }

// --- 1. Numbers ---
println("--- numbers ---")
x = 42
println(x)
y = 3.14
println(y)
println(x + y)
println(x - y)
println(x * y)
println(x / 2)
println(x % 7)
println(-5)

// --- 2. Comparisons ---
println("--- comparisons ---")
println(5 = 5)     // 1
println(5 = 6)     // []
println(5 != 6)    // 1
println(5 < 6)     // 1
println(5 > 6)     // []
println(5 > 3)     // 1

// --- 3. Truthiness ---
println("--- truthiness ---")
println(![])      // 1
println(!0)       // []
println(!1)       // []
println(!5)       // []
println(!!5)      // 1

// --- 4. Arrays ---
println("--- arrays ---")
a = [1, 2, 3]
println(a)
e = []
println(e)
mix = [1, "hello", [], [5, 6]]
println(mix)

// array index
println(a[0])
println(a[2])

// multi-index
m = [[1, 2], [3, 4]]
println(m[0][1])
println(m[1][0])
println(m[0, 1])    // same as m[0][1]

// lvalue assignment
a[0] = 99
println(a)

// nested lvalue
m[0][1] = 99
println(m)

// --- 5. Array repetition ---
println("--- array repetition ---")
z = [0] * 10
println(#z)
println(z[0])
println(z[9])

nils = [] * 5
println(#nils)

pairs = [1, 2] * 3
println(pairs)
println(#pairs)   // 6

// --- 6. Array concatenation ---
println("--- array concat ---")
c = [1, 2] + [3, 4]
println(c)
println(#c)       // 4

// --- 7. Array equality ---
println("--- array equality ---")
println([1, 2] = [1, 2])     // 1 (deep equal)
println([1, 2] = [1, 3])     // [] (not equal)
println([] = [])              // 1 (both empty)

// --- 8. Strings ---
println("--- strings ---")
s = "hello world"
println(s)
println(s[0])     // 104 'h'
println(s[4])     // 111 'o'
println(#s)

s2 = "line1\nline2"
println(s2)
println(#s2)

// --- 9. Functions ---
println("--- functions ---")
function double(x) {
    return x * 2
}
println(double(5))
println(double(double(5)))

function add(a, b) {
    return a + b
}
println(add(3, 4))

function noop() {
}
println(#noop())    // 0 (returns [])

// --- 10. Recursion ---
println("--- recursion ---")
function fact(n) {
    if n = 0 {
        return 1
    }
    return n * fact(n - 1)
}
println(fact(0))   // 1
println(fact(5))   // 120
println(fact(10))  // 3628800

// --- 11. if/elif/else ---
println("--- if/elif/else ---")
function classify(n) {
    if n < 0 {
        return -1
    } elif n = 0 {
        return 0
    } else {
        return 1
    }
}
println(classify(-5))
println(classify(0))
println(classify(42))

// --- 12. while ---
println("--- while ---")
i = 0
while i < 5 {
    i = i + 1
}
println(i)          // 5

function fact_while(n) {
    result = 1
    i = 1
    while i <= n {
        result = result * i
        i = i + 1
    }
    return result
}
println(fact_while(5))   // 120

// --- 13. Linked list (nested arrays) ---
println("--- linked list ---")
list = [1, [2, [3, []]]]
println(list)

function sum_list(lst) {
    if lst = [] {
        return 0
    }
    return lst[0] + sum_list(lst[1])
}
println(sum_list(list))   // 6

// --- 14. Manual heap (inline, no functions) ---
println("--- manual heap ---")
nodes = [[0, -1]] * 5
ptr = 0
nodes[0][0] = 10
nodes[0][1] = -1
nodes[1][0] = 20
nodes[1][1] = 0
nodes[2][0] = 30
nodes[2][1] = 1
println(nodes[2][0])    // 30
println(nodes[2][1])    // 1

// traverse with while
ptr = 2
while ptr != -1 {
    println(nodes[ptr][0])
    ptr = nodes[ptr][1]
}

// --- 15. Nested calls ---
println("--- nested calls ---")
println(double(add(3, 4)))   // 14

// --- 17. Value semantics ---
println("--- value semantics ---")
orig = [1, 2, 3]
copy = orig
copy[0] = 99
println(orig[0])    // 1
println(copy[0])    // 99
orig2 = [[1, 2], [3, 4]]
copy2 = orig2
copy2[0][1] = 99
println(orig2[0][1]) // 2 (deep COW: sub-array copied)
println(copy2[0][1]) // 99

// --- 18. Multi-index lvalue ---
println("--- multi-index lvalue ---")
mat = [[10, 20], [30, 40]]
mat[0, 1] = 99
println(mat[0][1])   // 99
println(mat[1][0])   // 30

// --- 19. Dynamic chain index ---
println("--- dynamic chain ---")
mat2 = [[1, 2], [3, 4]]
idx = [0, 1]
println(mat2[idx])   // 2

// --- 20. nil ---
println("--- nil ---")
n = nil
println(n = [])     // 1
println(n != [])    // []
println(!n)         // 1

// --- 21. Missing comparisons: <= and >= ---
println("--- <= >= ---")
println(3 <= 5)    // 1
println(5 <= 5)    // 1
println(6 <= 5)    // []
println(5 >= 3)    // 1
println(5 >= 5)    // 1
println(3 >= 5)    // []

// --- 22. Mixed-type comparison ---
println("--- mixed comparison ---")
println(5 = "hello")  // [] (type mismatch = not equal)
println(5 = [])       // []
println([] = "")     // [] (empty array vs string)
println(0 = [])       // []
println(5 != "hello") // 1

// --- 23. Modulo with negatives ---
println("--- negative modulo ---")
println((-5) % 2)  // -1
println(5 % (-2))  // 1
println((-5) % (-2)) // -1

// --- 24. Double negation ---
println("--- double bang ---")
println(!!0)       // 1  (!0=[], ![]=1)
println(!!1)       // 1  (!1=[], ![]=1)
println(!![])      // [] (![]=1, !1=[])

// --- 25. String escape sequences ---
println("--- string escapes ---")
esc_tab = "a\tb"
println(#esc_tab)       // 3 (a, tab, b)
println(esc_tab[0])     // 97 ('a')
println(esc_tab[1])     // 9 (tab)
println(esc_tab[2])     // 98 ('b')
esc_cr = "a\rb"      // \r = 13
println(#esc_cr)        // 3 (a, CR, b)
println(esc_cr[1])      // 13 (CR)
esc_null = "a\0b"    // \0 = 0
println(#esc_null)      // 3 (a, NUL, b)
println(esc_null[1])    // 0 (NUL)
esc_quote = "\""    // \" = 34
println(#esc_quote)     // 1
println(esc_quote[0])   // 34 ('"')
esc_hex = "\x41"     // \x41 = 65 = 'A'
println(#esc_hex)       // 1
println(esc_hex[0])     // 65 ('A')
esc_slash = "a\\b"   // \\ = 92
println(#esc_slash)     // 3 (a, \, b)
println(esc_slash[1])   // 92 ('\\')
empty_str = ""
println(#empty_str)     // 0

// --- 26. Multi-index with 3+ dimensions ---
println("--- deep multi-index ---")
deep = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]]
println(deep[0][0][0])  // 1
println(deep[0,1,1])    // 4 (same as deep[0][1][1])
println(deep[1][0][1])  // 6
println(deep[1,1,0])    // 7

// --- 27. Dynamic chain as lvalue ---
println("--- dynamic chain lvalue ---")
dyn = [[10, 20], [30, 40]]
idx = [0, 1]
dyn[idx] = 99
println(dyn[0][1])      // 99
println(dyn[1][0])      // 30

// --- 28. Function arity flexibility ---
println("--- function arity ---")
function add2(a, b) {
    return a + b
}
println(add2(3, 4))             // 7
println(add2(3, 4, 5, 6))       // 7 (extra args ignored)

function second(a, b) {
    return b
}
println(second(5))              // [] (b defaults to [])
println(second(5, 99))          // 99

function first(a, b) {
    return a
}
println(first(42))              // 42
println(first(42, 99))          // 42

// --- 29. No return (natural fall-through) ---
println("--- no return ---")
function gimme() {
    // no return statement — falls through to return []
}
println(gimme())         // []

function gimme2(n) {
    if n = 0 {
        return 99
    }
    // no else return — falls through on n != 0
}
println(gimme2(0))       // 99
println(gimme2(5))       // []

// --- 30. Nested while ---
println("--- nested while ---")
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
println(result)          // [0, 1, 1, 2, 2, 2]
println(#result)         // 6

// --- 31. Array edge cases ---
println("--- array edge cases ---")
zero_repeat = [5] * 0
println(#zero_repeat)   // 0
println(zero_repeat)    // []
neg_repeat = [5] * (-1)
println(#neg_repeat)    // 0
empty_repeat = [] * 3
println(#empty_repeat)  // 0
empty_concat = [] + []
println(#empty_concat)  // 0

// --- 32. # on various types ---
println("--- hash operator ---")
println(#[])           // 0
println(#nil)          // 0 (nil = [])
println(#"abc")        // 3
println(#[[1,2],[3,4]]) // 2

// --- 33. Function calling another function ---
println("--- mutual calls ---")
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
println(inc(dec(5)))  // 5
println(dec(inc(5)))  // 5
println(inc(inc(inc(0)))) // 3

// --- 34. Nested if/elif without else ---
println("--- nested if/elif ---")
function sign(n) {
    if n > 0 {
        return 1
    } elif n < 0 {
        return -1
    }
    return 0
}
println(sign(5))    // 1
println(sign(0))    // 0
println(sign(-3))   // -1

// --- 35. String + number concatenation ---
println("--- string + number concat ---")
println("hello" + 42)          // hello42
println(42 + "hello")          // 42hello
println("zero: " + 0)          // zero: 0
println("neg: " + (-5))        // neg: -5
println("pi: " + 3.14)         // pi: 3.14
println("big: " + 1000000)     // big: 1000000
println(99 + " bottles")       // 99 bottles
println("a" + 1 + "b")         // a1b

function greet(name) {
    return "hello " + name
}
println(greet("world"))        // hello world
println(greet(42))             // hello 42
