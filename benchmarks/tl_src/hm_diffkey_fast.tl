// Benchmark: different-key write — pre-computed keys to isolate hash cost
// Keys are pre-built, so the loop only does hash + modulo + write
map = [[]] * 100000

// Pre-compute 1000 keys
keys = []
i = 0
while i < 1000 {
    keys = keys + ["k" + i]
    i = i + 1
}

// Write using pre-computed keys (no string concat in the loop)
i = 0
while i < 100000 {
    map[keys[i % 1000]] = i
    i = i + 1
}
