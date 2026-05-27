// Build 10K-element linked list - BACKWARDS
// Each node: [value, next]
// Start from the tail, prepend each new node

list = []
i = 9999
for i > -1 {
  list = [i, list]
  i = i - 1
}

// Verify by counting nodes
len = 0
node = list
for node {
  len = len + 1
  node = node[1]
}
print(len); print("\n")
