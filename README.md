# TinyLang

> ✨ Tiny Is Beautiful ✨

TinyLang (pet name Tiny) is a tiny, **statically-typed** programming language
built to explore how simple a language can be while still being practical.
Every design choice prioritises **simple implementation** and 
**easy optimisation** — no runtime type dispatch, no garbage collector,
no closures, no pointers, no AST, no intermediate representations.
Just a single-pass compiler emitting flat bytecode for a stack-based VM,
all in a single C file.

The central thesis: **static typing and simple semantics are not constraints —
they are enablers.**  Every optimisation in the VM flows directly from a
language decision that kept things simple — no runtime type dispatch, no hash
table lookups, no GC, no intermediate representations.  The result is a
bytecode interpreter that punches well above its weight class.

There are exactly three types: `number`, `array`, and `string`. Under the hood
strings are implemented as byte arrays (with a marker flag) — all array operations
work on them. No closures, no pointers, no first-class functions, no GC.
Everything is static.  Everything is simple.  Everything is easy to optimise.

The compiler tracks the type of every variable and every function return value
at compile time. First assignment determines a variable's type permanently;
type changes are detected at compile time, not at runtime. Function return
types are inferred from the body and checked for consistency across all
return statements. Array-type slots are initialized to `[]` at scope-creation
time, eliminating the need for runtime type guards on array operations.

```tinylang
// Everything here compiles to flat bytecode in one pass.
// No AST, no runtime type checks, no GC pauses.

fun sum(list=[], acc=0) {
    if list == nil { ret acc }
    ret sum(list[1], acc + list[0])  // TCO, unlimited recursion
}

// Lisp-like linked list
print(sum([1, [2, [3, [4, nil]]]]))  // 10

nums = []; i = 0
for i < 100000 {
    nums = nums + [i] // array push with O(1)
    i = i + 1
}
print(#nums)                           // 100000

// Same with compound assignment
arr = []
arr += [10]
arr += [20, 30]
print(arr)                             // [10, 20, 30]

// Array slicing with Python-style semantics
arr = [0, 1, 2, 3, 4, 5]
print(arr[1:4])                        // [1, 2, 3]
print(arr[::-1])                       // [5, 4, 3, 2, 1, 0]

// Value semantics: no aliasing, no references — COW makes it free
orig = [[1, 2], [3, 4]]
copy = orig
copy[0][1] = 99
print(orig[0][1])                      // 2 (unchanged)
print(copy[0][1])                      // 99

// Array destructuring: unpack any array into variables
x, y = [10, 20]
print(x)                               // 10
print(y)                               // 20

// Safe in-place swap
x, y = y, x
print(x)                               // 20
```

### Where TinyLang Excels

A full language in a single C file. No required dependencies. Compiles in <1s.

| Property | TinyLang's | Python's | Node.js's | C's |
|----------|----------|--------|---------|---|
| **Memory efficiency** | 1.2–5.7 MB | 25–40 MB | 14–17 MB | 1–2 MB |
| **Startup time** | ✓ ~2ms compile + run | ~30ms startup | ~40ms startup + JIT warmup | Compile only |
| **Implementation size** | ✓ ~3K line C file | ~700K lines (CPython) | ~1.2M lines (V8+Node) | ~12.8M lines (LLVM) |
| **Deterministic cleanup** | ✓ Refcount | ✗ GC pauses | ✗ GC pauses | ✗ Manual |
| **No dependencies** | ✓ Single .c | ✗ Python runtime | ✗ Node runtime | LLVM |
| **Pure C99** | ✓ Compiles `-std=c99 -pedantic` | ✗ | ✗ | ✓ |
| **Predictable performance** | ✓ No JIT warmup, no GC pauses, no runtime dispatch | ✗ GC pauses, runtime type checks | ✗ Warmup-dependent, GC pauses | ✓ Always fast |
| **Array building** | O(1) amortized push | O(n) amortized | O(1) push, dynamic arrays | ✗ Manual |

| **Float math throughput** | 1× (baseline) | **~5× slower** | **~1.5× faster** | **~10× faster** |
| **Zero-copy slicing** | ✓ O(1) view, share backing store | ✗ O(n) full copy | ✗ O(n) full copy | ✓ O(1) pointer arithmetic |
| **Tail call optimization** | ✓ Guaranteed infinite recursion | ✗ No TCO | ✗ No TCO | ✓~ Compiler-dependent |

## Table of Contents

