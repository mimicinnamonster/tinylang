// Additional slice edge cases

function println(x=0) { print(x) print("\n") }

// Expression indices
a = 1
b = 5
arr = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
s = arr[a:b]
println(s)

// Negative step expressions
s = arr[8:3:-2]
println(s)

// Step as variable
st = 3
s = arr[0:10:st]
println(s)

// Step from nil (from literal [start:stop])
// Should default to 1
s = arr[2:7:]
println(s)

// Slice on empty array
empty = []
s = empty[0:5]
println(s)

s = empty[::-1]
println(s)

// Slice on nil variable
nilv = []
s = nilv[:]
println(s)

// Multi-index still works
matrix = [[10, 20], [30, 40]]
println(matrix[1][0])

// slice on 1D sub-array
vals = [1, 2, 3, 4, 5]
println(vals[1:4])

// Optimization test: x = x[slice] with exclusive ownership
// Should produce correct result and not alias
x = [1, 2, 3, 4, 5, 6]
y = x              // share
x = x[2:5]         // x gets slice, y unaffected
println(x)
println(y)           // y should still have full array

// In-place truncation via optimization
x = [100, 200, 300, 400, 500]
x = x[:3]          // truncate
println(x)
println(#x)          // should be 3

// Slice with ARR_VAL (heterogeneous)
h = [1, "hello", 3, [] , 5]
s = h[1:4]
println(s)

// Reverse on heterogeneous
s = h[::-1]
println(s)

// Slice assignment optimization with negative step (should copy, not in-place)
x = [1, 2, 3, 4, 5]
x = x[3:0:-1]
println(x)
