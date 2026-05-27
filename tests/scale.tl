// Cost model: contiguous vs rope
// Prints break-even reads (a) for each (n, C) combination
// 0 = contiguous always wins, -1 = rope always wins

fun println(x=0) { print(x) print("\n") }

fun ceil_div(x=0, y=1) {
    result = 0
    for result * y < x { result = result + 1 }
    ret result
}

fun realloc_copies(n=0) {
    if n <= 4 { ret 0 }
    copies = 0
    cap = 4
    for cap < n {
        copies = copies + cap
        cap = cap * 2
    }
    ret copies
}

fun build_be(n=0, C=64) {
    k = ceil_div(n, C)
    rc = realloc_copies(n)
    if k * C >= rc { ret 0 }
    ret (rc - k * C) / (k - 1)
}

fun concat_be(n=0, m=0, C=64) {
    k = ceil_div(n + m, C)
    if k == 1 { ret -1 }
    if n + m <= C { ret -1 }
    ret (n + m - C) / (k - 1)
}

fun concat_eq(n=0, C=64) {
    ret concat_be(n, n, C)
}

ns = [1, 10, 100, 500, 1000, 5000, 10000, 65000]
Cs = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]

// BUILD TABLE
println("BUILD: max reads where rope wins (0=never)")
h = "" + Cs[0]
i = 1
for i < #Cs { h = h + " " + Cs[i]; i = i + 1 }
println(h)
i = 0
for i < #ns {
    n = ns[i]
    line = "" + n
    j = 0
    for j < #Cs {
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
for i < #Cs { h = h + " " + Cs[i]; i = i + 1 }
println(h)
i = 0
for i < #ns {
    n = ns[i]
    line = "" + n
    j = 0
    for j < #Cs {
        line = line + " " + concat_eq(n, Cs[j])
        j = j + 1
    }
    println(line)
    i = i + 1
}

println("")
println("LITERAL APPEND: contiguous always wins")
println("done")