- [Quick start](#quick-start)
  - [REPL](#repl)
  - [Tests](#tests)
- [Language Features](#language-features)
  - [Types](#types)
  - [Strings](#strings-syntactic-sugar-for-byte-arrays)
  - [Literals & Identifiers](#literals--identifiers)
  - [Statement Separation](#statement-separation)
  - [Truthiness & Negation](#truthiness--negation)
  - [Operators](#operators)
  - [Operator Precedence](#operator-precedence)
  - [Variable Scoping](#variable-scoping)
  - [Assignment](#assignment)
  - [Value Semantics (Copy-on-Write)](#value-semantics-copy-on-write)
  - [Arrays](#arrays)
  - [nil](#nil)
  - [Multi-Indexing](#multi-indexing)
  - [Index Chains](#index-chains)
  - [Functions](#functions)
  - [Control Flow](#control-flow)
  - [Include System](#include-system)
  - [Error Handling](#error-handling)
  - [Built-in Functions](#built-in-functions)
- [Language Idioms](#language-idioms)
  - [Preferred Style](#preferred-style)
  - [Common Operations](#common-operations)
  - [Data Structures](#data-structures)
  - [OOP Style](#oop-style)
- [Optimizations & VM Internals](#optimizations--vm-internals)
  - [1. Goto-to-Switch Dispatch](#1-goto-to-switch-dispatch)
  - [2. Slot-Indexed Variable Access](#2-slot-indexed-variable-access)
  - [3. Compile-Time Type Tracking](#3-compile-time-type-tracking)
  - [4. Push Optimization](#4-push-optimization)
  - [5. Copy-on-Write (COW) Arrays](#5-copy-on-write-cow-arrays)
  - [6. Tail Call Optimization (TCO)](#6-tail-call-optimization-tco)
  - [7. Single-Pass Compilation](#7-single-pass-compilation-no-ast)
  - [8. Parameter Binding by Slot Index](#8-parameter-binding-by-slot-index)
  - [9. Pre-Sized Scopes with Slot Initialization](#9-pre-sized-scopes-with-slot-initialization)
  - [10. Zero-Copy Slice Views](#10-zero-copy-slice-views)
  - [11. Function Inlining (Compile-Time)](#11-function-inlining-compile-time)
  - [12. Move-Semantics COW Avoidance](#12-move-semantics-cow-avoidance)
  - [Cumulative Optimization Impact](#cumulative-optimization-impact)
- [Performance Comparisons](#performance-comparisons-tinylang-vs-c-vs-nodejs-vs-python)
  - [Full-Size Results](#full-size-results)
  - [Matching-Size Results](#matching-size-results-fair-comparison)
  - [Ratio Summary](#ratio-summary)
  - [Where TinyLang Excels](#where-tinylang-excels)
- [Running Benchmarks](#running-benchmarks)
- [Implementation](#implementation)
  - [Portability Notes](#portability-notes)
- [Grammar](#grammar)
- [Design](#design)

## Quick start

```sh
# Without readline (no line editing):
cc -std=c99 -Wall -pedantic -O3 -lm -o tiny tinylang.c
./tiny tests/test.tl

# With optional readline support (line editing, history, arrow keys):
cc -DREADLINE -Wall -Wextra -O2 -lm -o tiny tinylang.c -lreadline

# Bytecode dump (text format):
./tiny --bytecode program.tl

# Profile mode (count calls, TCO, COW deep copies):
./tiny --profile program.tl
```

### REPL

```sh
./tiny          # REPL with built-in input handling
```

When compiled with `-DREADLINE`, the REPL provides full readline editing:
Emacs keybindings, command history (up/down arrows), incremental search
(Ctrl+R), and tab completion. Without readline, a basic `fgets`-based input
is used. Either way, no external tools like `rlwrap` are needed — the REPL
is self-contained. Errors print a traceback and return to the prompt without
destroying variables or function definitions.

The REPL reads until braces balance before executing, so multi-line functions
and blocks work naturally.

### Tests

```sh
./tests/run_tests.sh      # runs all happy-path and error tests
```

- **Happy-path tests** — `tests/test_*.tl` and `tests/bench_*.tl`
- **Error tests** — `tests/e_*.tl` (expected runtime errors)
- **Benchmarks** in `tests/bench_*.tl` (backwards traversal, COW, push optimization, TCO)

All tests must pass before committing.

---

## Language Features

### Types

TinyLang has exactly **three types: `number`, `array`, and `string`**.

- **`number`** — always a C `double`. Supports integer and floating-point values,
  hex literals, and arithmetic.
- **`array`** — a heterogeneous `Value[]` sequence that can hold numbers and
  sub-arrays freely. The empty array `[]` is the only falsey value and serves
  as `nil`/`null`/`false`.
- **`string`** — a byte array (implemented as an `array` under the hood with a
  marker flag). All array operations work on strings: indexing, slicing,
  concatenation, length, and push optimization. Strings are distinguishable
  from generic arrays at compile time for string-specific formatting in `print()`.

```tinylang
// Numbers

```tinylang
// Numbers
x = 42           // integer
y = 3.14159      // float
z = 0xFF         // hex: 255
w = .5           // 0.5
v = 5.           // 5.0

// Arrays
a = [1, 2, 3]                // homogeneous
mixed = [1, "hello", []]     // heterogeneous
matrix = [[1, 2], [3, 4]]    // nested
empty = []                   // empty array = nil/false
```

### Strings

Strings are a distinct type implemented as byte arrays under the hood.
`"abc"` creates a string containing bytes `[97, 98, 99]`. All array operations
work on strings: indexing, slicing, concatenation, length, push, and repetition.
The `print` function displays strings as text rather than `[value, ...]`.

`print` does **not** add a trailing newline — include `\n` in your strings
when you want one.

```tinylang
s = "hello"
print(s)            // hello (prints as text)
print(#s)           // 5 (length)
print(s[0])         // 104 ('h')
print(s[4])         // 111 ('o')

// Escape sequences
tab = "a\tb"        // \t = 9
newline = "line1\nline2"  // \n = 10
quote = "\""        // \" = 34
backslash = "a\\b"  // \\ = 92
hex = "\x41"        // \x41 = 65 = 'A'
null = "a\0b"       // \0 = 0
```

Supported escape sequences:

| Escape | Byte Value |
|--------|-----------|
| `\n` | 10 (newline) |
| `\t` | 9 (tab) |
| `\r` | 13 (carriage return) |
| `\0` | 0 (null) |
| `\\` | 92 (backslash) |
| `\"` | 34 (double quote) |
| `\xHH` | hex byte value |

#### String Operations

Since strings are byte arrays underneath, all array operations work:
indexing, slicing, length, concatenation, and push optimization.

```tinylang
greet = "hello"
name = "world"

// Concatenation
msg = greet + ", " + name + "!"
print(msg)                   // hello, world!

// Indexing
print(greet[0])              // h (byte value 104 as character)
print(greet[1])              // e

// Slicing
print(greet[1:4])            // ell

// Length
print(#"hello")              // 5

// Push — appends bytes to the string
s = ""
s = s + [104, 101, 108, 108, 111]  // push each byte: "hello"
print(s)                             // hello

// String + number concatenation
print("pi: " + 3.14)         // pi: 3.14
print(99 + " bottles")       // 99 bottles

// Value semantics — strings are just arrays
orig = "hello"
copy = orig
copy[0] = 72                 // 72 = 'H'
print(orig)                  // hello (unchanged, COW)
print(copy)                  // Hello
```

### Literals & Identifiers

**Number literals** support decimal and hex:

```tinylang
42         // decimal integer
3.14       // decimal float
.5         // leading dot
5.         // trailing dot
0xFF       // hex uppercase
0xdeadbeef // hex lowercase
0X0        // hex with 0X prefix
```

**Identifiers** follow standard convention:

```
foo  foo_bar  _private  abc123
```

Regex: `[a-zA-Z_][a-zA-Z0-9_]*`

**Reserved keywords:**

```
fun   ret   if   elif   else   for   nil   include
```

Built-in function names (`print`, `input`) are **not** keywords.

### Statement Separation

Statements are separated by newlines (Go-style inference) or explicit semicolons:

```tinylang
// Newline-separated (idiomatic)
x = 5
y = 10
z = x + y

// Semicolon-separated
a = 1; b = 2; c = a + b

// Mixed
d = 5; e = 10
f = d + e
```

### Truthiness & Negation

- **`[]` (empty array), `0` (zero), `""` (empty string), and `nil` are falsey.**
  `[[]]`, `[0]` are truthy.
- The canonical truth value is the number `1`.
- `!` is prefix negation: turns truthy to `[]`, falsey to `1`.

| `x` | `!x` |
|-----|------|
| `[]` | `1` |
| `0` | `1` |
| `""` | `1` |
| `[1,2]` | `[]` |
| `1` | `[]` |
| `5` | `[]` |

```tinylang
print(![])      // 1
print(!0)       // 1   (0 is now falsey)
print(!1)       // []
print(!5)       // []
print(!!5)      // 1
print(!!0)      // []  (!!0: !0=1, !1=[])
print(!![])     // []
```

### Operators

#### Arithmetic

| Operator | Operation | Notes |
|----------|-----------|-------|
| `+` | Addition / array concat / push | Numbers add; arrays concatenate; `x = x + [...]` compiles to O(1) push; string + number converts number to string then concats |
| `-` | Subtraction | Requires two numbers (compile-time type error otherwise) |
| `*` | Multiplication / array repeat | Number × number, or array × number |
| `/` | Division | Halts on division by zero |
| `%` | Modulo | Uses `fmod`, halts on zero |
| `&` | Bitwise AND | Integer bitwise AND on doubles |
| `|` | Bitwise OR | Integer bitwise OR on doubles |
| `^` | Bitwise XOR | Integer bitwise XOR on doubles |
| `<<` | Left shift | Integer bit shift on doubles |
| `>>` | Right shift | Integer bit shift on doubles |

```tinylang
print(10 + 20)         // 30
print(10 - 4)          // 6
print(6 * 7)           // 42
print(10 / 3)          // 3.33333...
print(10 % 3)          // 1
print(1 << 8)          // 256
print(256 >> 4)        // 16
```

**Array concatenation** with `+`:

```tinylang
c = [1, 2] + [3, 4]   // [1, 2, 3, 4]
```

When the target is the same variable (`x = x + [...]`), the compiler emits
the push optimization — appends elements in-place instead of creating
a new concatenated array. This works for bracket literals, function calls,
variables, slices, and chained expressions, as long as the target is a known
array.

**String + number concatenation** converts the number to its decimal string
representation and concatenates as two strings. Only works when the array side
contains printable ASCII bytes — generic arrays like `[1, 2, 3]` produce a type
error:

```tinylang
print("hello" + 42)       // hello42
print(99 + " bottles")    // 99 bottles
print("pi: " + 3.14)      // pi: 3.14
print("neg: " + (-5))     // neg: -5

// Type error — non-printable array:
x = [1, 2, 3]
// print(x + 42)  // '+' type mismatch
```

**Array repetition** with `*`:

```tinylang
zeros = [0] * 10       // [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
pairs = [1, 2] * 3     // [1, 2, 1, 2, 1, 2]
```

#### Comparison

All comparisons return `1` (truthy) or `[]` (falsey).

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

Array equality is **deep** — element-by-element recursive comparison.

```tinylang
print(5 == 5)          // 1
print(5 == 6)          // []
print(5 < 6)           // 1
print(5 > 3)           // 1
print(3 <= 5)          // 1
print(5 <= 5)          // 1
print([1, 2] == [1, 2]) // 1 (deep equal)
print([1, 2] == [1, 3]) // []
print(5 == "hello")    // [] (mixed type = not equal)
```

> **Note:** `=` is always **assignment** (`x = 5`). `==` is the equality
> **comparison** operator (`if x == 5 { }`). These are distinct tokens — the
> lexer produces `T_ASSIGN` for `=` and `T_EQ` for `==`.

#### Logical (short-circuit)

| Operator | Behavior |
|----------|----------|
| `&&` | Evaluates left; if falsey, returns it without evaluating right |
| `||` | Evaluates left; if truthy, returns it without evaluating right |

```tinylang
print(1 && 1)         // 1
print([] && 1)        // [] (short-circuit)
print(1 || [])        // 1 (short-circuit)
print([] || 1)        // 1
print([] || [])       // []
```

#### Unary Prefix

| Operator | Meaning |
|----------|---------|
| `!` | Logical negation |
| `-` | Numeric negation |
| `#` | Array length |
| `~` | Bitwise NOT |

```tinylang
print(-5)             // -5
print(#[])            // 0
print(#nil)           // 0 (nil = [])
print(#"abc")         // 3
print(#[[1,2],[3,4]]) // 2
print(~0)             // -1
print(~1)             // -2
```

### Operator Precedence

Binary operators chain with standard precedence (highest to lowest,
all left-associative):

| Level | Operators | Category |
|-------|-----------|----------|
| 9 | `*` `/` `%` | Multiplicative |
| 8 | `+` `-` | Additive |
| 7 | `<<` `>>` | Shift |
| 6 | `<` `>` `<=` `>=` | Relational |
| 5 | `==` `!=` | Equality |
| 4 | `&` | Bitwise AND |
| 3 | `^` | Bitwise XOR |
| 2 | `\|` | Bitwise OR |
| 1 | `&&` | Logical AND |
| 0 | `\|\|` | Logical OR |

Unary operators (`!` `-` `#`) and indexing `[]` bind tighter than any binary
operator. Parentheses override.

```tinylang
print(5 + 3 * 2)        // 11   (5 + 6)
print((5 + 3) * 2)      // 16   (8 * 2)
print(10 - 4 <= 3 * 2)  // 1    (6 <= 6)
print(5 + 3 > 6 == 4 + 2 > 5) // 1  ((8>6) == (6>5))
print(1 + 2 * 3 > 4 + 5) // []  (7 > 9)
```

### Variable Scoping

Variables are always local to the **current function or top-level scope**.

- Functions **never** access variables from outside their own scope — no closures,
  no globals.
- Each function call gets a fresh scope with pre-allocated slots.
- First assignment determines a variable's type permanently. Changing type later
  halts with a type error.
- Variable reads/writes inside function bodies are O(1) slot-indexed operations.
- Variable and function names live in different namespaces.

```tinylang
x = 10               // top-level variable
fun foo() {
    x = 20           // creates NEW local variable 'x' in fun scope
    print(x)         // 20
}
foo()
print(x)             // 10 (top-level x unchanged)

x = "hello"          // ERROR: x was already number, type mismatch
```

### Assignment

```tinylang
x = 5                   // simple variable assignment
arr[0] = 99             // array element mutation (in-place via COW)
m[i][j] = 42            // nested mutation via lvalue chain
m[i, j] = 42            // multi-indexing: m[i][j] = 42
dyn[idx_arr] = 99       // dynamic chain: idx_arr = [i, j, k]
```

Assignments are **statements**, not expressions — they cannot appear inside
other expressions.

#### Compound Assignment

```tinylang
x += 5                  // x = x + 5
x -= 3                  // x = x - 3
x *= 2                  // x = x * 2
x /= 4                  // x = x / 4
a[0] += 10              // indexed array element
b[0][1] += 5            // nested indexed array element
```

Compound assignment operators (`+=`, `-=`, `*=`, `/=`) are syntactic sugar.
They desugar to the equivalent simple assignment at compile time, producing
exactly the same bytecode — no new opcodes, no VM changes.

Works with simple variables, indexed arrays, nested indices, and expression
right-hand sides. The push optimization fires for `arr += [elem]` just like
`arr = arr + [elem]`:

```tinylang
x = 2
x += 3 * 4              // x = x + (3 * 4) → 14

arr = []
arr += [99]             // push optimization: O(1) append
arr += [1, 2]           // multi-element: array concat (not push)
```

#### Array Destructuring

Multiple variables can be unpacked from any array expression in a single
assignment. Extra RHS elements are silently dropped; missing ones get `[]`
(nil). The RHS can be an array literal, function call, slice, variable, or
a comma-separated list of expressions (implicitly wrapped in an array).

```tinylang
x, y = [1, 2, 3]        // x=1, y=2 (3 dropped)
a, b, c = [5]           // a=5, b=[], c=[]

// In-place swap:
p, q = 100, 200
p, q = q, p             // p=200, q=100

// Any array expression:
r, s = mkarr()          // function return
t, u = mkarr()[1:]      // sliced return
v, w = src              // existing array variable

// String (byte array):
h, e_ = "he"            // h=104, e_=101

// Comma-separated RHS is implicit array:
m, n = 1, 2             // m=1, n=2
a, b = foo(), bar()     // [foo(), bar()]
```

This pairs naturally with comma-separated returns — a function can
`ret 1, 2, 3` (sugar for `ret [1, 2, 3]`) and the caller can
`a, b, c = foo()` to unpack them.

Destructured variables are assigned left-to-right, and the RHS is fully
evaluated before any assignment takes effect — this is what makes
swap-by-destructure safe.

### Value Semantics (Copy-on-Write)

TinyLang uses **reference counting with copy-on-write** to implement value
semantics. When you copy an array, both variables share the same underlying
data. A deep copy only happens when someone tries to **mutate** shared data.
This preserves value semantics (mutating a copy never affects the original)
while keeping read-only access O(1).

Contiguous **slice views** extend the same principle: a slice like `x[1:3]`
creates a lightweight view into the parent's backing store with zero copying.
The view behaves like an ordinary array for reads — only mutation triggers
a copy (flattening the view to an owned array).

```tinylang
orig = [1, 2, 3]
copy = orig
copy[0] = 99
print(orig[0])           // 1 (unchanged)
print(copy[0])           // 99

// Deep COW: nested arrays are copied on mutation
orig2 = [[1, 2], [3, 4]]
copy2 = orig2
copy2[0][1] = 99
print(orig2[0][1])       // 2 (unchanged)
print(copy2[0][1])       // 99

// Zero-copy slice views: no copy on read
orig3 = [1, 2, 3, 4, 5]
view = orig3[1:4]        // view into orig3, no elements copied
print(view)              // [2, 3, 4]
view[0] = 99             // COW: view flattened to owned copy
print(view)              // [99, 3, 4]
print(orig3)             // [1, 2, 3, 4, 5] (unchanged)
```

**Key insight:** Sharing is invisible. There is no "borrow" concept. Every value
always has a valid refcount. When you read `arr[i]`, you get a value whose
refcount was incremented. When the variable holding it goes out of scope, the
refcount is decremented. Everything is automatic and transparent.

### Arrays

Heterogeneous, nested, zero-indexed, bounds-checked.

```tinylang
empty = []
nums  = [1, 2, 3]
matrix = [[1, 2], [3, 4]]
mixed = [1, "hello", [], [5, 6]]

// Indexing
print(nums[0])           // 1
print(nums[2])           // 3

// Multi-index
m = [[1, 2], [3, 4]]
print(m[0][1])           // 2
print(m[0, 1])           // 2 (sugar for m[0][1])

// Dynamic index chain
idx = [0, 1]
print(m[idx])            // 2

// Length
print(#nums)             // 3

// Concatenation (new array)
a = [1, 2]
b = [3, 4]
c = a + b
print(c)                 // [1, 2, 3, 4]

// Push (in-place, O(1) amortized)
arr = []
arr = arr + [10]
arr = arr + [20, 30]
print(arr)               // [10, 20, 30]

// Repetition
zeros = [0] * 5
print(zeros)              // [0, 0, 0, 0, 0]
```

#### Array Slicing

Python-style slice syntax with negative indices, omitted bounds, and step:

```tinylang
arr = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

print(arr[0:5])          // [0, 1, 2, 3, 4]
print(arr[:3])           // [0, 1, 2]
print(arr[7:])           // [7, 8, 9]
print(arr[:])            // [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
print(arr[0:10:2])       // [0, 2, 4, 6, 8]
print(arr[1:10:2])       // [1, 3, 5, 7, 9]
print(arr[::-1])         // [9, 8, 7, 6, 5, 4, 3, 2, 1, 0]
print(arr[5:2:-1])       // [5, 4, 3]
print(arr[-3:])          // [7, 8, 9]
print(arr[:-3])          // [0, 1, 2, 3, 4, 5, 6]
print(arr[-5:-2])        // [5, 6, 7]
print(arr[0:0])          // []
print(arr[0:100])        // [0..9] (clamped)
print(arr[100:200])      // [] (clamped)
```

Slice syntax general form: `arr[start:stop:step]`

- `start` defaults to `0` (positive step) or `len-1` (negative step)
- `stop` defaults to `len` (positive step) or `-len-1` (negative step)
- Negative indices wrap around by adding `len`
- Bounds are clamped to valid range
- Step cannot be zero

**Zero-copy views:** When `step == 1` (contiguous slices), the VM creates a
lightweight view into the parent array's backing store. No elements are copied
— the view records an offset and length, sharing the parent's memory via
refcounting. This means:

- `y = x[1:3]` — O(1), no allocation, no copying
- `x = x[1:]` — O(1), assigns a view back to the same variable
- `y[0] = 99` — automatically flattens the view to an owned copy (COW)
- `x[:]` — a full view of the array, still O(1)

Strided slices (`step != 1`, e.g. `arr[::2]`, `arr[::-1]`) still allocate a
new array and copy elements, since the elements are not contiguous in memory.

#### Array Concatenation vs Push

The `+` operator on arrays behaves differently depending on the operands:

- **`c = a + b`** — creates a new array `[a..., b...]`. Neither `a` nor `b` is modified. O(n).
- **`x = x + [...]`** — appends elements in-place, no temporary array. O(1) amortized per element.
- **`x = x + expr`** — pushes all elements of any array expression into `x` in-place. O(n) but avoids the final concatenated copy.

See the [Push Optimization](#4-push-optimization) section in Optimizations & VM Internals for details and examples.

#### Array Repetition

```tinylang
zeros = [0] * 10          // 10 zeros
pairs = [1, 2] * 3       // [1, 2, 1, 2, 1, 2]
zero_repeat = [5] * 0    // []
neg_repeat = [5] * (-1)  // []
```

### nil

`nil` is syntactic sugar for the empty array `[]`.

```tinylang
n = nil
print(n == [])           // 1
print(n != [])           // []
print(!n)                // 1
```

Truth table for `nil`: it is falsey, negates to `1`, and is deeply equal to `[]`.

### Multi-Indexing

`arr[i, j, k]` desugars to `arr[i][j][k]` at compile time. This works for both
reading and assignment (lvalue chains):

```tinylang
deep = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]]
print(deep[0][0][0])    // 1
print(deep[0,1,1])      // 4 (same as deep[0][1][1])
print(deep[1][0][1])    // 6
print(deep[1,1,0])      // 7

// Multi-index lvalue
mat = [[10, 20], [30, 40]]
mat[0, 1] = 99
print(mat[0][1])        // 99
```

### Index Chains

Pass an array of indices to index into nested arrays dynamically:

```tinylang
mat = [[1, 2], [3, 4]]
idx = [0, 1]
print(mat[idx])         // 2

// As lvalue
dyn = [[10, 20], [30, 40]]
dyn_idx = [0, 1]
dyn[dyn_idx] = 99
print(dyn[0][1])        // 99
```

### Functions

```tinylang
// Simple fun
fun double(x=0) {
    ret x * 2
}
print(double(5))         // 10

// Multiple parameters
fun add(a=0, b=0) {
    ret a + b
}
print(add(3, 4))         // 7

// No ret → returns [] (nil)
fun noop() { }
print(#noop())           // 0

// Recursion
fun fact(n=0) {
    if n == 0 { ret 1 }
    ret n * fact(n - 1)
}
print(fact(5))           // 120

// Tail recursion (TCO — no stack growth)
fun fact_tco(n=0, acc=1) {
    if n == 0 { ret acc }
    ret fact_tco(n - 1, n * acc)
}
print(fact_tco(1000, 1)) // inf (no stack overflow)
```

Key rules:
- `fun` keyword — defined only at top level
- Define-before-use (no forward references)
- Return type is inferred and checked: all `ret` statements in a function
  must return the same type (number or array). A mismatch halts at compile
  time with `"inconsistent return type"`.
- Comma-separated return values like `ret 1, 2, 3` are sugar for
  `ret [1, 2, 3]` — the values are wrapped in an implicit array.
  This pairs naturally with destructure: `a, b, c = foo()`.
- Functions with no `ret` statement return `[]` (nil).
- Recursion works; tail calls are optimized (TCO)
- Not first-class — cannot be stored in variables or passed as arguments
- Extra arguments are silently ignored; missing arguments use default values
- **Every parameter must have a default value.** Parameter types are determined
  at compile time from their defaults. Supported default value types:
  numbers, strings, `nil`, and array literals with constant elements.

  ```tinylang
  fun add(a=0, b=10) { ret a + b }
  add(5)       // 5 + 10 = 15
  add(5, 3)    // 5 + 3 = 8

  fun first(arr=[10, 20, 30]) { ret arr[0] }
  first()      // 10
  first([99])  // 99

  fun greet(name="world") { ret "hello " + name }
  greet()      // hello world

  // Error: parameter without default
  // fun bad(a=5, b) { }  // compile-time error
  ```

  Array defaults are evaluated once at compile time and shared across all
  function calls. Copy-on-write (COW) ensures that mutating the parameter
  does not mutate the shared default — a deep copy is triggered
  transparently on first write.

### Control Flow

#### if/elif/else

```tinylang
fun classify(n=0) {
    if n < 0 {
        ret -1
    } elif n == 0 {
        ret 0
    } else {
        ret 1
    }
}
print(classify(-5))      // -1
print(classify(0))       // 0
print(classify(42))      // 1
```

- `{}` on bodies are always required
- All conditions use `[]`-is-falsey rule
- `elif` chains can continue indefinitely

#### while

```tinylang
i = 0
for i < 5 {
    i = i + 1
}
print(i)                 // 5

// Nested for
i = 0
result = []
for i < 3 {
    j = 0
    for j <= i {
        result = result + [i]
        j = j + 1
    }
    i = i + 1
}
print(result)            // [0, 1, 1, 2, 2, 2]
```

### Include System

`include "path"` loads and compiles another TinyLang source file in place.
Paths are relative to the including file's directory.

```tinylang
// main.tl
include "lib/utils.tl"
print(greet("world"))

// lib/utils.tl
fun greet(name="") {
    ret "hello " + name
}
```

String literals are the primary way to specify include paths.
Nested includes work arbitrarily deep.

### Error Handling

Runtime errors halt execution with a message to stderr and a stack trace:

```
index out of bounds
in tests/test.tl:42: compute()
in tests/test.tl:10: process_data()
in tests/test.tl:1
```

In script mode the process exits with status 1. In the REPL, errors are caught
and the REPL continues with the next input, preserving the current scope.
No recovery, no try/catch, no assertions.

### Built-in Functions

All builtins are registered as proper native functions and called through the
normal `OC_CALL` dispatch — no special opcodes, no VM changes for new additions.

#### I/O

##### `print(x)`

Writes `x` to stdout with **no trailing newline**. Numbers print in decimal.
Arrays whose elements are all printable ASCII are printed as text strings rather
than `[104, 101, ...]`. Use `print("\n")` or embed `\n` in your strings to
produce newlines.

```tinylang
print(42)                // 42
print("hello\n")         // hello
print([1, 2, 3])         // [1, 2, 3]
```

##### `input()`

Reads a line from stdin, returns as a byte array (string).

```tinylang
name = input()
print(name)
```

##### `read(filepath, mode)`

Reads a file and returns its entire content as a string. `mode` is a standard
fopen mode string like `"r"`.

```tinylang
content = read("/tmp/data.txt", "r")
print(content)
```

##### `write(filepath, data, mode)`

Writes a string to a file. `mode` is `"w"` (overwrite) or `"a"` (append).
Binary data (NUL bytes, non-ASCII) is handled correctly.

```tinylang
write("/tmp/out.txt", "hello world\n", "w")
write("/tmp/out.txt", "more data\n", "a")
```

##### `exec(command)`

Runs a command via `/bin/sh -c` and returns stdout as a string. Uses `popen()`
internally — pipes, redirects, and all shell constructs work.

```tinylang
out = exec("echo hello world")
print(out)               // hello world\n
files = exec("ls -1 | wc -l")
```

##### `glob(pattern)`

Returns an array of file paths matching a shell glob pattern, using POSIX
`glob()`. Returns an empty array on no match.

```tinylang
files = glob("*.tl")
print(#files)            // number of .tl files
print(files[0])          // first match
```

##### `key()`

Reads a single keypress from the terminal. Returns the key as a byte array
(string). Sets the terminal to raw mode — no Enter needed, no echo. Handles
arrow keys, function keys, and Meta combinations by reading escape sequences
with a 100ms timeout. Ctrl+C won't kill the program (raw mode disables `ISIG`).
Restores the terminal after each call. Falls back to reading one byte when
stdin is a pipe.

```tinylang
k = key()
if k == "\x1b[A"
    print("up\n")
elif k[0] == 3
    print("ctrl+c\n")
elif k[0] == 113
    break   # q to quit
end
```

#### System

##### `args()`

Returns an array of strings with the command-line arguments used to invoke
tinylang for this script. Excludes the program name (element 0), similar to
`process.argv.slice(1)` in Node.

```tinylang
for i < #args() {
    print(args()[i])
}
```

##### `env(name)`

Returns the value of an environment variable as a string, or an empty string
if the variable doesn't exist.

```tinylang
path = env("PATH")
home = env("HOME")
print(env("TINYLANG_NONEXISTENT") == "")  // 1
```

##### `die(code)`

Terminates the process with an exit status code. `code` defaults to `1` when
called without arguments.

```tinylang
die()        // exit with code 1
die(0)       // exit with code 0 (success)
die(2)       // exit with code 2
```

#### Time

##### `time()`

Returns the current wall-clock time as a double with nanosecond precision, using
`clock_gettime(CLOCK_REALTIME, ...)`. The integer part is Unix epoch seconds;
the fractional part is sub-second nanoseconds.

```tinylang
ts = time()
print(ts > 1700000000)   // after Jan 2024
print(ts)                // e.g. 1779840000.123456789
```

##### `date()`

Returns an array `[year, month, day, hour, minute, second]` using the current
system timezone.

```tinylang
d = date()
print(d[0])              // year (e.g. 2026)
print(d[1])              // month (1–12)
print(d[2])              // day   (1–31)
print(d[3])              // hour  (0–23)
```

##### `sleep(seconds)`

Suspends execution for the given duration. Uses `nanosleep()` which yields the
CPU to the OS scheduler (no busy-waiting). Accepts fractional seconds for
sub-second precision.

```tinylang
sleep(1)         // sleep for 1 second
sleep(0.1)       // sleep for 100 milliseconds
sleep(0.001)     // sleep for 1 millisecond
```

#### Math

##### `sin(x)` / `cos(x)` / `sqrt(x)` / `exp(x)` / `log(x)`

Standard math functions, wrappers around `<math.h>`.

```tinylang
print(sin(0) < 0.001)     // 1
print(sqrt(4) == 2)       // 1
print(exp(1) > 2.718)     // 1
print(log(1) == 0)        // 1
```

`slog` errors on arguments ≤ 0; `sqrt` errors on negative arguments.

##### `floor(x)` / `ceil(x)` / `round(x)`

Rounding functions: `floor` rounds toward -∞, `ceil` rounds toward +∞, `round`
rounds to nearest integer (half away from zero).

```tinylang
print(floor(3.14) == 3)   // 1
print(ceil(3.14) == 4)    // 1
print(round(3.5) == 4)    // 1
print(floor(-3.14) == -4) // 1
```

##### `rand(min, max)`

Returns a uniformly distributed random double in the range `[min, max]`.
Seeded once per process via `srand(time ^ pid)`.

```tinylang
r = rand(0, 100)
print(r >= 0 && r <= 100)  // 1

// Roll a die
roll = floor(rand(1, 7))
```

#### Strings & Data

##### `split(string, separator)`

Splits a string by a separator, returning an array of substring slices. The
substrings use zero-copy slice views into the original string — no element
copying for contiguous segments. Follows JavaScript semantics:

```tinylang
parts = split("a,b,c", ",")
print(#parts)              // 3
print(parts[0])            // a
print(parts[1])            // b

// Empty separator = individual chars
chars = split("abc", "")
print(#chars)              // 3

// Trailing/leading separators produce empty strings
print(#split("a,b,", ",")[2] == 0)  // 1
print(#split(",a,b", ",")[0] == 0)  // 1
```

##### `hash(string)`

Returns the FNV-1a hash of a string using the FNV-1a algorithm.
Deterministic: same string always produces the same hash
value (unsigned 32-bit range).

```tinylang
print(hash("hello") == hash("hello"))  // 1 (idempotent)
print(hash("abc") != hash("ABC"))      // 1 (case-sensitive)
```

##### `sort(arr)`

Returns a sorted copy of an array. Numbers sort numerically, strings sort
lexicographically byte by byte, arrays sort by length then elements.
Uses `qsort` internally.

```tinylang
arr = sort([3, 1, 4, 1, 5])
print(arr)                 // [1, 1, 3, 4, 5]

words = sort(["banana", "apple", "cherry"])
print(words[0])            // apple
```

##### `set(arr)`

Returns an array of unique elements, preserving the order of first occurrence.
Uses deep equality (`==`) for comparison.

```tinylang
uniq = set([1, 2, 2, 3, 1, 4])
print(#uniq)               // 4
print(uniq)                // [1, 2, 3, 4]
```

##### `flat(arr)`

Recursively flattens nested arrays into a single flat array. Strings are not
flattened (they are kept as-is).

```tinylang
f = flat([1, [2, [3, 4], 5], 6])
print(f)                   // [1, 2, 3, 4, 5, 6]

// Strings are not flattened
fs = flat(["a", ["b"], "c"])
print(#fs)                 // 3
```

#### Path

##### `thisfile()`

Returns the full file path of the current script where the call appears,
as a byte array. Always returns the literal file path regardless of whether
it's called inside an included file.

```tinylang
print(thisfile())        // e.g., /Users/mimi/project/test.tl
```

---

## Language Idioms

This section documents the idiomatic style and patterns that TinyLang code
naturally evolves toward.  Some patterns arise from compiler constraints
(define-before-use, no closures), others from performance considerations
(COW, move semantics), and others from the minimal type system.

### Preferred Style

#### Formatting

- Use **2-space indentation** — no tabs.
- Opening braces go on the same line as the control structure (`if expr {`).
- `elif` and `else` must appear on the **same line** as the preceding `}`:
  ```tinylang
  if x < 0 {
      ret -1
  } elif x == 0 {          // } and elif on same line
      ret 0
  } else {
      ret 1
  }
  ```
  The parser does not skip newlines looking for `elif`/`else`; putting them on
  a new line produces an "unexpected token" error.
- Short 2-branch `if`-`elif` chains may stay on one line:
  ```tinylang
  if mx < px { dx = 1 } elif mx > px { dx = -1 }
  ```
- Longer chains (3+ branches) use multi-line formatting as shown above.
- Separate logically distinct `if` statements (mutually exclusive conditions)
  rather than forcing a single `elif` chain:
  ```tinylang
  if k0 == key_UP_W  || k0 == key_UP_K   { dy = -1 }
  if k0 == key_DOWN_S|| k0 == key_DOWN_J { dy = 1 }
  ```
- Multi-line array literals (`[...]`) are **not** supported — keep them on one
  line.  The parser treats newlines inside brackets as unexpected tokens.

#### Naming Conventions

| Kind | Convention | Example |
|---|---|---|
| Functions | `snake_case` | `player_new`, `monster_find_at` |
| Private helpers | `_namespace_method` | `_player_descend`, `_mapgen_rooms` |
| Field index functions | `_classname_FIELD` | `_player_HP()` |
| Class constants | `_classname_CONSTANT` | `_monster_GOBLIN()` |
| Top-level constants | `namespace_NAME` | `tile_WALL`, `map_W` |
| Variables | `snake_case` | `player`, `monsters` |

#### Early Returns and Short Circuits

Use **guard clauses** to flatten nesting.  Validate conditions early and return:

```tinylang
fun _ai_try_move(nx=0, ny=0, mx=0, my=0, map=[]) {
    if nx < 0 || nx >= MW || ny < 0 || ny >= MH { ret [mx, my] }
    if map[ny][nx] == 0 { ret [mx, my] }
    // main path continues here without extra indentation
}
```

Prefer the short-circuit logical operators `&&` and `||` over nested `if`:

```tinylang
// Clear:
if x >= 0 && x < width && y >= 0 && y < height { ... }

// Instead of nested:
if x >= 0 {
    if x < width {
        if y >= 0 { ... }
    }
}
```

#### Structured Programming

- **One function, one responsibility.**  When a block reaches 15–20 lines or
  acquires a second level of nesting, extract it into a private helper.
- Functions communicate exclusively through parameters and return values —
  no globals, no closures.
- Public orchestration functions call private helpers in sequence:
  ```tinylang
  fun mapgen_level(lvl=1, MW=60, MH=20) {
      rooms    = _mapgen_rooms(MW, MH)
      map      = _mapgen_fill(rooms, MW, MH)
      map      = _mapgen_carve(rooms, map, MW, MH)
      start    = _mapgen_stairs(map, rooms, MW, MH)
      ret level_new(map, vis, monsters, items, start[0], start[1])
  }
  ```
- **Avoid deeply nested or very long functions.**  A function over ~30 lines is
  a strong signal it should be decomposed.  The largest function in the
  roguelike demo is `render_frame` at 231 instructions — a fraction of the
  original 821-instruction monolith.
- **Avoid too many function parameters.**  If a function needs more than 5–7
  parameters, consider grouping related values into an array (struct) and
  passing that instead.

### Common Operations

#### Push to Array (In-Place Append)

The canonical way to append elements to an array uses the `+` operator with
assignment to the same variable.  The compiler recognises this pattern and
emits the push optimisation — O(1) amortised per element, no temporary array:

```tinylang
arr = []
arr = arr + [10]          // single element, O(1) amortised
arr = arr + [20, 30]      // multi-element, O(1) per element

// In a loop — O(n) total, not O(n²):
i = 0
for i < 100000 {
    arr = arr + [i]
    i = i + 1
}
```

The compound assignment `+=` desugars to the same pattern and benefits from
the same optimisation:

```tinylang
arr = []
arr += [10]
arr += [20, 30]
```

#### Mutate a Table (Struct) with a Function

Because TinyLang uses copy-on-write (COW) with value semantics, mutating a
passed array does **not** affect the caller's variable.  The idiom is to
mutate inside the function, **return the object**, and have the caller
**reassign**:

```tinylang
fun player_damage(p=[], v=0) {
    p[_player_HP()] = p[_player_HP()] - v
    if p[_player_HP()] < 0 { p[_player_HP()] = 0 }
    ret p                     // ← return the modified object
}

// Caller must reassign:
player = player_damage(player, 5)
```

For systems that modify multiple objects, return a tuple and destructure:

```tinylang
r = ai_process_monsters(level, player, messages)
level    = r[0]
player   = r[1]
messages = r[2]
```

The **move-semantics optimisation** in the VM detects `x = f(x, ...)` and
eliminates the COW deep copy — ownership is transferred directly when the
array is exclusively owned.

#### Const (Compile-Time Constants via Functions)

TinyLang functions cannot see top-level variables, but they **can** see other
functions.  The idiomatic way to create a named constant accessible from any
function is a tiny function that returns a literal value.  The inliner
eliminates the call overhead entirely:

```tinylang
// Field index constant — inlined to OC_NUM
fun _player_HP()    { ret 0 }
fun _player_MAXHP() { ret 1 }

// Class constant — inlined to OC_NUM or OC_STR
fun _monster_GOBLIN() { ret 0 }
fun _item_POTION()    { ret 0 }
```

```tinylang
// Usage in any function:
p[_player_HP()] = p[_player_HP()] + 5
```

For top-level constants that only top-level code needs (no function access
required), use plain variables with a namespace prefix:

```tinylang
tile_WALL   = 0
tile_FLOOR  = 1
key_QUIT_Q  = 113
```

### Data Structures

#### Arrays as Fixed-Length Structs

The primary data structure idiom is a fixed-length array whose fields are
accessed by named index functions.  This is the closest TinyLang has to a
struct or class:

```tinylang
// Layout: [hp, maxhp, atk, def, gold, level, kills]
// Each field is a named index function:
fun _player_HP()    { ret 0 }
fun _player_MAXHP() { ret 1 }
fun _player_ATK()   { ret 2 }
// ...

// Constructor pre-allocates with [0] * N:
fun player_new(hp=0, maxhp=0, atk=0) {
    p = [0] * 7
    p[_player_HP()]    = hp
    p[_player_MAXHP()] = maxhp
    p[_player_ATK()]   = atk
    ret p
}

// Access:
p[_player_HP()]
```

#### Linked Lists (Lisp-Style)

TinyLang supports Lisp-style linked lists naturally through nested arrays.
A cons cell is `[value, rest]` — a 2-element array where the first element
is the value and the second is either another cons cell or `nil` (empty
array) to terminate the list:

```tinylang
// Build a linked list: (1 2 3 4)
list = [1, [2, [3, [4, nil]]]]

// Access head and tail:
head = list[0]           // 1
tail = list[1]           // [2, [3, [4, nil]]]

// Iterate recursively with tail-call optimisation:
fun sum(list=[], acc=0) {
    if list == nil { ret acc }
    ret sum(list[1], acc + list[0])   // TCO — no stack growth
}

print(sum([1, [2, [3, [4, nil]]]]))    // 10
```

Functions that operate on linked lists follow the classic recursive pattern,
safe for arbitrary depth thanks to guaranteed tail-call optimisation:

```tinylang
fun length(list=[], acc=0) {
    if list == nil { ret acc }
    ret length(list[1], acc + 1)
}

fun map(fn_name="", list=[], result=[]) {
    // fn_name dispatch via if/elif chain
}
```

#### Hashmaps (String Keys via `hash()`)

TinyLang does not have built-in string-keyed array indexing (`arr["foo"]`).
Instead, use the `hash()` built-in function to compute an FNV-1a hash of a
string and index into an array manually:

```tinylang
arr = [0, 0, 0, 0, 0]
idx = hash("hello") % #arr
arr[idx] = 42
print(arr[hash("hello") % #arr])   // 42
```

**Deterministic:** the same string always hashes to the same value, so
`hash("hello") % N` always produces the same index for a given N.

**Collisions:** different strings may hash to the same bucket (index).
With a simple flat array, the last write wins:

```tinylang
map = [0, 0, 0, 0, 0]
map[hash("hello") % #map] = 10
map[hash("world") % #map] = 20  // overwrites if same bucket
```

**Collision-safe storage:** use an array of arrays (`[[]] * N`) and
store multiple values per bucket:

```tinylang
buckets = [[]] * 5
b = hash("foo") % #buckets
buckets[b] = buckets[b] + [10]   // push into bucket
buckets[b] = buckets[b] + [20]
```

**Full hashmap with key-value pairs and chaining:**

```tinylang
fun hashmap_new(n=0) { ret [[]] * n }

fun _hm_find(bucket=[], key="") {
    i = 0
    for i < #bucket {
        if bucket[i][0] == key { ret i }
        i = i + 1
    }
    ret -1
}

fun hashmap_set(map=[], key="", val=0) {
    n = #map
    if n == 0 { ret map }
    b = hash(key) % n
    bucket = map[b]
    j = _hm_find(bucket, key)
    if j >= 0 {
        bucket[j][1] = val          // overwrite existing key
        map[b] = bucket
    } else {
        map[b] = bucket + [[key, val]]  // append new pair
    }
    ret map
}

fun hashmap_get(map=[], key="") {
    n = #map
    if n == 0 { ret [] }
    b = hash(key) % n
    j = _hm_find(map[b], key)
    if j >= 0 { ret map[b][j][1] }
    ret []
}

// Usage
m = hashmap_new(10)
m = hashmap_set(m, "name", 42)
m = hashmap_set(m, "name", 100)     // overwrite
print(hashmap_get(m, "name"))       // 100
```

Key concepts:
- `hash(key) % N` maps any string to a deterministic bucket index
- Each bucket is an array of `[key, value]` pairs (chaining for collisions)
- `_hm_find` searches a bucket linearly for a matching key
- On overwrite, COW makes a copy of the bucket; write it back to `map[b]`
- The `hash()` value and the number of buckets determine the memory/performance
trade-off: more buckets → fewer collisions → faster lookups

### OOP Style

TinyLang has no classes, no objects, and no methods.  The OOP style is an
idiomatic pattern that emulates classes using arrays (structs), private
helper functions (methods), and a naming convention (namespacing).

#### Namespacing

All functions belonging to a conceptual "class" share a prefix matching the
file name.  This is enforced by the **one class per file** convention:

```
player.tl     →  all functions start with player_
monster.tl    →  all functions start with monster_
level.tl      →  all functions start with level_
```

Private functions (not called from outside the module) use a leading
underscore: `_player_HP`, `_mapgen_rooms`.

#### Class Structure

Every conceptual class consists of four layers:

| Layer | Convention | Example |
|---|---|---|
| Field index functions | `_classname_FIELD()` → ret N | `_player_HP() { ret 0 }` |
| Constructor | `classname_new(...)` | `player_new(hp=0, ...) { p=[0]*N; ... ret p }` |
| Accessors | `classname_field(obj)` → ret value | `player_hp(p) { ret floor(p[_player_HP()]) }` |
| Mutators | `classname_action(obj, ...)` → ret modified obj | `player_damage(p, v) { ... ret p }` |

#### Field Index Functions

These are the foundation.  Every field of the struct has a tiny function
returning its index.  To reorder fields, change only these `ret N` values
and the constructor — all accessors and mutators follow automatically:

```tinylang
// player.tl — [hp, maxhp, atk, def, gold, level, kills]

fun _player_HP()    { ret 0 }
fun _player_MAXHP() { ret 1 }
fun _player_ATK()   { ret 2 }
fun _player_DEF()   { ret 3 }
fun _player_GOLD()  { ret 4 }
fun _player_LEVEL() { ret 5 }
fun _player_KILLS() { ret 6 }
```

All field index functions are inlined at compile time (the inliner detects
`fun _() { ret CONSTANT }`), so the call overhead is zero.

#### Constructor

Pre-allocate the array with `[0] * N` (or `[[]] * N` for array-valued
fields), then assign each field by its named index:

```tinylang
fun player_new(hp=0, maxhp=0, atk=0, def=0, gold=0, lvl=1, kills=0) {
    p = [0] * 7
    p[_player_HP()]    = hp
    p[_player_MAXHP()] = maxhp
    p[_player_ATK()]   = atk
    p[_player_DEF()]   = def
    p[_player_GOLD()]  = gold
    p[_player_LEVEL()] = lvl
    p[_player_KILLS()] = kills
    ret p
}
```

For objects that store arrays, use `[[]] * N`:

```tinylang
fun level_new(map=[], vis=[], monsters=[], items=[], px=0, py=0) {
    l = [[]] * 6
    l[_level_MAP()]  = map
    l[_level_VIS()]  = vis
    l[_level_MONSTERS()] = monsters
    l[_level_ITEMS()]     = items
    l[_level_PX()]   = px
    l[_level_PY()]   = py
    ret l
}
```

#### Accessors

Read a single field and return it.  Numeric accessors **must** wrap the
value with `floor()` — TinyLang's type inference treats `ret arr[idx]` as
`T_UNKNOWN`, making the function default to `T_ARR_TYPE`.  `floor()` is
a built-in whose return type is registered as `T_NUM_TYPE`, propagating
the correct numeric type:

```tinylang
fun player_hp(p=[])    { ret floor(p[_player_HP()]) }
fun player_maxhp(p=[]) { ret floor(p[_player_MAXHP()]) }

// Array-valued accessors do NOT need floor():
fun level_map(l=[])        { ret l[_level_MAP()] }
fun level_monsters(l=[])   { ret l[_level_MONSTERS()] }
```

#### Mutators

Mutate one or more fields, then **return the object** so the caller can
reassign (see the mutate-with-function idiom above):

```tinylang
fun player_damage(p=[], v=0) {
    p[_player_HP()] = p[_player_HP()] - v
    if p[_player_HP()] < 0 { p[_player_HP()] = 0 }
    ret p
}

// Higher-level mutators compose the primitives:
fun player_descend(p=[]) {
    p[_player_LEVEL()] = p[_player_LEVEL()] + 1
    p[_player_MAXHP()] = p[_player_MAXHP()] + 5
    p[_player_HP()]    = p[_player_MAXHP()]
    ret p
}

// Caller reassigns:
player = player_damage(player, 5)
player = player_descend(player)
```

---

## Optimizations & VM Internals

TinyLang is a bytecode interpreter with a deliberately minimal implementation.
Despite its simplicity, it incorporates several optimizations that dramatically
improve performance over a naive interpreter. 

Because every variable has exactly one type and every function has exactly one
scope, the compiler can:

- **Resolve variables to integer slots** at compile time — O(1) runtime access
  with zero string comparisons.
- **Inline array append** — the compiler
  recognises `x = x + [e]` and emits an O(1) amortised append.
- **Inline slice truncation** — the compiler
  recognises `x = x[slice]` and mutates the array in-place when exclusive.
- **Detect tail calls** by scanning the last few emitted instructions — no
  separate analysis pass needed.
- **Dispatch opcodes via C99 goto-to-switch** — a single `switch` with `goto`
  cases dispatches all opcodes; the compiler optimises it into a jump table.
- **Pre-allocate scopes** with the exact number of variables known at compile
  time — no hash tables, no dynamic growth.

### 1. Goto-to-Switch Dispatch

The VM uses a **goto-to-switch** dispatch pattern — standard C99, no GNU
extensions. Each opcode handler ends with `goto dispatch`, and the single
`dispatch:` label contains a `switch` on the opcode with `goto` cases:

```c
void exec(Code *c) {
    int ip = 0;
    goto dispatch;

op_num:
    // ... handler ...
    ip++; goto dispatch;

op_var:
    // ... handler ...
    ip++; goto dispatch;

    /* ── Central dispatch ── */
dispatch:
    switch (c->code[ip].op) {
    case OC_NUM: goto op_num;
    case OC_VAR: goto op_var;
    // ... all opcodes ...
    }
}
```

**Why it's fast:** The compiler recognises this pattern and generates a
single byte-compressed jump table (one `br` instruction). Unlike a
`for(;;){switch{...}}` loop, there's no bounds check on the hot path — Clang
eliminates it because all cases are covered. The single indirect branch at
`dispatch:` trains the BTB perfectly, avoiding mispredictions that plague
scattered computed-goto dispatch sites. Despite 2 extra instructions per
iteration (bounds check + branch back), real-world benchmarks show
**identical wall-clock time** to the computed goto version — the extra work
is hidden by OoO execution while BTB pressure is reduced.

### 2. Slot-Indexed Variable Access

Inside function bodies, variables are accessed by integer slot index rather
than by name lookup. The compiler assigns each variable a slot during
compilation, and the VM reads/writes via `cs->v[slot]` — a direct array access.

```c
// Before (strcmp lookup — O(n) per access):
case OC_VAR:
    for (int i = 0; i < cs->c; i++)
        if (!strcmp(cs->n[i], name)) return cs->v[i];

// After (slot-indexed — O(1) per access):
case OC_VAR_SLOT:
    istk[++isp] = cs->v[slot];
    return;
```

**Impact:** ~50% reduction in variable access cost. The biggest win for
variable-heavy loops. Combined with goto-to-switch dispatch, the total speedup
over a naive switch+strcmp VM is **55–85% across benchmarks.**

### 3. Compile-Time Type Tracking

The compiler maintains a parallel `comp_types[]` array alongside `comp_vars[]`,
tracking whether each variable holds a number (`T_NUM_TYPE`), generic array
(`T_ARR_TYPE`), or string (`T_STR_TYPE`). A `peek_expr_type()` function walks
the token stream ahead of compilation to infer expression types from literals,
variables, function calls, and binary operators.

Key properties:
- **First assignment determines type.** `set_var_type()` records the type on
  first assignment; any subsequent attempt to assign a different type halts
  at compile time with `"type mismatch"`. This means `x = "hello"; x = [1,2,3]`
  is an error — a string variable cannot become a generic array.
- **Function return types are checked.** Each `return expr` infers its type;
  all returns in a function must agree. Functions with no return get
  `ret_type = T_ARR_TYPE` (they return `[]`).
- **Indexed expressions are unknown.** `arr[i]` returns `T_UNKNOWN` — element
  types are not tracked at compile time.
- **Binary operator inference.** `num + num = num`; anything else with `+` =
  array; `arr * num = array`; all other ops produce numbers. Unknown
  operands produce unknown results.

The type information drives the push optimization decisions, enables
slot initialization at scope creation time, and enables dedicated
numeric opcodes for compile-time-known number operations.

When the compiler tracks `comp_last_type` through `comp_prim()` and
`comp_expr_prec()`, it knows the type of every sub-expression at compile
time. When both operands of `+`, `-`, `*`, or `/` are `T_NUM_TYPE`, it
emits a dedicated opcode (`OC_ADD_NUM`, `OC_SUB_NUM`, `OC_MUL_NUM`,
`OC_DIV_NUM`) that does the arithmetic inline in 3-4 C statements,
skipping the 15-operator `apply()` switch entirely.

For compound assignment on arrays (`arr[idx] op= delta`), the compiler
emits `OC_MUTATE_NUM` — a fused opcode that evaluates the index once,
reads the current element, applies the operator with the delta, and
writes back. This eliminates the duplicate index evaluation that token
rewriting produces, saving ~4 bytecodes per mutation.

### 4. Push Optimization

The compiler detects `x = x + [...]` at compile time and replaces it with
direct in-place appends instead of creating a new concatenated array.

#### When temporary arrays are created

A **temporary array** (or *temp*) is created whenever `apply(T_PL, ...)`
concatenates two arrays — it allocates a new `Arr`, copies all elements from
both sources, and returns it. The old arrays are released afterward. This is
O(n) time and O(n) memory per concatenation.

With the push optimization, temps are reduced or eliminated depending on
the expression form:

| Expression | Temps | Time per element | Why |
|------------|-------|-----------------|-----|
| `c = a + b` | 1 temp | O(n) | Creates new `[a..., b...]`. Required — `c` is a different variable. |
| `x = x + [e]` | **0 temps** | **O(1) amortized** | Appends `e` directly into `x`'s array. No copy at all. |
| `x = x + [a, b, c]` | **0 temps** | **O(1) each** | Three appends, one per element. No temp array created. |
| `x = x + fn()` | **1 temp** *(fn result)* + **0 final** | O(n) | The function's return array exists anyway; push-all avoids the final `x + result` concat copy and store-back. |
| `x = x + a + b + c` | **1 temp** *(RHS expression)* + **0 final** | O(n) | The naive path creates 3 temps (`a+b`, `(a+b)+c`, `(x+rhs)`) + store copy. Push-all creates only the RHS temp (`a+b+c`) then pushes in-place. |
| `x = x + ([a,b] + [c,d])` | **1 temp** + **0 final** | O(n) | The bracketed `[a,b] + [c,d]` creates 1 temp, then push-all moves its elements. |

#### Per-element push

When the RHS is a bare bracket literal `[a, b, ...]` with no chained
operators following, each element is appended directly. No temporary array
is created at all — not even for the bracket literal itself.

```tinylang
// Single element — O(1) amortized, 0 temps
arr = []
arr = arr + [99]
print(#arr)              // 1

// Multi-element — O(1) per element, 0 temps
arr = [1, 2]
arr = arr + [3, 4, 5]   // three individual appends
print(arr)               // [1, 2, 3, 4, 5]

// Building an array in a loop — O(n) total, 0 temps per iteration
arr = []
i = 0
for i < 1000 {
    arr = arr + [i]      // O(1) each — no array copy
    i = i + 1
}
print(#arr)              // 1000
```

**Why it matters:** Without push, `arr = arr + [i]` in a loop would create
a new array on every iteration, copy all existing elements, and discard the
old one — O(n²) total. With push, each iteration is O(1) amortized, making
the whole loop O(n).

**Multiple temps in a chain (naive path):** Without push optimization,
each `+` creates a new temp. The entire chain of temps is allocated,
copied, and freed:

```tinylang
// Naive path — creates 3 temps + 1 store copy
a = [1]
a = a + [2] + [3] + [4]
// temp1 = a + [2]        = [1, 2]     ← alloc + copy 2 elements
// temp2 = temp1 + [3]    = [1, 2, 3]  ← alloc + copy 3 elements
// temp3 = temp2 + [4]    = [1, 2, 3, 4]  ← alloc + copy 4 elements
// a = temp3                           ← store (copy via vassign)
// Total: 3 allocs, 9 element copies, 1 store

// Push-all path — 1 temp, no store
// temp = [2] + [3] + [4]   = [2, 3, 4]  ← alloc + copy 3 elements
// push all of temp into a                 ← mutate in place, 0 copies
// Total: 1 alloc, 3 element copies, 0 store
```

The naive path also creates an increasingly large temp at each step because
`a`'s elements are copied into each successive temp. Push-all defers the
work to the end — only the RHS expression chain creates temps.

For a chain of **n** concatenations `x = x + a1 + a2 + ... + an`:

| Path | Temps | Element copies | Store copies |
|------|-------|----------------|-------------|
| Naive | n | O(n²) | 1 |
| Push-all | n−1 | O(n) | **0** |

With push-all, the RHS chain `a1 + a2 + ... + an` creates n−1 temps
(the intermediate concatenations), then pushes all elements into `x`
in-place with zero additional copies of `x`'s existing data.

#### Push-all

When the RHS is any expression other than a bare bracket literal (function
call, variable, slice, chained `+`, parenthesized expression), the compiler
emits `OC_PUSH_ALL`. The RHS is evaluated normally (which may create temps
as part of the expression evaluation), then all elements of the result are
pushed into `x` in-place. The final concatenation copy and store-back are
eliminated.

```tinylang
// Function call — fn returns an array, elements pushed into x
arr = []
arr = arr + make_array()   // fn's ret array is consumed, no final copy

// Chained + — RHS produces one temp, then all elements pushed
arr = [10, 20]
arr = arr + [30] + [40, 50]    // naive: arr+[30] = temp1, temp1+[40,50] = temp2, store
                                // push-all: [30]+[40,50] = temp, push into arr

// From a variable — no temp from the read, just in-place append
base = [1, 2, 3]
extra = [4, 5]
base = base + extra             // pushes extra's elements into base

// From a slice — slice creates one temp, then pushes into target
nums = [1, 2, 3, 4, 5]
nums = nums + nums[2:#nums]     // slice [3,4,5] is temp, then push elements into nums

// Parenthesized expression — same as any other expression
arr = [100]
arr = arr + ([200] + [300])     // [200]+[300] creates temp, push into arr
```

**How push-all saves:** Without it, `x = x + fn()` would: evaluate `fn()`
(creating the return array), then `apply(T_PL, x, fn_result)` would allocate
a new array and copy all of `x`'s elements plus `fn_result`'s elements into
it, then release both `x`'s old array and the `fn_result` array. With
push-all, `fn_result`'s elements are moved directly into `x`'s existing
array — no extra allocation, no copy of `x`'s existing elements, no
store-back.

#### Guard conditions

Both optimizations are guarded by compile-time type tracking — they only fire
when `x` is known to be an array. This prevents silent data corruption from
uninitialized slots. Copy-on-write (COW) ensures that if `x` is shared with
another variable, a deep copy happens before mutation, preserving value
semantics.

#### In-place slice optimization (`x = x[slice]`)

Just as `x = x + [...]` is fused into `OC_PUSH`, the compiler detects
`x = x[start:stop:step]` and emits a single `OC_SLICE_INPLACE` opcode.
This is the same lookahead pattern: after `=`, the compiler checks if the
RHS starts with the same variable name followed by `[...:` (slice bracket).

At runtime, the opcode handles three cases:

1. **Exclusive ownership + numbers-only + step==1** — The array is mutated
   in-place via `memmove` and length trim. No allocation, no view created.
   This prevents view chain accumulation in loops like `while i < n { x = x[1:] }`.

2. **Shared array, sub-arrays, or step==1** — Falls back to a zero-copy view
   (identical to `OC_SLICE` behavior).

3. **step != 1** — Falls back to a full copy (identical to `OC_SLICE` behavior).

```tinylang
// In-place: exclusively owned, numbers only
x = [1, 2, 3, 4, 5]
x = x[2:]
print(x)                 // [3, 4, 5] (memmove + trim, no view)

// Falls back to view: shared array
y = [10, 20, 30, 40, 50]
z = y
y = y[1:]                // refcount > 1, creates a view

// Falls back to view: sub-arrays
w = [[1], [2], [3]]
w = w[1:]                // contains sub-arrays, creates a view
```

### 5. Copy-on-Write (COW) Arrays

Arrays use reference counting with copy-on-write to implement value semantics
without unnecessary copying:

```c
void amake_uniq(Value *v) {
    if (v->type != VAL_ARR || !v->arr) return;
    if (v->arr->is_slice) {
        Arr *old = v->arr;
        v->arr = adeep_copy(old);    // always flatten views to owned copy
        arelease(old);
    } else if (v->arr->refcount > 1) {
        Arr *old = v->arr;
        v->arr = adeep_copy(old);    // deep copy only when shared
        arelease(old);
    }
}
```

**Key behavior:**
- **Read-sharing is free** — reading `arr[i]` on a shared array or slice view
  increments the refcount, no copy.
- **Slice views are always flattened on write** — views share memory with their
  parent, so they can never be mutated in-place. The COW machinery transparently
  copies the viewed elements to a new owned array before mutation.
- **Owned arrays copy only when shared** — writing `arr[i] = x` calls
  `amake_uniq`, which
  deep-copies the array only if `refcount > 1`.
- **Nested COW** — mutating a sub-array deep-copies only that sub-array, not
  the entire tree.

This avoids GC pauses (deterministic cleanup) while keeping memory usage low.
In benchmarks, TinyLang uses **3–12× less memory than Node.js** for the same
computation.

### 6. Tail Call Optimization (TCO)

When a function ends with `return f(args...)` where `f` is the function itself,
the compiler detects this at compile time and replaces the call with a direct
instruction-pointer reset that rebinds parameters without allocating a new C
stack frame.

```tinylang
// Tail-recursive: never overflows the C stack
fun countdown(n=0) {
    if n == 0 { ret 0 }
    ret countdown(n - 1)   // TCO
}

// NOT tail-recursive: still uses C stack
fun broken(n=0) {
    if n == 0 { ret 0 }
    ret 1 + broken(n - 1)  // needs to multiply after ret
}
```

Without TCO, `fact_tco(1000, 1)` would recurse 1,000 C stack frames deep and
overflow. With TCO, each iteration reuses the same frame — `fact_tco(100000, 1)`
runs just as safely as iteration 1.

### 7. Single-Pass Compilation (No AST)

The compiler walks the pre-lexed token array and emits bytecode directly —
no intermediate AST (Abstract Syntax Tree) is built:

```
Source → Lexer (Tok[]) → Compiler → Instr[] (bytecode) → VM
```

This means compilation is essentially free — the entire program is compiled in
a single pass with no tree allocations, no visitor patterns, and no memory
overhead for intermediate representations.

The type inference system (`peek_expr_type`) uses **token-level lookahead** —
it walks the same token array ahead of the compiler to determine expression
types, without emitting code, building structures, or modifying the stream.
This is the same technique used for push optimization detection
(`is_bracket_literal`). No separate analysis pass is needed.

### 8. Parameter Binding by Slot Index

Function parameters are bound directly by slot index at call time, bypassing
name lookup entirely:

```c
// Parameter slots are pre-computed during compilation
f->p_slots = malloc(pa * sizeof(int));
for (int i = 0; i < pa; i++)
    f->p_slots[i] = var_find(params[i]);

// At call time — O(1) per parameter, no strcmp
for (int j = 0; j < f->a; j++)
    cs->v[f->p_slots[j]] = (j < ac) ? args[j] : nilv();
```

### 9. Pre-Sized Scopes with Slot Initialization

When a function is called, its scope is allocated with the exact number of
variables known at compile time (`snew_sized(f->nvars)`). No incremental
growing, no reallocation during execution.

### 10. Zero-Copy Slice Views

Contiguous slices (`step == 1`) do not copy elements. Instead, the VM creates
a lightweight `Arr` with `is_slice = 1` whose `val` pointer points directly
into the parent's backing store at the appropriate offset:

```c
if (step == 1) {
    /* Zero-copy view: share parent's backing store */
    Arr *view = malloc(sizeof(Arr));
    view->len = count;
    view->val = src->val + start;   // pointer arithmetic
    view->is_slice = 1;
    view->parent = src;
    aretain(src);                    // keep parent alive
    // ... push view to stack
}
```

**How it works:**
- The view's `val` is a pointer into the parent's `val[]` at offset `start`.
  Element reads via `view->val[i]` resolve to `parent->val[start + i]`.
- The view retains the parent (`aretain`), keeping the backing store alive
  for the lifetime of the view.
- When the view is released, it releases the parent (not its own `val`,
  which belongs to the parent).

**Mutation triggers flattening:** The existing `amake_uniq` COW machinery
handles views: if the value is a slice view, it is always flattened to an
owned copy before mutation (views share memory with the parent, so
in-place modification is never safe):

```c
if (v->arr->is_slice) {
    Arr *old = v->arr;
    v->arr = adeep_copy(old);      // copies viewed elements
    arelease(old);                  // releases parent
}
```

This means all mutation paths — indexed assignment, push, push-all, lvalue
chains — automatically flatten views without any special-case awareness.

**Performance impact:**
- `x = x[1:]` — O(1), assigns a view, no copy
- `y = x[1:3]` — O(1), creates a view, no copy
- `y[0] = 99` — O(n) on the view size (first mutation flattens), O(1) thereafter
- `x[5] = 42` when views exist — O(n) on the parent (COW copies parent),
  views still point to old data
- Strided slices (`step != 1`) — still O(n) copy

### 11. Function Inlining (Compile-Time)

The compiler detects simple function patterns at compile time and replaces the
call with inline instructions, completely eliminating the dispatch overhead
(scope allocation, parameter binding, stack save/restore, return).

**Detected patterns:**

1. **Constant functions** (zero args, always return same value):
   `fun foo() { ret 42 }` → inline as `OC_NUM 42`
   `fun foo() { ret nil }` → inline as `OC_NIL`
   `fun foo() { ret "hi" }` → inline as `OC_STR "hi"`

2. **Accessor with constant index** (one arg, returns `obj[N]`):
   `fun first(obj=[]) { ret obj[0] }` → inline as `OC_NUM 0` + `OC_INDEX`

3. **Accessor with parameter index** (two args, returns `obj[field]`):
   `fun elem(obj=[], i=0) { ret obj[i] }` → inline as `OC_INDEX`

4. **Floor-wrapped accessors**: `fun hp(p=[]) { ret floor(p[HP]) }` →
   inline as `OC_NUM HP` + `OC_INDEX` + `CALL floor` (saves an accessor dispatch
   but keeps the builtin `floor()` fast-path call)

The inliner also catches the field-index functions used in the OOP style
(`_player_HP() { ret 0 }`, `_monster_X() { ret 0 }`), eliminating those
call dispatches entirely.

**Impact:** Eliminates ~40% of function call dispatches in OOP-style code.
Field index functions (33 in the roguelike demo) are fully inlined.
Accessors with `floor()` wrappers save one dispatch level each.

### 14. Move-Semantics COW Avoidance

When a function mutates a passed array and returns it, the standard OOP pattern
`x = f(x, ...)` normally triggers a COW deep copy inside the function (because
the parameter creates a shared reference). The move-semantics optimization
eliminates this copy by transferring ownership instead of sharing.

**How it works at compile time:**

When the compiler detects `x = f(x, ...)` (same variable as first arg and
assignment target), it emits an `OC_CLEAR_SLOT` instruction after pushing
x's value. This releases x's slot, leaving only the stack reference.

**How it works at runtime:**

Inside `OC_CALL`, the parameter binding checks the array's refcount before
retaining:

```c
if (args[j].type == VAL_ARR && args[j].arr) {
    if (args[j].arr->refcount > 1) aretain(args[j].arr);
}
```

When refcount is 1 (exclusive ownership — the slot was cleared), the retain
is skipped. The function takes ownership of the array with no shared
references. `amake_uniq` sees refcount == 1 and does **not** deep-copy,
so the mutation happens in-place.

When the function returns the array, the caller stores it back in the slot.

**Safety:** If another variable shares the array (refcount > 1), the retain
fires as normal. The COW copy inside the function protects the other
reference — value semantics are preserved. The optimization only elides
the copy when the array is exclusively owned.

**Impact:** Eliminates ~90% of COW deep copies in OOP-style mutation patterns.
Player updates, monster movement, and level mutations all benefit.

#### Runtime detection

Indexing with an array of numbers chains through multiple indices:

```tinylang
arr[[0, 1]]    // chain-based: arr[0][1]
```


### Cumulative Optimization Impact

| Optimization | Speedup vs Naive | Description |
|-------------|-----------------|-------------|
| Goto-to-switch dispatch | ~0% (matches CG) | Single jump table, no bounds check overhead |
| Slot-indexed variables | ~50% on var access | O(1) array index vs O(n) strcmp |
| Compile-time type tracking | Enables all below | Static types eliminate runtime dispatch |
| Push optimization | O(n²)→O(n) on array builds | Amortized O(1) append vs full copy |
| Push-all | Eliminates final copy+store | In-place mutation for any RHS array expr |
| Slice views | O(n)→O(1) on contiguous slices | Zero-copy views; copied only on mutation |
| Slice in-place | Eliminates view allocation | `x = x[slice]` mutates directly when exclusive |
| COW sharing | Variable, workload-dependent | Zero-copy reads, copy only on write |
| Slot initialization | Eliminates runtime guards | Array slots pre-initialized to `[]` |
| **Function inlining** | ~40% fewer dispatches | Inlines constant funcs, accessors at compile time |
| **Move-semantics COW** | ~90% fewer COW copies | `x = f(x, ...)` passes by move, avoids deep copy |
| Single-pass compiler | ~0 (constant factor) | No AST allocation overhead |
| Combined (dispatch + slots + types + inline + move) | **55–85%** | Across all benchmarks |

---

## Performance Comparisons: TinyLang vs C vs Node.js vs Python

I ported four benchmarks from the [Computer Language Benchmarks
Game](https://benchmarksgame-team.pages.debian.net/benchmarksgame/) to
TinyLang, C, Node.js (V8), and Python (CPython) and compared their
performance on an Apple MacBook Air M1 (16GB, macOS 15.7.5).

Benchmark sizes were reduced 10× from standard benchmark-game sizes
for faster iteration (see [`benchmarks/REPORT.md`](benchmarks/REPORT.md)
for full-size extrapolations). N-body uses the flat-array version with
`OC_MUTATE_NUM` fused read-modify-write.

| Benchmark | Description | Computation Pattern |
|-----------|-------------|-------------------|
| **spectral-norm** | Matrix eigenvalue via power iteration | O(N²) per iter, float multiply-add |
| **n-body** | Solar system simulation (5 bodies) | Gravity interactions, sqrt per pair |
| **mandelbrot** | Fractal set generation | Per-pixel float iteration (50 max) |
| **fasta** | Random DNA sequence generation | PRNG + table lookup + buffered I/O |

### Current Results (run 2026-05-28, Apple MacBook Air M1)

| Benchmark | Size | C (-O2) | Node.js | Python 3 | **TinyLang** |
|-----------|------|---------|---------|----------|-------------|
| spectral-norm | N=550 | **0.02s** | **0.07s** | 2.00s | **1.94s** |
| n-body | N=500K | **0.04s** | **0.10s** | 9.33s | **4.57s** |
| mandelbrot | 1000×1000 | **0.11s** | **0.15s** | 5.38s | **4.41s** |
| fasta | N=500K | **<0.01s** | **0.36s** | 3.13s | **3.06s** |

**TinyLang beats Python on every benchmark** — 1.03× on spectral-norm,
2.0× on n-body, 1.22× on mandelbrot, and ties on fasta. vs Node.js:
3–46× slower on numeric workloads where V8's JIT shines.

### Optimization Impact on N-Body

| Version | 5M steps (extrapolated) | Speedup |
|---------|------------------------|---------|
| Original (nested arrays) | ~204s | 1.0× |
| + Flat array + compound assign | ~158s | 1.3× |
| + Dedicated numeric opcodes | ~90s | 2.3× |
| **+ OC_MUTATE_NUM (fused mutate)** | **~46s** | **4.4×** |

Three targeted VM changes using compile-time type information brought n-body
from 204s down to ~46s.

---

## Running Benchmarks

```sh
cd benchmarks
./run.sh
```

This compiles and runs all four benchmarks for C, Node.js, Python, and
TinyLang, and reports wall-clock time.

Benchmark source files:
- `benchmarks/tl_src/` — TinyLang versions
- `benchmarks/c_src/` — C versions
- `benchmarks/js_src/` — JavaScript (Node.js) versions
- `benchmarks/py_src/` — Python versions

---

## Implementation

- small C file
- Optional GNU Readline/libedit integration for line editing and history
- Pre-lexed token array → single-pass compiler → flat bytecode (`Instr[]`)
- Stack-based VM: C99 goto-to-switch dispatch, `Value istk[4096]` stack
- 31 opcodes including 4 dedicated numeric opcodes (ADD/SUB/MUL/DIV),
  `OC_MUTATE_NUM` fused read-modify-write, `OC_CLEAR_SLOT` for move semantics,
  and `OC_PROFILE` for profiling
- Slot-indexed variable access: O(1) instead of O(n) strcmp
- Compile-time type tracking: `comp_types[]` parallel to `comp_vars[]`
  with expression-level type inference for dedicated opcode dispatch
- Function return type inference and consistency checking
- Runtime operator fast path: `OC_OP` inlines all 15 operators for num+num
- Per-element array append for single and multi-element literals
- Push-all from any array expression (function, variable, slice, chained `+`)
- Deep copy on assignment, refcount+COW arrays
- Zero-copy slice views: contiguous slices share backing store, no element copy
- In-place slice truncation: `x = x[slice]` mutates directly when exclusive
- Tail call optimization: parameter rebinding + ip reset (no C stack growth)
- Slot initialization at scope creation for array-typed variables
- Array destructuring: `x, y = [1, 2, 3]` — unpack arrays into multiple variables
- Comprehensive test suite (68+ happy-path tests, 24 error tests)

For detailed implementation notes, see [`IMPLEMENTATION.md`](IMPLEMENTATION.md).
For design rationale and language semantics, see [`DESIGN.md`](DESIGN.md).

### Portability Notes

The VM uses a standard C99 **goto-to-switch** dispatch pattern — no GNU
extensions. It compiles cleanly with `-std=c99 -pedantic -Wall` on both Clang
and GCC with zero warnings.

The code uses `strdup()` which is a POSIX function, not part of C99.
macOS, Linux, and BSDs all provide it; strict C99-or-only platforms may not.

Build with:

```sh
cc -std=c99 -Wall -pedantic -O3 -o tiny tinylang.c -lm
```

---

## Grammar

```
program       := top-level statements

statement     := assignment | if_stmt | while_stmt | func_def | ret_stmt | include_stmt | expr_stmt
include_stmt  := "include" include_path
include_path  := string

assignment    := lvalue ("=" | "+=" | "-=" | "*=" | "/=" ) expr
               | destructure
lvalue        := identifier ("[" slice_or_index "]")*
destructure   := identifier "," identifier ("," identifier)* "=" rhs_list
rhs_list      := expr ("," expr)*
if_stmt       := "if" expr block ("elif" expr block)* ("else" block)?
while_stmt    := "for" expr block
func_def      := "fun" identifier "(" params ")" block
params        := param ("," param)*
param         := identifier "=" expr
ret_stmt      := "ret" expr ("," expr)*

block         := "{" stmt_list "}"
expr          := logical_or
logical_or    := logical_and ("||" logical_and)*
logical_and   := bitwise_or ("&&" bitwise_or)*
bitwise_or    := bitwise_xor ("|" bitwise_xor)*
bitwise_xor   := bitwise_and ("^" bitwise_and)*
bitwise_and   := comparison ("&" comparison)*
comparison    := shift (("==" | "!=" | "<" | ">" | "<=" | ">=") shift)?
shift         := addition (("<<" | ">>") addition)*
addition      := multiplication (("+" | "-") multiplication)*
multiplication := primary (("*" | "/" | "%") primary)*

primary       := number | identifier | "nil" | string | array_literal
               | call | index | slice | "(" expr ")" | "!" primary | "-" primary | "#" primary | "~" primary

slice         := primary "[" expr? ":" expr? (":" expr?)? "]"
```

## Design

- [`DESIGN.md`](DESIGN.md) — Language design, types, semantics
- [`IMPLEMENTATION.md`](IMPLEMENTATION.md) — C implementation details, bytecode VM
