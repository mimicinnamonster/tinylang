// hashmap benchmark — string-keyed hashmap performance tests
// Run: time ./tiny benchmarks/tl_src/hashmap.tl

fun println(x=0) { print(x) print("\n") }
fun print_csv(name="", n=0, elapsed=0) {
    print(name)
    print(",")
    print(n)
    print(",")
    print(elapsed)
    print("\n")
}

// ============================================================
// Benchmark 1: Read throughput — same key, repeated access
// Demonstrates hash caching: hash computed once, cached for 9999
// ============================================================
println("=== Benchmark 1: Same-key read throughput ===")
println("# iterations,time_ms")
i = 0
t0 = clock()
map = [[]] * 1000
map["target"] = 42
for i < 100000 {
    _ = map["target"]
    i = i + 1
}
t1 = clock()
print_csv("same-key", 100000, t1 - t0)

// ============================================================
// Benchmark 2: Write throughput — different keys each time
// No caching benefit since each key is new
// ============================================================
println("=== Benchmark 2: Different-key write throughput ===")
println("# writes,time_ms")
map2 = [[]] * 1000
i = 0
t0 = clock()
for i < 50000 {
    map2["key_" + i] = i
    i = i + 1
}
t1 = clock()
print_csv("diff-key-write", 50000, t1 - t0)

// ============================================================
// Benchmark 3: Collision-safe append throughput
// arr["key"] += [val] — uses push optimization
// ============================================================
println("=== Benchmark 3: Collision-safe append throughput ===")
println("# appends,time_ms")
map3 = [[]] * 100
i = 0
t0 = clock()
for i < 50000 {
    map3["batch"] += [i]
    i = i + 1
}
t1 = clock()
print_csv("append-same-key", 50000, t1 - t0)

// ============================================================
// Benchmark 4: Collision rate vs array size
// Write N keys to arrays of different sizes, count collisions
// ============================================================
println("=== Benchmark 4: Collision rate ===")
println("buckets,keys,collisions,unique-buckets-used")

fun measure_collisions(buckets=0, num_keys=0) {
    tbl = [[]] * buckets
    used = [0] * buckets
    collisions = 0
    i = 0
    for i < num_keys {
        tbl["k" + i] = i
        i = i + 1
    }
    // Count unique buckets used
    // We can't iterate buckets easily, so count by checking each bucket's content
    // Use a second pass: for each key, hash it and mark
    i = 0
    for i < num_keys {
        tbl["k" + i] = i
        i = i + 1
    }
    // Instead, count collisions by seeing how many keys share buckets
    // Since we can't iterate, use the fact that tbl["k"+i] = i
    // A collision happened if tbl["k"+i] != i (overwritten by another key)
    i = 0
    coll = 0
    for i < num_keys {
        if tbl["k" + i] != i {
            coll = coll + 1
        }
        i = i + 1
    }
    // Count used buckets
    used_buckets = 0
    i = 0
    for i < buckets {
        if #tbl[i] > 0 {
            used_buckets = used_buckets + 1
        }
        i = i + 1
    }
    print_csv("rate", buckets, 0)
    // overwrite last line with real data
    // print manually
    print(buckets) print(",")
    print(num_keys) print(",")
    print(coll) print(",")
    print(used_buckets) print("\n")
}

measure_collisions(10, 100)
measure_collisions(100, 100)
measure_collisions(1000, 100)
measure_collisions(100, 1000)
measure_collisions(1000, 1000)

// ============================================================
// Benchmark 5: Varying key lengths — impact on hash computation
// ============================================================
println("=== Benchmark 5: Key length impact ===")
println("key-length,time_ms")

fun bench_key_length(len=0, iters=0) {
    // Build a key of given length
    k = ""
    j = 0
    for j < len {
        k = k + "x"
        j = j + 1
    }
    m = [0] * 1000
    i = 0
    t0 = clock()
    for i < iters {
        _ = m[k]
        i = i + 1
    }
    t1 = clock()
    print_csv("len", len, 0)
    print(len) print(",")
    print(t1 - t0) print("\n")
}

bench_key_length(1, 100000)
bench_key_length(10, 100000)
bench_key_length(100, 100000)
bench_key_length(1000, 10000)

// ============================================================
// Benchmark 6: Hashmap vs linear scan in an array
// Compare: hash-based lookup vs scanning an array for a value
// ============================================================
println("=== Benchmark 6: Hashmap vs linear scan ===")
println("method,size,time_ms")

// Build a hashmap and an equivalent flat array
hm_size = 100
hmap = [0] * hm_size
flat = [0] * 1000

// Fill both with the same data
i = 0
for i < 1000 {
    flat[i] = i
    hmap["v" + i] = i
    i = i + 1
}

// Hashmap read
t0 = clock()
i = 0
for i < 100000 {
    _ = hmap["v" + 500]
    i = i + 1
}
t1 = clock()
print_csv("hash-read", 100000, 0)
print("hash-read,")
print(100000) print(",")
print(t1 - t0) print("\n")

// Linear scan to find value 500 in flat array
t0 = clock()
i = 0
found = []
for i < 10000 {
    j = 0
    for j < 1000 {
        if flat[j] = 500 { j = 1000 }  // found, break
        j = j + 1
    }
    i = i + 1
}
t1 = clock()
print_csv("linear-scan", 10000, 0)
print("linear-scan,")
print(10000) print(",")
print(t1 - t0) print("\n")

// ============================================================
// Benchmark 7: Multi-key caching — many distinct keys in a loop
// Each unique string caches its hash after first use
// ============================================================
println("=== Benchmark 7: Multi-key caching ===")
println("unique-keys,time_ms")

fun bench_multi_key(nkeys=0, iters=0) {
    m = [[]] * 1000
    // Pre-populate
    i = 0
    for i < nkeys {
        m["key_" + i] = i
        i = i + 1
    }
    // Read all keys in a loop
    i = 0
    t0 = clock()
    for i < iters {
        j = 0
        for j < nkeys {
            _ = m["key_" + j]
            j = j + 1
        }
        i = i + 1
    }
    t1 = clock()
    print(nkeys) print(",")
    print(t1 - t0) print("\n")
}

bench_multi_key(10, 1000)
bench_multi_key(100, 100)
bench_multi_key(1000, 10)

println("")
println("=== done ===")
