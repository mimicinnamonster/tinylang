// Test OC_PUSH: x = x + [elem] optimization

function println(x) { print(x) print("\n") }

// 1. Basic append
arr = []
i = 0
while i < 10 {
  arr = arr + [i]
  i = i + 1
}
i = 0
while i < 10 {
  if arr[i] != i {
    println("FAIL 1")
  }
  i = i + 1
}
println(#arr)

// 2. Value semantics preserved
x = [1, 2, 3]
y = x + [4]
if #x != 3 { println("FAIL 2a") }
if #y != 4 { println("FAIL 2b") }
if y[3] != 4 { println("FAIL 2c") }
println(#x)

// 3. Self-assign
x = [1, 2, 3]
x = x + [4]
if x[3] != 4 { println("FAIL 3") }
println(#x)

// 4. Complex inner expression (parens for nested ops)
path = []
i = 0
while i < 5 {
  path = path + [(i * 2)]
  i = i + 1
}
i = 0
while i < 5 {
  if path[i] != (i * 2) {
    println("FAIL 4")
  }
  i = i + 1
}
println(#path)

// 5. Push with arrays as elements
list = []
i = 0
while i < 3 {
  list = list + [[i]]
  i = i + 1
}
i = 0
while i < 3 {
  if list[i][0] != i {
    println("FAIL 5")
  }
  i = i + 1
}
println(#list)

// 6. Multi-element array literal (fallback to normal +)
multi = [1]
multi = multi + [2, 3, 4]
if multi[0] != 1 { println("FAIL 6a") }
if multi[1] != 2 { println("FAIL 6b") }
if multi[2] != 3 { println("FAIL 6c") }
if multi[3] != 4 { println("FAIL 6d") }
println(#multi)

// 7. Large chain (1000 elements)
big = []
i = 0
while i < 1000 {
  big = big + [i]
  i = i + 1
}
if #big != 1000 { println("FAIL 7a") }
if big[0] != 0 { println("FAIL 7b") }
if big[500] != 500 { println("FAIL 7c") }
if big[999] != 999 { println("FAIL 7d") }
println(#big)

println("done")
