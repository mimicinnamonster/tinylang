// Cost model simulator: contiguous buffer vs rope
//
// We considered adding ropes (linked fixed-size chunks) to TinyLang
// to make arr = arr + [1,2,3] O(1) instead of O(n). This file computes
// the break-even for each operation pattern to see if ropes are worth it.
//
// Three scenarios:
//   A: Build array from scratch (repeated pushes with OC_PUSH)
//   B: Append literal [1,2,3] to existing array (multi-push fix)
//   C: Concat two existing arrays (the one case multi-push doesn't cover)
//
// Cost model (in element-copy units):
//   Contiguous:  concat = copy all existing elements, access = O(1)
//   Rope (C=64): concat = allocate one chunk (cost C), access = walk chunks
//
// Results (run this file with ./tinylang cost_model.tl):
//   A: contiguous always wins (OC_PUSH realloc copies < rope chunk allocs)
//   B: contiguous always wins (multi-push makes concat free)
//   C: rope wins only when reads < C (~64). TinyLang never does array-array
//      concat in hot paths (benchmarks use OC_PUSH loops).
//


function realloc_copies(n) {
    if n <= 4 { return 0 }
    copies = 0
    cap = 4
    while cap < n {
        copies = copies + cap
        cap = cap * 2
    }
    return copies
}

function ceil_div(x, y) {
    // Integer ceiling division: ceil(x / y)
    result = 0
    while result * y < x {
        result = result + 1
    }
    return result
}

function show(label, c_total, r_total) {
    print(label)
    print(c_total)
    print(r_total)
}

// Scenario B: Append literal [1,2,3] using multi-push fix
function scenario_append_literal(n, m, a, C) {
    // contiguous: m writes (push), no element copies
    c_total = m + a

    // rope: allocate chunk (size C) + m writes, then a * chunks walks
    total_len = n + m
    chunks = ceil_div(total_len, C)
    r_total = C + m + a * chunks

    show("B: " + n + " " + m + " " + a, c_total, r_total)
}

// Scenario C: Concat two existing arrays
function scenario_concat_arrays(n, m, a, C) {
    // contiguous: copy all n + m elements
    c_total = n + m + a

    // rope: link chunks (cost C), then walk
    total_len = n + m
    chunks = ceil_div(total_len, C)
    r_total = C + a * chunks

    show("C: " + n + " " + m + " " + a, c_total, r_total)
}

// Scenario A: Build from scratch via pushes
function scenario_build(n, a, C) {
    // contiguous: n writes + realloc copies
    c_total = n + realloc_copies(n) + a

    // rope: n writes + chunk allocs + a * chunks walks
    chunks = ceil_div(n, C)
    r_total = n + chunks * C + a * chunks

    show("A: " + n + " " + a, c_total, r_total)
}

// Sweeps: print cost at each a to see crossover
function sweep_append(n, m, C) {
    print("sweep B n=" + n)
    a = 0
    while a <= 200 {
        total_len = n + m
        chunks = ceil_div(total_len, C)
        c_total = m + a
        r_total = C + m + a * chunks
        print(a)
        print(c_total)
        print(r_total)
        a = a + 1
    }
    print("---")
}

function sweep_concat(n, m, C) {
    print("sweep C n=" + n)
    a = 0
    while a <= 200 {
        total_len = n + m
        chunks = ceil_div(total_len, C)
        c_total = n + m + a
        r_total = C + a * chunks
        print(a)
        print(c_total)
        print(r_total)
        a = a + 1
    }
    print("---")
}

function sweep_build(n, C) {
    print("sweep A n=" + n)
    a = 0
    while a <= 200 {
        chunks = ceil_div(n, C)
        c_total = n + realloc_copies(n) + a
        r_total = n + chunks * C + a * chunks
        print(a)
        print(c_total)
        print(r_total)
        a = a + 1
    }
    print("---")
}

// Run
C = 64

print("===== B: append literal =====")
scenario_append_literal(100, 3, 10, C)
scenario_append_literal(100, 3, 100, C)
scenario_append_literal(100, 3, 1000, C)
scenario_append_literal(1000, 3, 10, C)
scenario_append_literal(1000, 3, 100, C)
scenario_append_literal(1000, 3, 1000, C)

print("===== C: array concat =====")
scenario_concat_arrays(100, 100, 10, C)
scenario_concat_arrays(100, 100, 100, C)
scenario_concat_arrays(100, 100, 1000, C)
scenario_concat_arrays(1000, 1000, 10, C)
scenario_concat_arrays(1000, 1000, 100, C)
scenario_concat_arrays(1000, 1000, 1000, C)

print("===== A: build =====")
scenario_build(100, 10, C)
scenario_build(100, 100, C)
scenario_build(100, 1000, C)
scenario_build(1000, 10, C)
scenario_build(1000, 100, C)
scenario_build(1000, 1000, C)

// Now sweeps with full data
// Each sweep prints: a, contiguous_total, rope_total for a=0..200
// We can identify crossover: where contiguous_total becomes less than rope_total

print("===== sweep B: append n=100 =====")
sweep_append(100, 3, C)

print("===== sweep B: append n=1000 =====")
sweep_append(1000, 3, C)

print("===== sweep B: append n=10000 =====")
sweep_append(10000, 3, C)

print("===== sweep C: concat n=100 =====")
sweep_concat(100, 100, C)

print("===== sweep C: concat n=1000 =====")
sweep_concat(1000, 1000, C)

print("===== sweep C: concat n=10000 =====")
sweep_concat(10000, 10000, C)

print("===== sweep A: build n=100 =====")
sweep_build(100, C)

print("===== sweep A: build n=1000 =====")
sweep_build(1000, C)

print("===== sweep A: build n=10000 =====")
sweep_build(10000, C)

print("done")
