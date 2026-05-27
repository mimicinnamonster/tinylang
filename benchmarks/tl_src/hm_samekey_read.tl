// Benchmark: same-key read throughput
// Tests hash caching — hash computed once per unique string
map = [[]] * 1000
map["target"] = 42
i = 0
for i < 1000000 {
    _ = map["target"]
    i = i + 1
}
