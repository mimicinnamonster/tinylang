// Test basic slicing
arr = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

// Basic slice
s = arr[0:5]
print(s)

// Slice with defaults
s = arr[:3]
print(s)

s = arr[7:]
print(s)

// Full copy
s = arr[:]
print(s)

// Slice with step
s = arr[0:10:2]
print(s)

s = arr[1:10:2]
print(s)

// Reverse
s = arr[::-1]
print(s)

// Negative step with start/stop
s = arr[5:2:-1]
print(s)

// Empty slice
s = arr[0:0]
print(s)

// OOB clamped
s = arr[0:100]
print(s)

s = arr[100:200]
print(s)

// Negative indices
s = arr[-3:]
print(s)

s = arr[:-3]
print(s)

s = arr[-5:-2]
print(s)

// Slice assignment optimization (x = x[slice])
x = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
x = x[0:5]
print(x)

// Ensure old arr is gone and x is correct
print(#x)

// Truncation via slice
y = [10, 20, 30, 40, 50]
y = y[:3]
print(y)

// Slicing basic arrays
c = [100, 200, 300, 400, 500]
s = c[1:4]
print(s)

// Slice with step
s = c[0:5:2]
print(s)
