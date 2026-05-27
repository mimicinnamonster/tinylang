""" hashmap.py — Python hashmap benchmark (FNV-1a over array, matching TinyLang)
Run: python3 hashmap.py
"""

import time

FNV32_BASE = 0x811c9dc5
FNV32_PRIME = 0x01000193
MASK32 = 0xFFFFFFFF

def fnv1a(s):
    h = FNV32_BASE
    for c in s.encode('utf-8'):
        h = ((h * FNV32_PRIME) & MASK32) ^ c
    return h

def now_ms():
    return int(time.monotonic() * 1000)

# ============================================================
# Benchmark 1: Same-key read (1M)
# ============================================================
def bench_samekey_read():
    size = 1000
    mp = [0] * size
    mp[fnv1a("target") % size] = 42

    t0 = now_ms()
    s = 0
    for _ in range(1000000):
        idx = fnv1a("target") % size
        s += mp[idx]
    t1 = now_ms()
    print(f"  Py same-key read (1M):   {(t1 - t0) / 1000:.3f}s  (hash recomputed each iter)")
    del s

# ============================================================
# Benchmark 2: Different-key write (50k)
# ============================================================
def bench_diffkey_write():
    size = 100000
    mp = [0] * size

    t0 = now_ms()
    for i in range(50000):
        key = f"key_{i}"
        idx = fnv1a(key) % size
        mp[idx] = i
    t1 = now_ms()
    print(f"  Py diff-key write (50k):  {(t1 - t0) / 1000:.3f}s")

# ============================================================
# Benchmark 3: Multi-key read (1000 keys x 100 reps)
# ============================================================
def bench_multikey_read():
    size = 100000
    mp = [0] * size
    nkeys = 1000
    indices = []
    for i in range(nkeys):
        idx = fnv1a(f"k{i}") % size
        indices.append(idx)
        mp[idx] = i

    t0 = now_ms()
    s = 0
    for _ in range(100):
        for j in range(nkeys):
            s += mp[indices[j]]
    t1 = now_ms()
    print(f"  Py multi-key read (100k): {(t1 - t0) / 1000:.3f}s  (pre-computed indices)")
    del s

# ============================================================
# Benchmark 4: Collision-safe append (50k)
# ============================================================
def bench_append():
    size = 100
    buckets = [[] for _ in range(size)]

    t0 = now_ms()
    h = fnv1a("batch") % size
    for i in range(50000):
        buckets[h].append(i)
    t1 = now_ms()
    print(f"  Py append (50k):          {(t1 - t0) / 1000:.3f}s  (list append per bucket)")

print("=== Python Hashmap Benchmarks ===\n")
bench_samekey_read()
bench_diffkey_write()
bench_multikey_read()
bench_append()
print("")
