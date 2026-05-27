// Test zero-copy slice views with COW semantics
// Slices [start:stop] with step=1 should share the parent's backing store,
// only copying on mutation (COW).

function println(x=0) { print(x) print("\n") }

// === 1. Basic view — read through slice ===
arr = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
view = arr[2:7]
println(view)              // [2, 3, 4, 5, 6]
println(#view)             // 5

// === 2. Default bounds ===
println(arr[:3])           // [0, 1, 2]
println(arr[7:])           // [7, 8, 9]
println(arr[:])            // [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

// === 3. Negative indices ===
println(arr[-3:])          // [7, 8, 9]
println(arr[:-3])          // [0, 1, 2, 3, 4, 5, 6]
println(arr[-5:-2])        // [5, 6, 7]

// === 4. OOB clamped ===
println(arr[0:100])        // [0..9]
println(arr[100:200])      // []

// === 5. COW: mutating a view should not affect parent ===
orig = [10, 20, 30, 40, 50]
v = orig[1:4]
println(v)                 // [20, 30, 40]
v[0] = 99                  // COW flatten — v becomes owned copy
println(v)                 // [99, 30, 40]
println(orig)              // [10, 20, 30, 40, 50] (unchanged)

// === 6. COW: mutating parent should not affect existing views ===
base = [1, 2, 3, 4, 5]
sv = base[1:4]
println(sv)                // [2, 3, 4]
base[2] = 99               // mutate parent
println(base)              // [1, 2, 99, 4, 5]
println(sv)                // [2, 3, 4] (should be unchanged because sv is separate)

// Wait — actually sv is a view into base. If we mutate base[2], does sv[1] change?
// sv = view with parent=base, offset=1, len=3
// sv.val = base.val + 1
// base.val[2] = 99 → sv.val[1] becomes 99
// This is expected for a view! The view shares memory.
// If user wants independence, they should copy explicitly.
// But with COW: when sv was created, base wasn't mutated yet.
// After sv creation, mutating base: does the view still see it?

// Actually, the view shares the parent's val[]. If parent mutates via
// amake_uniq (which happens when base[2]=99), the parent gets a new copy
// and the view still points to the old backing store.
// Let's verify:

// sv was created when base had no other references.
// base[2] = 99: amake_uniq checks base.arr->refcount.
// If sv's view holds parent=base.arr, base.arr refcount >= 2
// So amake_uniq copies! base gets a new copy, sv's view still points to old data.
// So sv should be [2, 3, 4], base should be [1, 2, 99, 4, 5]

println("")                  // separator

// Let me verify this more carefully:
a = [100, 200, 300, 400, 500]
b = a[1:4]                   // b = view: [200, 300, 400]
println(b)                   // [200, 300, 400]
a[1] = 999                   // COW: a gets new copy, b still points to old
println(a)                   // [100, 999, 300, 400, 500]
println(b)                   // [200, 300, 400] (unchanged, view kept old backing)

// === 7. x = x[slice] — should be a view, no copy ===
x = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
x = x[3:8]
println(x)                   // [3, 4, 5, 6, 7]
println(#x)                  // 5
// Mutate x — should COW flatten
x[0] = 99
println(x)                   // [99, 4, 5, 6, 7]

// === 8. Strided slices still copy (step != 1) ===
s = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
println(s[0:10:2])           // [0, 2, 4, 6, 8] (copy, but same result)
println(s[::-1])             // [9, 8, 7, 6, 5, 4, 3, 2, 1, 0]
println(s[5:2:-1])           // [5, 4, 3]

// === 9. Empty slice from view ===
empty_arr = [1, 2, 3]
println(empty_arr[0:0])      // []

// === 10. Nested array view ===
matrix = [[1, 2], [3, 4], [5, 6]]
row_view = matrix[1:3]       // view of last two rows
println(row_view)            // [[3, 4], [5, 6]]
println(#row_view)           // 2
println(row_view[0])         // [3, 4]
// Mutate element of row_view — COW should flatten row_view
row_view[0][1] = 99
println(row_view)            // [[3, 99], [5, 6]]
println(matrix)              // [[1, 2], [3, 4], [5, 6]] (unchanged)

// === 11. Concatenation with views ===
parts = [10, 20, 30]
v1 = parts[0:2]              // [10, 20]
v2 = parts[1:3]              // [20, 30]
combined = v1 + v2
println(combined)            // [10, 20, 20, 30]

// === 12. Multiple views, independent COW ===
data = [1, 2, 3, 4, 5, 6]
va = data[0:3]               // [1, 2, 3]
vb = data[3:6]               // [4, 5, 6]
println(va)                  // [1, 2, 3]
println(vb)                  // [4, 5, 6]
va[1] = 99                   // COW flatten va
vb[1] = 88                   // COW flatten vb
println(va)                  // [1, 99, 3]
println(vb)                  // [4, 88, 6]
println(data)                // [1, 2, 3, 4, 5, 6] (unchanged)

// === 13. Slice of a slice (should work via parent chain) ===
// Note: a view's val already points into parent, so slicing a view
// creates another view pointing into the same parent
all = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
inner = all[2:8]             // view: [2, 3, 4, 5, 6, 7]
inner2 = inner[1:4]          // view of view: [3, 4, 5]
println(inner2)              // [3, 4, 5]

// === 14. Push onto a slice variable (COW flatten + push) ===
p = [1, 2, 3]
pv = p[0:2]                  // view: [1, 2]
pv = pv + [99]               // COW flatten, then push
println(pv)                  // [1, 2, 99]
println(p)                   // [1, 2, 3] (unchanged)

// === 15. Index through view ===
items = [10, 20, 30, 40, 50]
iv = items[1:4]              // view: [20, 30, 40]
println(iv[0])               // 20
println(iv[2])               // 40
println(iv[0] + iv[2])       // 60

// === 16. Length through view ===
empty_view = items[2:2]
println(#empty_view)         // 0
println(#items[1:4])        // 3

// === 17. Comparison with views ===
a1 = [1, 2, 3, 4, 5]
b1 = [1, 2, 3, 4, 5]
va1 = a1[0:3]               // [1, 2, 3]
vb1 = b1[0:3]               // [1, 2, 3]
println(va1 == vb1)         // 1 (deep equal, views read from backing)

// === 18. Expression indices for slice bounds ===
lo = 2
hi = 7
zz = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
println(zz[lo:hi])           // [2, 3, 4, 5, 6]

// === 19. Verify that repeated x = x[slice] creates views correctly ===
c = [100, 200, 300, 400, 500, 600, 700, 800]
c = c[2:6]                   // view: [300, 400, 500, 600]
c = c[1:3]                   // view: [400, 500]
println(c)                   // [400, 500]
println(#c)                  // 2

// === 20. In-place optimization: x = x[1:] on exclusive array ===
// When exclusively owned with numbers, this should memmove+trim, not create a view
a = [10, 20, 30, 40, 50]
a = a[1:]
println(a)                   // [20, 30, 40, 50]
println(#a)                  // 4
a = a[1:]
println(a)                   // [30, 40, 50]
println(#a)                  // 3

// === 21. In-place: x = x[:3] tail truncation ===
b = [100, 200, 300, 400, 500]
b = b[:3]
println(b)                   // [100, 200, 300]
println(#b)                  // 3

// === 22. In-place: x = x[1:4] mid-section extract ===
d = [1, 2, 3, 4, 5, 6, 7]
d = d[1:5]
println(d)                   // [2, 3, 4, 5]
println(#d)                  // 4

// === 23. In-place: x = x[2:2] empty ===
e = [10, 20, 30, 40]
e = e[2:2]
println(#e)                  // 0
println(e)                   // []

// === 24. In-place falls back to view when shared ===
f = [1, 2, 3, 4, 5]
g = f                        // share
f = f[1:]                    // refcount > 1, should create a view
println(f)                   // [2, 3, 4, 5]
println(g)                   // [1, 2, 3, 4, 5] (unchanged)

// === 25. In-place: sub-arrays fall back to view ===
h = [[1], [2], [3], [4], [5]]
h = h[1:4]
println(#h)                  // 3
println(h[0][0])             // 2

println("all slice view tests passed")
