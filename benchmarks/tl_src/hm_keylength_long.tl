// Benchmark: long key length (100 bytes)
k = ""
i = 0
while i < 100 {
    k = k + "x"
    i = i + 1
}
m = [0] * 1000
i = 0
while i < 500000 {
    _ = m[k]
    i = i + 1
}
