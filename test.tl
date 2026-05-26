// test.tl — test basic features

x = 5
y = x + 3
print(y)

// array
arr = [1, 2, 3]
print(arr)

// array index
print(arr[0])

// lvalue assignment
arr[0] = 99
print(arr)

// function
function double(n) {
    return n * 2
}

result = double(10)
print(result)

// factorial (recursion)
function fact(n) {
    if n = 0 {
        return 1
    }
    return n * fact(n - 1)
}

print(fact(5))

// multi-index
matrix = [[1, 2], [3, 4]]
print(matrix[0][1])

// string
name = "hello"
print(name)
