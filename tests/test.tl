// test.tl — test basic features

fun println(x=0) { print(x) print("\n") }

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

// fun
fun double(n=0) {
    ret n * 2
}

result = double(10)
println(result)

// factorial (recursion)
fun fact(n=0) {
    if n == 0 {
        ret 1
    }
    ret n * fact(n - 1)
}

println(fact(5))

// multi-index
matrix = [[1, 2], [3, 4]]
println(matrix[0][1])

// string
name = "hello"
println(name)
