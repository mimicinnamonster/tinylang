// Benchmark: hashmap read throughput (pure lookup, no string concat overhead)
hmap = [0] * 1000
// pre-populate
i = 0
while i < 1000 {
    hmap["v" + i] = i
    i = i + 1
}

// 1M reads of the same literal key — hash cached after first
i = 0
while i < 1000000 {
    _ = hmap["v500"]
    i = i + 1
}
