// Benchmark: key length impact on hash computation
// Long keys require walking more bytes for FNV-1a

// Short key (3 bytes)
m = [0] * 1000
i = 0
while i < 500000 {
    _ = m["abc"]
    i = i + 1
}
