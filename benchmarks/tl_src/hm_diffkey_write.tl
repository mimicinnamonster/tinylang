// Benchmark: different-key write throughput
// Each key is a new string — no caching benefit per key
// Tests raw hash computation + modulo cost
map = [[]] * 100000
i = 0
while i < 50000 {
    map["key_" + i] = i
    i = i + 1
}
