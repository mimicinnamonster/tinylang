// Benchmark: linked list traversal with refcount+COW
// Builds a linked list of N elements using TCO, then traverses it.
// Expected: O(N) time, regardless of list depth.

function range(n, acc) {
    if n = 0 {
        return acc
    }
    return range(n - 1, [n, acc])
}

function sum_list(lst) {
    total = 0
    while lst != [] {
        total = total + lst[0]
        lst = lst[1]
    }
    return total
}

function max_list(lst) {
    m = 0
    while lst != [] {
        if lst[0] > m {
            m = lst[0]
        }
        lst = lst[1]
    }
    return m
}

function len_list(lst) {
    n = 0
    while lst != [] {
        n = n + 1
        lst = lst[1]
    }
    return n
}

// Build a list of N elements
list = range(10000, [])
print("sum:")
print(sum_list(list))
print("max:")
print(max_list(list))
print("len:")
print(len_list(list))
