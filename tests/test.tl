// test.tl — test basic features

function println(x=0) { print(x) print("\n") }

x = 5
y = x + 3
println(y)

// array
arr = [1, 2, 3]
println(arr)

// array index
println(arr[0])

// lvalue assignment
arr[0] = 99
println(arr)

// function
function double(n=0) {
    return n * 2
}

result = double(10)
println(result)

// factorial (recursion)
function fact(n=0) {
    if n == 0 {
        return 1
    }
    return n * fact(n - 1)
}

println(fact(5))

// multi-index
matrix = [[1, 2], [3, 4]]
println(matrix[0][1])

// string
name = "hello"
println(name)
