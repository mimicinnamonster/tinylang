// Performance test: slice views should be O(1), not O(n)
// Slices are either zero-copy views (step==1, shared) or
// in-place mutations (step==1, exclusively owned, numbers-only).
// Only strided slices (step!=1) copy.

fun println(x=0) { print(x) print("\n") }

// Build a large array
fun build(n=0) {
    r = []
    i = 0
    for i < n {
        r = r + [i]
        i = i + 1
    }
    ret r
}

// === 1. Slice of a large array — should be instant ===
big = build(50000)
// Large contiguous slice — view, no copy
slice1 = big[10000:40000]
println(#slice1)                // 30000

// === 2. x = x[slice] chain — no copies ===
chain = build(50000)
chain = chain[5000:45000]       // view 1
chain = chain[5000:40000]       // view 2 (of view)
chain = chain[5000:35000]       // view 3 (of view of view)
println(#chain)                 // 30000

// Verify values are correct through the view chain
println(chain[0])               // 15000 (5000+5000+5000 from offsets)

// === 3. Shared array slice — view shares backing, COW on write ===
shared = build(20000)
also = shared
view_shared = shared[5000:15000] // view, no copy
println(#view_shared)           // 10000
// Mutate original — COW kicks in, view unaffected
shared[5000] = 999999
// View should still see the original value
println(view_shared[0])         // 5000 (not 999999)

// === 4. Multiple views from same array ===
data = build(10000)
va = data[0:5000]               // view: [0..4999]
vb = data[5000:10000]           // view: [5000..9999]
// Writing to data shouldn't affect views (COW during the write)
data[0] = 888888
data[9999] = 777777
println(va[0])                  // 0 (not 888888, COW on data mutation)
println(vb[#vb - 1])            // 9999 (not 777777, COW on data mutation)

// === 5. Repeated re-slice of same variable ===
// Should produce correct results without accumulating garbage
reslice = build(10000)
reslice = reslice[1000:9000]    // 8000 elements
reslice = reslice[1000:8000]    // 7000
reslice = reslice[1000:7000]    // 6000
reslice = reslice[1000:6000]    // 5000
reslice = reslice[1000:5000]    // 4000
reslice = reslice[1000:4000]    // 3000
reslice = reslice[1000:3000]    // 2000
reslice = reslice[1000:2000]    // 1000
println(#reslice)               // 1000
println(reslice[0])             // 8000 (each step added 1000 offset)

// === 6. Push optimization still works with slices ===
p_arr = build(5000)
pv = p_arr[1000:2000]           // view: [1000..1999]
// Push onto view variable should trigger COW flatten + push
pv = pv + [9999]
println(#pv)                    // 1001
println(pv[#pv - 1])            // 9999
// Original unchanged
println(p_arr[1000])            // 1000
println(#p_arr)                 // 5000

// === 7. Step != 1 still copies (expected, no performance regression) ===
stepped = build(50000)
s = stepped[0:50000:3]
println(#s)                     // 16667 (50000/3 rounded up)

// === 8. Slice of a slice of a slice should not leak ===
// Chain multiple views and reassign — old views should be GC'd
leak_test = build(5000)
leak_test = leak_test[500:4500]   // view 1
leak_test = leak_test[500:4000]   // view 2 (releases view 1)
leak_test = leak_test[500:3500]   // view 3 (releases view 2)
leak_test = leak_test[500:3000]   // view 4 (releases view 3)
println(#leak_test)             // 2500

// === 9. In-place optimization: x = x[1:] in a loop ===
// When exclusively owned with numbers, each iteration memmove+trims.
// This should be fast regardless of array size.
loop_arr = build(10000)
i = 0
for i < 9000 {
    loop_arr = loop_arr[1:]
    i = i + 1
}
println(#loop_arr)              // 1000
println(loop_arr[0])            // 9000

// === 10. In-place: repeated tail truncation ===
tail = build(10000)
tail = tail[:5000]
println(#tail)                  // 5000
tail = tail[:2000]
println(#tail)                  // 2000
tail = tail[:1000]
println(#tail)                  // 1000

// === 11. In-place: mid-section extract on exclusive array ===
mid = build(10000)
mid = mid[2000:8000]
println(#mid)                   // 6000
println(mid[0])                 // 2000

// === 12. Empty slice (in-place path) ===
emp = build(1000)
emp = emp[500:500]
println(#emp)                   // 0

println("all perf slice view tests passed")
