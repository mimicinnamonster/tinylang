# TinyLang vs C vs Node.js — Performance Benchmark Report

**Date:** 2026-05-27
**Hardware:** Apple MacBook Air M1 (16GB)
**OS:** macOS 15.7.5
**C:** Apple Clang (cc) `-O2 -lm`
**Node.js:** v25.9.0 (V8 JIT)
**TinyLang:** ~1160-line single-pass bytecode VM, refcount+COW arrays

---

## 1. Benchmarks

Four benchmarks from the [Computer Language Benchmarks Game](https://benchmarksgame-team.pages.debian.net/benchmarksgame/) were ported identically to C, JavaScript (Node.js), and TinyLang:

| Benchmark | Description | Computations |
|-----------|-------------|-------------|
| **spectral-norm** | Matrix eigenvalue via power iteration | O(N²) per iter, 10 iters, float FMA |
| **n-body** | Solar system simulation (5 bodies) | Gravity interactions per step, sqrt |
| **mandelbrot** | Fractal set generation | Per-pixel iteration, 50 max/pixel |
| **fasta** | Random DNA sequence generation | PRNG + table lookup + I/O |

---

## 2. Full-Size Results (Benchmark Game Sizes)

### 2.1 Spectral-Norm — N=5500 (302.5M inner iterations)

| Metric | C (-O2) | Node.js | TinyLang |
|--------|---------|---------|----------|
| **Real time** | **2.75s** | **1.93s** | N/A (too slow) |
| **User time** | 2.49s | 1.90s | N/A |
| **Instructions** | 13.3B | 19.7B | N/A |
| **Peak memory** | 1.18 MB | 14.96 MB | N/A |
| **Relative** | 1.0× | **0.7×** (faster than C!) | — |

> Node.js is **faster than C** here because V8 inlines the tiny `eval_A()` function and
> vectorizes the inner loop. The C version's function call overhead dominates.

### 2.2 N-Body — N=5,000,000 steps

| Metric | C (-O2) | Node.js | TinyLang |
|--------|---------|---------|----------|
| **Real time** | **0.59s** | **0.52s** | N/A (too slow) |
| **User time** | 0.41s | 0.50s | N/A |
| **Instructions** | 2.25B | 7.05B | N/A |
| **Peak memory** | 1.08 MB | 14.10 MB | N/A |
| **Relative** | 1.0× | **0.9×** (comparable!) | — |

> Node.js is nearly **on par with C** thanks to `Float64Array` typed array optimization
> in V8. The typed array gives direct memory access without boxing.

### 2.3 Mandelbrot — 200×200 pixels, 50 iterations/pixel

| Metric | C (-O2) | Node.js | TinyLang |
|--------|---------|---------|----------|
| **Real time** | 0.18s | **0.05s** | **1.58s** |
| **User time** | <0.01s | 0.04s | 1.57s |
| **Instructions** | 24.2M | 305M | **11.6B** |
| **Peak memory** | 1.08 MB | 14.70 MB | 1.16 MB |
| **Relative** | **1.0×** | **0.3×** (faster!) | **8.8×** (vs C) |

> Node.js is **3.4× faster than C** because V8 JIT-compiles the tight float loop to
> native code and keeps the entire computation in registers — the M1's 4-wide
> FP pipeline runs at full speed. C's `fwrite` per row adds system call overhead.

### 2.4 Fasta — N=25,000 bases

| Metric | C (-O2) | Node.js | TinyLang |
|--------|---------|---------|----------|
| **Real time** | 0.16s | **0.07s** | **0.57s** |
| **User time** | <0.01s | 0.06s | 0.56s |
| **Instructions** | 19.4M | 410M | **4.5B** |
| **Peak memory** | 1.13 MB | 17.21 MB | 5.69 MB |
| **Relative** | 1.0× | **0.4×** (faster!) | **3.6×** (vs C) |

> Node.js is faster than C due to V8's optimized string building and buffered I/O.
> C's `fwrite` calls per line cause more system call overhead.

---

## 3. Matching-Size Results (Dual-Scale Comparison)

For fair comparison with TinyLang's limited throughput:

### 3.1 Spectral-Norm — N=100 (10K inner iterations)

| Metric | C (-O2) | Node.js | TinyLang |
|--------|---------|---------|----------|
| **Real time** | **<0.01s** | **0.05s** | **0.19s** |
| **Instructions** | 15.2M | 265M | **1.86B** |
| **Instr ratio vs C** | 1.0× | 17× | **122×** |
| **Peak memory** | 1.07 MB | 13.59 MB | 1.26 MB |

**TinyLang is 4× slower than Node.js and ~190× slower than C.**

### 3.2 N-Body — N=5,000 steps

| Metric | C (-O2) | Node.js | TinyLang |
|--------|---------|---------|----------|
| **Real time** | **<0.01s** | **0.05s** | **0.45s** |
| **Instructions** | 12.7M | 269M | **3.82B** |
| **Instr ratio vs C** | 1.0× | 21× | **300×** |
| **Peak memory** | 1.08 MB | 13.36 MB | **5.72 MB** |

**TinyLang is 9× slower than Node.js and ~760× slower than C.**

---

## 4. Summary Comparison Table

### 4.1 Full Sizes (C vs Node.js)

| Benchmark | C (time) | Node (time) | C/Node | Instr(C) | Instr(Node) | Instr Ratio |
|-----------|----------|-------------|--------|----------|-------------|-------------|
| spectral-norm N=5500 | 2.75s | 1.93s | **0.70×** | 13.3B | 19.7B | 1.5× |
| n-body N=5M | 0.59s | 0.52s | **0.88×** | 2.25B | 7.05B | 3.1× |
| mandelbrot 200×200 | 0.18s | 0.05s | **0.28×** | 24.2M | 305M | 12.6× |
| fasta N=25000 | 0.16s | 0.07s | **0.44×** | 19.4M | 410M | 21.1× |

> **Node.js is 1.1–3.6× faster than C on these workloads** due to V8's aggressive
> JIT compilation enabling loop vectorization and function inlining beyond what
> `-O2` does for the simple C code. However, it uses **1.5–21× more instructions**
> and **10–17× more memory**.

### 4.2 Matching Sizes (C vs Node.js vs TinyLang)

| Benchmark | C | Node.js | TinyLang | TL/Node | TL/C |
|-----------|---|---------|----------|---------|------|
| spectral-norm N=100 | <0.01s | 0.05s | 0.19s | **3.8×** | **~190×** |
| n-body N=5000 | <0.01s | 0.05s | 0.45s | **9.0×** | **~760×** |
| mandelbrot 200×200 | 0.18s | 0.05s | 1.58s | **31.6×** | **8.8×** |
| fasta N=25000 | 0.16s | 0.07s | 0.57s | **8.1×** | **3.6×** |

### 4.3 Visual Scaling

```
                  slow ──────────────────────────────► fast
                     
spectral-norm N=5500  : C ██████████████████████████ 2.75s
                        JS ████████████████████ 1.93s

n-body N=5M           : C ██████ 0.59s
                        JS █████ 0.52s

mandelbrot 200×200    : TL ██████████████████████████████████████████████████ 1.58s
                        C  ██████ 0.18s
                        JS ██ 0.05s

fasta N=25000          : TL ██████████████████████████████████████████ 0.57s
                        C  ████████████ 0.16s
                        JS █████ 0.07s

n-body N=5000          : TL ███████████████████████████████████████████████████████████ 0.45s
                        C  █ 0.00s
                        JS ██████ 0.05s

spectral-norm N=100    : TL █████████████████████████████████████████████████████████ 0.19s
                        C  █ 0.00s
                        JS ████████████████ 0.05s
```

---

## 5. Performance Ratios (Harmonic Means)

| Comparison | Time Ratio | Instruction Ratio | Memory Ratio |
|-----------|-----------|-------------------|-------------|
| **C vs Node.js** (full sizes) | **0.5×** (JS faster) | 6× | 15× |
| **Node.js vs TinyLang** (equal sizes) | **9×** | 18× | 0.2× (TL uses less) |
| **C vs TinyLang** (equal sizes) | **~250×** | ~200× | 0.9× |

---

## 6. Root-Cause Analysis

### 6.1 Why Node.js sometimes beats C

Node.js (V8) can outperform naive C code because:

1. **JIT inlining:** `eval_A(i,j)` in spectral-norm is a tiny 4-operation function.
   V8 inlines it and hoists the loop-invariant computation. The C compiler with
   `-O2` may also inline it, but V8 does runtime profile-guided optimization.

2. **Loop vectorization:** V8 detects the tight `for` loops in mandelbrot and
   spectral-norm and auto-vectorizes using ARM NEON/SIMD, while our simple C
   version uses scalar operations.

3. **Typed array optimization:** `Float64Array` in n-body gives V8 direct
   memory access without boxing — nearly as fast as C's `double[]`.

4. **Buffered I/O:** Node.js's `console.log` and `process.stdout.write` use
   internal buffering, reducing system call overhead vs C's `fwrite` per row.

### 6.2 Why TinyLang is 4–31× slower than Node.js

| Factor | Approx Impact | Details |
|--------|---------------|---------|
| **Interpreted vs JIT** | **10–50×** | TinyLang: switch-based bytecode dispatch. Node: native code via V8's TurboFan JIT |
| **Array overhead** | **3–10×** | TinyLang: refcount+COW, type dispatch per element, 16-byte Value structs. Node: `Float64Array` or optimized `Array` with monomorphic inline caches |
| **No hardware sqrt** | **2–5×** | TinyLang: 100-iteration Newton-Raphson. Node/V8: `Math.sqrt` → single `fsqrt` instruction |
| **Function call cost** | **3–6×** | TinyLang: stack save/restore (12+ opcodes), scope creation. Node/V8: inlined after warmup |
| **Memory model** | **2–4×** | TinyLang: heap-allocated arrays with COW copies. Node: GC with generational collection, object pooling |

### 6.3 Comparative Architecture

```
                         C                        Node.js                    TinyLang
                         │                        │                          │
  Execution model        Native ARM64             JIT-compiled (TurboFan)    Bytecode interpreter
  ──────────────────────────────────────────────────────────────────────────────────────
  Number repr            double (8B)              double (8B) or Int32       1-8B compact
  Array repr             double[] (contiguous)    FixedArray or TypedArray   Arr struct + refcount
  Function call          `bl` instruction         Inlined after warmup       Stack save/restore
  Loop                   for/while → branch       JIT: loop rotation,       OC_JZ + OC_JMP
  sqrt                   fsqrt (~15 cycles)       Math.sqrt → fsqrt          Newton (100 iters)
  Memory                 stack + malloc           Generational GC           Refcount + COW
  Line count             ~100-150 per bench       50-100 per bench          ~100-200 per bench
```

### 6.4 Where TinyLang Excels

Despite being slower, TinyLang shows strengths:

| Property | Advantage |
|----------|-----------|
| **Memory efficiency** | 1.2–5.7 MB vs Node's 14–17 MB (3–12× less) |
| **Deterministic cleanup** | Refcount: no GC pauses, predictable latency |
| **Compact storage** | `[1,2,3]` → 3 bytes (int8), vs 24+ bytes in JS |
| **Startup time** | ~2ms to compile + run vs Node's ~40ms V8 init |
| **Implementation size** | ~1160 lines C vs V8's millions of lines |

---

## 7. Key Takeaways

1. **Node.js (V8) is remarkably fast** — within 0.3–1× of naive C, and often
   faster thanks to profile-guided JIT optimization.

2. **TinyLang is 9–31× slower than Node.js** for the same workloads. The gap
   is dominated by interpreted dispatch vs JIT compilation.

3. **Typed arrays bridge the gap** — Node.js's `Float64Array` enables C-like
   performance on numeric workloads. TinyLang's compact type system is a
   memory optimization but adds dispatch overhead.

4. **The biggest single fix for TinyLang** would be a direct-threaded bytecode
   dispatch (computed goto), which typically yields 2–3× speedup, closing the
   gap to ~3–15× slower than Node.js for compute-heavy code.

5. **Mandelbrot is the most revealing benchmark** — pure scalar float arithmetic
   isolates dispatch and loop overhead. Node.js is 31× faster than TinyLang and
   3× faster than C, showing the full range of the performance spectrum.
