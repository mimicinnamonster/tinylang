function println(x=0) { print(x) print("\n") }

// === Basic destructure ===
println("--- basic ---")
x, y = [1, 2, 3]
println(x)      // 1
println(y)      // 2

// === Extra RHS elements silently dropped ===
println("--- extra rhs ---")
a, b = [10, 20, 30, 40]
println(a)      // 10
println(b)      // 20

// === Missing RHS elements get nil ===
println("--- missing rhs ---")
c, d, e = [5]
println(c)      // 5
println(d)      // []
println(e)      // []

// === In-place swap ===
println("--- swap ---")
p, q = 100, 200
println(p)      // 100
println(q)      // 200
p, q = q, p
println(p)      // 200
println(q)      // 100

// === Destructure from function return ===
println("--- function return ---")
function mkarr() { return [10, 20, 30] }
r, s = mkarr()
println(r)      // 10
println(s)      // 20

// === Destructure from slice ===
println("--- slice rhs ---")
t, u = mkarr()[1:]
println(t)      // 20
println(u)      // 30

// === Destructure from array variable ===
println("--- array var rhs ---")
src = [100, 200, 300]
d1, d2 = src
println(d1)     // 100
println(d2)     // 200

// === Implicit array on RHS (comma-separated expressions) ===
println("--- implicit array ---")
m, n = 1, 2
println(m)      // 1
println(n)      // 2

// === Destructure from expression (string = byte array) ===
println("--- string rhs ---")
h, e_ = "he"
println(h)      // 104 ('h')
println(e_)     // 101 ('e')

// === Destructure with nested arrays ===
println("--- nested arrays ---")
outer, inner = [[1, 2], [3, 4, 5]]
println(outer)  // [1, 2]
println(inner)  // [3, 4, 5]

// === Chained swap (multiple swaps) ===
println("--- chain swap ---")
a1, a2 = 1, 10
a1, a2 = a2, a1
println(a1)     // 10
println(a2)     // 1
a1, a2 = a2, a1
println(a1)     // 1
println(a2)     // 10

// === Destructure from nil (empty array) ===
println("--- from nil ---")
n1, n2, n3 = nil
println(n1)     // []
println(n2)     // []
println(n3)     // []

// === Destructure with array concat expression ===
println("--- concat rhs ---")
c1, c2 = [1] + [2, 3]
println(c1)     // 1
println(c2)     // 2

// === Four variables ===
println("--- four vars ---")
w, x, y, z = [10, 20, 30, 40, 50]
println(w)      // 10
println(x)      // 20
println(y)      // 30
println(z)      // 40

// === Destructure inside function body ===
println("--- inside function ---")
function first_two(arr=[]) {
    a, b = arr
    return a + b
}
println(first_two([3, 7]))       // 10
println(first_two([100, 200]))   // 300

// === Destructure then verify original array unchanged (COW) ===
println("--- cow ---")
orig = [1, 2, 3]
a, b = orig
a = 99
println(orig[0])    // 1 (COW preserved)
println(orig[1])    // 2
println(a)          // 99

// === Nested destructure with mixed types ===
println("--- mixed types ---")
mixed = [42, "abc", []]
num_part, str_part, nil_part = mixed
println(num_part)   // 42
println(str_part)   // abc
println(nil_part)   // []

// === Destructure with implicitly generated array ===
println("--- implicit large ---")
i1, i2, i3 = 5, 10, 15
println(i1)         // 5
println(i2)         // 10
println(i3)         // 15

// === Multiple assignments share the same RHS array ===
println("--- reuse rhs ---")
fa, fb = [1, 2, 3]
ga, gb = [1, 2, 3]
println(fa)         // 1
println(fb)         // 2
println(ga)         // 1
println(gb)         // 2

// === return with comma-separated values (implicit array) ===
println("--- return comma ---")
function ret_multi() {
    return 1, 2, 3
}
result = ret_multi()
println(result)     // [1, 2, 3]
println(#result)    // 3

// destructure from comma return
ra, rb, rc = ret_multi()
println(ra)         // 1
println(rb)         // 2
println(rc)         // 3

// mixed types in comma return
function ret_mixed() {
    return "hello", 99
}
a, b = ret_mixed()
println(a)          // hello
println(b)          // 99

// single return still works
function ret_one() {
    return 42
}
println(ret_one())  // 42

// return empty array
function ret_empty() {
    return []
}
println(#ret_empty()) // 0
