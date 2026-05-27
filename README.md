# TinyLang

A tiny, statically-typed programming language implemented in 1,163 lines of C.
Single-pass compiler to bytecode with a stack-based VM, refcount+COW, and tail
call optimization. No AST, no GC, no closures, no pointers.

## Features

- **Two types:** `number` (double) and `array` (heterogeneous, deep equality)
- **Value semantics:** No references, no aliasing, no GC — refcount+COW sharing
- **Tail call optimization:** Recursive functions don't blow the C stack
- **Operator precedence:** `||` < `&&` < `==`/`!=` < `<`/`>`/`<=`/`>=` < `+`/`-` < `*`/`/`/%` 
- **Short-circuit** `&&` and `||` with conditional evaluation
- **Statement separation:** Newlines (Go-style inference) or explicit `;` separate statements
- **Functions:** Pure (no globals), define-before-use, no closures, recursion OK
- **Control flow:** `if`/`elif`/`else`, `while`
- **Arrays:** Nested, heterogeneous, `[val] * n` repetition, `arr + arr` concatenation
- **Multi-index:** `arr[i, j, k]` desugars to `arr[i][j][k]`
- **Array slicing:** `arr[start:stop]` and `arr[start:stop:step]`, with Python-style defaults
- **Strings:** Syntactic sugar for byte arrays, escape sequences supported
- **Number literals:** decimal and `0x` hex
- **REPL:** Expression values auto-printed, brace balancing for multi-line blocks
- **Operators:** `+ - * / %`, `= != < > <= >=`, `!`, `#` (array length prefix), `&&` `||`, `<<` `>>`
- **Built-ins:** `print()`, `input()`, `thispath()`

## Quick start

```sh
cc -Wall -Wextra -O2 -lm -o tinylang tinylang.c
./tinylang tests/test.tl
```

### REPL

```sh
./tinylang          # bare REPL, no line editing
rlwrap ./tinylang   # with history and arrow keys (brew install rlwrap)
./repl.sh           # restarts on error (Ctrl+C to exit)
```

The REPL reads until braces balance before executing, so multi-line functions
and blocks work naturally.

### Tests

```sh
./run_tests.sh      # runs all happy-path and error tests
```

- **Happy-path tests** — `tests/test_*.tl` and `tests/bench_*.tl`
- **Error tests** — `tests/e_*.tl` (expected runtime errors)
- Benchmarks in `tests/bench_*.tl` (backwards traversal, COW, push optimization, TCO)

All tests must pass before committing.

## Example

```
// linked list sum
function sum(list) {
    if list == nil {
        return 0
    }
    return list[0] + sum(list[1])
}

print(sum([1, [2, [3, nil]]]))     // 6

// factorial (tail-recursive)
function fact(n, acc) {
    if n == 0 {
        return acc
    }
    return fact(n - 1, n * acc)    // TCO: no stack growth
}

print(fact(5, 1))                   // 120
print(fact(1000, 1))                // inf (no stack overflow)

// array slicing
arr = [0, 1, 2, 3, 4, 5]
print(arr[1:3])                     // [1, 2]
print(arr[:4:2])                    // [0, 2]
print(arr[::2])                     // [0, 2, 4]

// hex literals
print(0xFF)                         // 255

// manual heap pattern
nodes = [[0, -1]] * 10
nodes[0][0] = 42
print(nodes[0][0])                  // 42
```

## Implementation

- 1,163 lines of C, single file
- Pre-lexed token array → single-pass compiler → flat bytecode (`Instr[]`)
- Stack-based VM: computed goto dispatch, `Value istk[4096]` stack
- Slot-indexed variable access: O(1) instead of O(n) strcmp
- Deep copy on assignment, refcount+COW arrays with push optimization
  (`x = x + [elem]` compiles to O(1) `OC_PUSH`, no array copy)
- Tail call optimization: parameter rebinding + ip reset (no C stack growth)
- Comprehensive test suite (25+ happy-path tests, 20 error tests)

## Benchmarks

[`benchmarks/`](benchmarks/) contains a performance comparison of TinyLang against
C (Apple Clang `-O2`), Node.js (V8 JIT), and Python (CPython 3.9) on four
benchmarks from the [Computer Language Benchmarks Game](https://benchmarksgame-team.pages.debian.net/benchmarksgame/):

| Benchmark | Description |
|-----------|-------------|
| **spectral-norm** | Matrix eigenvalue via power iteration (float FMA) |
| **n-body** | Solar system simulation (5 bodies, gravity, sqrt) |
| **mandelbrot** | Fractal set generation (per-pixel iteration) |
| **fasta** | Random DNA sequence generation (PRNG + table + I/O) |

### Key Results

```
                   C     Node.js     Python    TinyLang
spectral-norm    2.75s    1.93s     212.70s     0.07s*    (* N=100 vs 5500)
n-body           0.59s    0.52s      93.10s     0.19s*    (* N=5000 vs 5M)
mandelbrot       0.18s    0.05s       0.47s     0.26s
fasta            0.16s    0.07s       0.31s     0.14s
```

- **Node.js is 0.3–1× of C** (V8's JIT often beats naive C)
- **Python is 77–158× slower than C** at full sizes
- **TinyLang is 1.5–2.7× faster than Python** at matching sizes, and
  4–31× slower than Node.js

See [`benchmarks/REPORT.md`](benchmarks/REPORT.md) for the full analysis.

### Running Benchmarks

```sh
cd benchmarks
./run.sh
```

## Grammar

```
program       := top-level statements

statement     := assignment | if_stmt | while_stmt | func_def | ret_stmt | include_stmt | expr_stmt
include_stmt  := "include" string

assignment    := lvalue "=" expr
lvalue        := identifier ("[" slice_or_index "]")*
if_stmt       := "if" expr block ("elif" expr block)* ("else" block)?
while_stmt    := "while" expr block
func_def      := "function" identifier "(" params ")" block
ret_stmt      := "return" expr

block         := "{" stmt_list "}"
expr          := logical_or
logical_or    := logical_and ("||" logical_and)*
logical_and   := comparison ("&&" comparison)*
comparison    := shift (("=" | "!=" | "<" | ">" | "<=" | ">=") shift)?
shift         := addition (("<<" | ">>") addition)*
addition      := multiplication (("+" | "-") multiplication)*
multiplication := primary (("*" | "/" | "%") primary)*

primary       := number | identifier | "nil" | string | array_literal
               | call | index | slice | "(" expr ")" | "!" primary | "-" primary

slice         := primary "[" expr? ":" expr? (":" expr?)? "]"
```

## Design

- [`DESIGN.md`](DESIGN.md) — Language design, types, semantics
- [`IMPLEMENTATION.md`](IMPLEMENTATION.md) — C implementation details, bytecode VM
