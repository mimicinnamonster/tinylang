// Benchmark: linear scan through 1000-element array
// Find value 500 by scanning — O(n) per lookup
flat = [0] * 1000
i = 0
for i < 1000 {
    flat[i] = i
    i = i + 1
}

// 10000 linear scans (10000 * avg 500 comparisons = 5M comps)
i = 0
for i < 10000 {
    j = 0
    for j < 1000 {
        if flat[j] == 500 { j = 1000 }
        j = j + 1
    }
    i = i + 1
}
