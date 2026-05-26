// Test that slice assignment optimization works
// By constructing a large array and slicing it repeatedly

function build(n) {
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
print(#arr)

arr = arr[:5000]
print("after arr = arr[:5000], len = ")
print(#arr)

// Further truncation
arr = arr[:2000]
print("after arr = arr[:2000], len = ")
print(#arr)

// Tail slice
arr = arr[500:]
print("after arr = arr[500:], len = ")
print(#arr)

// Step slice (needs copy since step != 1)
arr = arr[0:#arr:2]
print("after arr = arr[0:#arr:2], len = ")
print(#arr)

// Shared array — should fall back to copy path
shared = build(1000)
copy = shared
shared = shared[:500]   // refcount > 1, must copy
print("shared len = ")
print(#shared)
print("copy len = ")
print(#copy)            // should still be 1000 (unaffected)

print("done")
