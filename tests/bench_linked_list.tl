// Benchmark: linked list traversal with refcount+COW
// Builds a linked list of N elements using TCO, then traverses it.
// Expected: O(N) time, regardless of list depth.

fun println(x=0) { print(x) print("\n") }

fun range(n=0, acc=[]) {
    if n == 0 {
        ret acc
    }
    ret range(n - 1, [n, acc])
}

fun sum_list(lst=[]) {
    total = 0
    for lst != [] {
        total = total + lst[0]
        lst = lst[1]
    }
    ret total
}

fun max_list(lst=[]) {
    m = 0
    for lst != [] {
        if lst[0] > m {
            m = lst[0]
        }
        lst = lst[1]
    }
    ret m
}

fun len_list(lst=[]) {
    n = 0
    for lst != [] {
        n = n + 1
        lst = lst[1]
    }
    ret n
}

// Build a list of N elements
list = range(10000, [])
println("sum:")
println(sum_list(list))
println("max:")
println(max_list(list))
println("len:")
println(len_list(list))
