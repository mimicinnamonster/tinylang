// Collision analysis — measure collision rates
// Writes N keys into arrays of different sizes and counts overlaps

fun count_collisions(buckets=0, num_keys=0) {
    tbl = [[]] * buckets
    i = 0
    for i < num_keys {
        tbl["k" + i] = i
        i = i + 1
    }
    i = 0
    coll = 0
    for i < num_keys {
        if tbl["k" + i] != i {
            coll = coll + 1
        }
        i = i + 1
    }
    ret coll
}

fun pct(n=0, total=0) {
    if total == 0 { ret "0%" }
    ret "" + (n * 100 / total) + "%"
}

print("--- Collision Rate Analysis ---")
print("")
print("FNV-1a hash, modulo bucket count")
print("")
print("100 keys into varying bucket sizes:")
print("")
buckets_list = [10, 50, 100, 200, 500, 1000, 2000]
i = 0
for i < #buckets_list {
    b = buckets_list[i]
    c = count_collisions(b, 100)
    print("  " + b + " buckets: " + c + " collisions (" + pct(c, 100) + ")")
    i = i + 1
}

print("")
print("1000 buckets, varying key counts:")
print("")
keys_list = [100, 200, 500, 1000, 2000, 5000, 10000]
i = 0
for i < #keys_list {
    n = keys_list[i]
    c = count_collisions(1000, n)
    print("  " + n + " keys: " + c + " collisions (" + pct(c, n) + ")")
    i = i + 1
}

print("")
print("Load factor analysis (keys/bucket ratio vs collision rate):")
print("")
sizes = [[10000, 1000], [10000, 5000], [10000, 10000], [10000, 20000], [10000, 50000], [10000, 100000]]
i = 0
for i < #sizes {
    b = sizes[i][0]
    n = sizes[i][1]
    c = count_collisions(b, n)
    ratio = n / b
    print("  " + n + " keys / " + b + " buckets = " + ratio + "x load: " + c + " collisions (" + pct(c, n) + ")")
    i = i + 1
}
