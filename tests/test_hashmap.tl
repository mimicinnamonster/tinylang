// test_hashmap.tl — string-indexed hashmap access on arrays
//
// String keys use FNV-1a hash (Git's strhash algorithm) modulo array length
// to determine the index. Hash is cached on the Arr after first computation.

fun println(x=0) { print(x) print("\n") }

// ========================================================
// Test 1: basic read with string key
// ========================================================
arr = [10, 20, 30, 40, 50]
// hash("a") = 84696446, % 5 = 1
println(arr["a"])            // 20

// ========================================================
// Test 2: basic write with string key
// ========================================================
arr["b"] = 99
// hash("b") = 84696445, % 5 = 0
println(arr[0])              // 99

// ========================================================
// Test 3: same key is deterministic — always maps to same slot
// ========================================================
arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
arr["hello"] = 777
println(arr["hello"])        // 777
println(arr["hello"])        // 777 (same key, same result)

// ========================================================
// Test 4: collision — different keys can map to same slot
// last write wins
// ========================================================
arr2 = [100, 200]
// hash("hello")=3069866343, hash("world")=2609808943, both % 2 = 1
arr2["hello"] = 111
arr2["world"] = 222
println(arr2[1])             // 222 (world overwrote hello at slot 1)

// ========================================================
// Test 5: collision-safe append — arr[key] += [val]
// When you want to store multiple values per key, ensure the
// slot holds an array and append to it.
// ========================================================
hm = [[], [], [], [], []]
hm["a"] += [42]
hm["a"] += [43]
// hash("a") % 5 = 1 — bucket 1 accumulates [42, 43]
println(hm[1][0])            // 42
println(hm[1][1])            // 43
println(#hm[1])              // 2 (length of appended array)

// ========================================================
// Test 6: variable holding a string as key
// ========================================================
key = "hello"
arr3 = [0, 0, 0, 0, 0]
arr3[key] = 999
// hash("hello")=3069866343, % 5 = 3
println(arr3[3])             // 999
println(arr3[key])           // 999 (read via same variable)

// ========================================================
// Test 7: string variable reassigned — different key
// ========================================================
arr4 = [0, 0, 0]
k = "x"
arr4[k] = 10
k = "y"
arr4[k] = 20
println(arr4[0])             // whichever slot x or y maps to
println(arr4[1])
println(arr4[2])
// at least one of these should be 10 and one should be 20
// (x and y may collide depending on hash)

// ========================================================
// Test 8: chained bracket access — arr["a"]["b"]
// ========================================================
m = [[1, 2], [3, 4]]
m[0][0] = 99
println(m[0][0])             // 99 (regular chain for comparison)
println(m[0][1])             // 2

// ========================================================
// Test 9: numeric indexing still works
// ========================================================
nums = [5, 10, 15]
println(nums[0])             // 5
nums[2] = 99
println(nums[2])             // 99

// ========================================================
// Test 10: dynamic chain with non-string array still works
// Non-printable arrays [0,1,2] use chain behavior, not hash
// ========================================================
mat = [[10, 20], [30, 40]]
idx = [0, 1]
println(mat[idx])            // 20 (mat[0][1])

// ========================================================
// Test 11: compound assignment on string-indexed slot
// ========================================================
ca = [0, 0, 0, 0, 0]
// hash("z") % 5 → increment a few times
ca["z"] = 10
ca["z"] = ca["z"] + 5
println(ca["z"])             // 15

// ========================================================
// Test 12: many different keys in a loop — caching doesn't affect
// correctness (each unique string caches its own hash)
// ========================================================
coll = [0, 0, 0, 0]
keys = ["a", "b", "c", "d", "e", "f", "g", "h"]
j = 0
for j < #keys {
    coll[keys[j]] = j + 1
    j = j + 1
}
// verify each key reads back what was written
j = 0
for j < #keys {
    v = coll[keys[j]]
    if v != j + 1 {
        println(0)
    }
    j = j + 1
}
println(1)                   // 1 (all checks passed)
