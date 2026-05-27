// hashmap.js — Node.js hashmap benchmark (FNV-1a over array, matching TinyLang)
// Run: node hashmap.js

const FNV32_BASE = 0x811c9dc5 >>> 0;
const FNV32_PRIME = 0x01000193 >>> 0;

function fnv1a(str) {
    let hash = FNV32_BASE;
    for (let i = 0; i < str.length; i++)
        hash = ((hash * FNV32_PRIME) >>> 0) ^ str.charCodeAt(i);
    return hash >>> 0;
}

function now_ms() {
    const hrt = process.hrtime.bigint();
    return Number(hrt / 1000000n);
}

/* ============================================================
 * Benchmark 1: Same-key read (1M)
 * ============================================================ */
function bench_samekey_read() {
    const size = 1000;
    const map = new Array(size).fill(0);
    map[fnv1a("target") % size] = 42;

    const t0 = now_ms();
    let sum = 0;
    for (let i = 0; i < 1000000; i++) {
        const idx = fnv1a("target") % size;
        sum += map[idx];
    }
    const t1 = now_ms();
    console.log(`  JS same-key read (1M):   ${((t1 - t0) / 1000).toFixed(3)}s  (hash recomputed each iter)`);
}

/* ============================================================
 * Benchmark 2: Different-key write (50k)
 * ============================================================ */
function bench_diffkey_write() {
    const size = 100000;
    const map = new Array(size).fill(0);

    const t0 = now_ms();
    for (let i = 0; i < 50000; i++) {
        const key = "key_" + i;
        const idx = fnv1a(key) % size;
        map[idx] = i;
    }
    const t1 = now_ms();
    console.log(`  JS diff-key write (50k):  ${((t1 - t0) / 1000).toFixed(3)}s`);
}

/* ============================================================
 * Benchmark 3: Multi-key read (1000 keys x 100 reps)
 * ============================================================ */
function bench_multikey_read() {
    const size = 100000;
    const map = new Array(size).fill(0);
    const nkeys = 1000;
    const indices = new Array(nkeys);
    for (let i = 0; i < nkeys; i++) {
        indices[i] = fnv1a("k" + i) % size;
        map[indices[i]] = i;
    }

    const t0 = now_ms();
    let sum = 0;
    for (let r = 0; r < 100; r++) {
        for (let j = 0; j < nkeys; j++) {
            sum += map[indices[j]];
        }
    }
    const t1 = now_ms();
    console.log(`  JS multi-key read (100k): ${((t1 - t0) / 1000).toFixed(3)}s  (pre-computed indices)`);
}

/* ============================================================
 * Benchmark 4: Collision-safe append (50k)
 * Using array per bucket to match TinyLang's arr[key] += [val]
 * ============================================================ */
function bench_append() {
    const size = 100;
    const buckets = new Array(size).fill(null).map(() => []);

    const t0 = now_ms();
    const h = fnv1a("batch") % size;
    for (let i = 0; i < 50000; i++) {
        buckets[h].push(i);
    }
    const t1 = now_ms();
    console.log(`  JS append (50k):          ${((t1 - t0) / 1000).toFixed(3)}s  (array push per bucket)`);
}

console.log("=== Node.js Hashmap Benchmarks ===\n");
bench_samekey_read();
bench_diffkey_write();
bench_multikey_read();
bench_append();
console.log("");
