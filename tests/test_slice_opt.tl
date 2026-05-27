// Test that slice assignment optimization works
// By constructing a large array and slicing it repeatedly

function println(x=0) { print(x) print("\n") }

function build(n=0) {
    r = []
    i = 0
    while i < n {
        r = r + [i]
        i = i + 1
    }
    return r
}

arr = build(10000)

// Slice assignment to same variable — should use in-place optimization
print("before slice, len = ")
println(#arr)

arr = arr[:5000]
print("after arr = arr[:5000], len = ")
println(#arr)

// Further truncation
arr = arr[:2000]
print("after arr = arr[:2000], len = ")
println(#arr)

// Tail slice
arr = arr[500:]
print("after arr = arr[500:], len = ")
println(#arr)

// Step slice (needs copy since step != 1)
arr = arr[0:#arr:2]
print("after arr = arr[0:#arr:2], len = ")
println(#arr)

// Shared array — should fall back to copy path
shared = build(1000)
copy = shared
shared = shared[:500]   // refcount > 1, must copy
print("shared len = ")
println(#shared)
print("copy len = ")
println(#copy)            // should still be 1000 (unaffected)

println("done")
