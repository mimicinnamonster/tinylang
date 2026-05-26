// Benchmark: COW (copy-on-write) behavior
// Tests that sharing an array is O(1) and mutation of shared data
// only triggers one copy, not repeated copies.

// --- Test 1: Write to unique array (no COW needed) ---
// Create a large array and write to every element.
// Each write should be O(1) — no copy needed.
arr1 = [0] * 50000
i = 0
while i < 50000 {
    arr1[i] = i
    i = i + 1
}
print("unique done:")
print(arr1[0])
print(arr1[49999])

// --- Test 2: COW on first write, then in-place ---
// Share the array, then mutate the copy.
// The first write should trigger one deep copy,
// subsequent writes are in-place on the private copy.
arr2 = [0] * 50000
copy = arr2       // share: refcount = 2
i = 0
while i < 50000 {
    // first write (i=0) triggers deep copy of 50K elements
    // rest of writes are in-place on the private copy
    copy[i] = i
    i = i + 1
}
// Verify original is unaffected
print("cow done:")
print(arr2[0])    // should be 0 (original unchanged)
print(copy[0])    // should be 0
print(copy[49999]) // should be 49999

// --- Test 3: Deep COW on nested arrays ---
// Sharing and then writing into a sub-array triggers
// COW at each level independently.
matrix = [[0, 0]] * 1000
row_copy = matrix[0]  // share the first row
row_copy[0] = 99      // triggers COW on row_copy (not on matrix)
matrix[1][0] = 42     // writes through matrix directly
print("deep cow done:")
print(matrix[0][0])   // should be 0 (row_copy's COW isolated it)
print(matrix[1][0])   // should be 42 (direct write)
print(row_copy[0])    // should be 99 (private copy)
