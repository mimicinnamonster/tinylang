// hashmap_compare.tl — TinyLang hashmap benchmark for cross-language comparison
// Mirrors the C/JS/Python benchmarks exactly

fun println(x=0) { print(x) print("\n") }

// ============================================================
// Benchmark 1: Same-key read (1M)
// TinyLang caches the hash on the Arr after first access.
// This is an advantage over C/JS/Python which recompute each iteration.
// For fairness, we measure what TinyLang actually does.
// ============================================================
map = [0] * 1000
map["target"] = 42
i = 0
for i < 1000000 {
    _ = map["target"]
    i = i + 1
}

// ============================================================
// Benchmark 2: Different-key write (50k unique keys)
// ============================================================
map2 = [0] * 100000
i = 0
for i < 50000 {
    map2["key_" + i] = i
    i = i + 1
}

// ============================================================
// Benchmark 3: Multi-key read (1000 keys x 100 reps)
// ============================================================
map3 = [0] * 100000
nkeys = 1000
i = 0
for i < nkeys {
    map3["k" + i] = i
    i = i + 1
}
// Read all keys 100 times
r = 0
for r < 100 {
    j = 0
    for j < nkeys {
        _ = map3["k" + j]
        j = j + 1
    }
    r = r + 1
}

// ============================================================
// Benchmark 4: Collision-safe append (50k)
// ============================================================
map4 = [[]] * 100
i = 0
for i < 50000 {
    map4["batch"] += [i]
    i = i + 1
}
