// Build 10K-element linked list - FORWARDS
// Each node: [value, next]
// Use dynamic indexing to append at the tail

arr = [0, []]
path = [1]
i = 1
while i < 10000 {
  arr[path] = [i, []]
  path = path + [1]
  i = i + 1
}

// Verify by counting nodes
len = 0
i = arr
while i {
  len = len + 1
  i = i[1]
}
print(len)
