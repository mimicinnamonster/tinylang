// Build 500-element linked list forwards
// Each node is [value, next_node]
// We use dynamic indexing arr[indices] to write at the tail
// without creating shared references (no COW copies)

arr = [0, []]       // first node
path = [1]           // indices to reach the current tail
i = 1
while i < 500 {
  arr[path] = [i, []]  // write new node at the tail
  path = path + [1]    // extend path to point to the new tail
  i = i + 1
}

// Traverse
i = arr
while i {
  print(i[0])
  i = i[1]
}
