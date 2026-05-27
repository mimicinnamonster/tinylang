// Test default fun parameter values

fun println(x=0) { print(x) print("\n") }

// Basic number default
fun add(a=0, b=10) {
    ret a + b
}

println(add(5))       // 5 + 10 = 15
println(add(5, 3))    // 5 + 3 = 8

// String default
fun greet(name="world") {
    ret "hello " + name
}

println(greet())          // hello world
println(greet("mimi"))    // hello mimi

// Array default
fun first(arr=[10, 20, 30]) {
    ret arr[0]
}

println(first())           // 10
println(first([99, 88]))   // 99

// Nil default
fun maybe(x=nil) {
    if x == nil {
        ret "was nil"
    }
    ret "was " + x
}

println(maybe())          // was nil
println(maybe("set"))     // was set

// Multiple defaults
fun configure(a=1, b=2, c=3) {
    ret a + b + c
}

println(configure())        // 6
println(configure(10))      // 10 + 2 + 3 = 15
println(configure(10, 20))  // 10 + 20 + 3 = 33
println(configure(10, 20, 30))  // 10 + 20 + 30 = 60

// Calling with subset of arguments
fun foo(x=0, y=100) {
    ret x + y
}

println(foo())       // 0 + 100 = 100
println(foo(5))      // 5 + 100 = 105
println(foo(5, 7))   // 5 + 7 = 12

// Complex array literal as default
fun bar(nested=[1, [2, 3], 4]) {
    ret nested[0]
}

println(bar())    // 1
println(bar([99]))  // 99

// String default
fun say(msg="hello!\n") {
    print(msg)
}

say()           // hello!
say("bye!\n")   // bye!
