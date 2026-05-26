// Build 10K-element linked list - BACKWARDS
// Each node: [value, next]
// Start from the tail, prepend each new node

list = []
i = 9999
while i > -1 {
  list = [i, list]
  i = i - 1
}

// Verify by counting nodes
len = 0
i = list
while i {
  len = len + 1
  i = i[1]
}
print(len)
