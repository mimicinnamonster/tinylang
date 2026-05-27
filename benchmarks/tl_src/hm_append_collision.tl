// Benchmark: collision-safe append throughput
// arr["key"] += [val] — uses push optimization
map = [[]] * 100
i = 0
while i < 50000 {
    map["batch"] += [i]
    i = i + 1
}
