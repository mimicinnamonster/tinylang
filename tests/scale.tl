// Cost model: contiguous vs rope
// Prints break-even reads (a) for each (n, C) combination
// 0 = contiguous always wins, -1 = rope always wins

function println(x=0) { print(x) print("\n") }

function ceil_div(x=0, y=1) {
    result = 0
    while result * y < x { result = result + 1 }
    return result
}

function realloc_copies(n=0) {
    if n <= 4 { return 0 }
    copies = 0
    cap = 4
    while cap < n {
        copies = copies + cap
        cap = cap * 2
    }
    return copies
}

function build_be(n=0, C=64) {
    k = ceil_div(n, C)
    rc = realloc_copies(n)
    if k * C >= rc { return 0 }
    return (rc - k * C) / (k - 1)
}

function concat_be(n=0, m=0, C=64) {
    k = ceil_div(n + m, C)
    if k == 1 { return -1 }
    if n + m <= C { return -1 }
    return (n + m - C) / (k - 1)
}

function concat_eq(n=0, C=64) {
    return concat_be(n, n, C)
}

ns = [1, 10, 100, 500, 1000, 5000, 10000, 65000]
Cs = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]

// BUILD TABLE
println("BUILD: max reads where rope wins (0=never)")
h = "" + Cs[0]
i = 1
while i < #Cs { h = h + " " + Cs[i]; i = i + 1 }
println(h)
i = 0
while i < #ns {
    n = ns[i]
    line = "" + n
    j = 0
    while j < #Cs {
        line = line + " " + build_be(n, Cs[j])
        j = j + 1
    }
    println(line)
    i = i + 1
}

println("")
println("CONCAT (a=b): max reads where rope wins (-1=always)")
h = "" + Cs[0]
i = 1
while i < #Cs { h = h + " " + Cs[i]; i = i + 1 }
println(h)
i = 0
while i < #ns {
    n = ns[i]
    line = "" + n
    j = 0
    while j < #Cs {
        line = line + " " + concat_eq(n, Cs[j])
        j = j + 1
    }
    println(line)
    i = i + 1
}

println("")
println("LITERAL APPEND: contiguous always wins")
println("done")
