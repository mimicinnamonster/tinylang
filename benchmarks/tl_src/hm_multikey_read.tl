// Benchmark: multi-key read — many unique cached strings
// N unique keys, each read R times = total reads
// After first pass, all hashes are cached
nkeys = 1000
reps = 100
map = [[]] * 100000

// Populate
i = 0
while i < nkeys {
    map["k" + i] = i
    i = i + 1
}

// Read all keys multiple times
i = 0
while i < reps {
    j = 0
    while j < nkeys {
        _ = map["k" + j]
        j = j + 1
    }
    i = i + 1
}
