# TinyLang

> ✨ Tiny Is Beautiful ✨

TinyLang (pet name Tiny) is a tiny, **statically-typed** programming language
built to explore how simple a language can be while still being practical.
Every design choice prioritises **simple implementation** and 
**easy optimisation** — no runtime type dispatch, no garbage collector,
no closures, no pointers, no AST, no intermediate representations.
Just a single-pass compiler emitting flat bytecode for a stack-based VM,
all in a small single C file.

The central thesis: **static typing and simple semantics are not constraints —
they are enablers.**  Every optimisation in the VM flows directly from a
language decision that kept things simple — no runtime type dispatch, no hash
table lookups, no GC, no intermediate representations.  The result is a
bytecode interpreter that punches well above its weight class.

There are exactly two types: `number` and `array` (strings are just arrays of
numbers), no pointers, references, or aliasing, and arrays use refcount+copy-on-write to
give value semantics without GC.  No closures, no pointers, no first-class
functions, no runtime type checks.  Everything is static.  Everything is simple.
Everything is easy to optimise.

The compiler tracks the type of every variable and every function return value
at compile time. First assignment determines a variable's type permanently;
type changes are detected at compile time, not at runtime. Function return
types are inferred from the body and checked for consistency across all
return statements. Array-type slots are initialized to `[]` at scope-creation
time, eliminating the need for runtime type guards on array operations.

```tinylang
// Everything here compiles to flat bytecode in one pass.
// No AST, no runtime type checks, no GC pauses.

function sum(list) {
    if list == nil { return 0 }
    return list[0] + sum(list[1])      // TCO - inifinte recursion
}

// Lisp-like linked list
print(sum([1, [2, [3, [4, nil]]]]))    // 10

nums = []; i = 0
while i < 100000 {
    nums = nums + [i] // array push with O(1)
    i = i + 1
}
print(#nums)                           // 100000

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
```

A full language in <1,600 lines of C. No required dependencies. Compiles in <1s.

| Language | Lines | Compilation | Dispatch | Variables | GC | AST |
|----------|-------|-------------|----------|-----------|----|------|
| TinyLang | ~1,555 | Single-pass | Computed goto | Slot-indexed O(1) | None | None |
| Python | ~700K+ | Multi-pass | Switch + eval loop | Dict O(log n) | Generational | Full |
| Lua | ~25K | Multi-pass | Switch | Table O(log n) | Incremental | Full |
| JavaScript (V8) | Millions | Multi-tier JIT | Native code | Inline cache | Generational | Full |

## Quick start

```sh
# Without readline (no line editing):
cc -Wall -Wextra -O2 -lm -o tiny tinylang.c
./tiny tests/test.tl

# With optional readline support (line editing, history, arrow keys):
cc -DREADLINE -Wall -Wextra -O2 -lm -o tiny tinylang.c -lreadline
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
./run_tests.sh      # runs all happy-path and error tests
```

- **Happy-path tests** — `tests/test_*.tl` and `tests/bench_*.tl`
- **Error tests** — `tests/e_*.tl` (expected runtime errors)
- **Benchmarks** in `tests/bench_*.tl` (backwards traversal, COW, push optimization, TCO)

All tests must pass before committing.

---

## Language Features Guide

### Types

TinyLang has exactly **two runtime types: `number` and `array`**.

- **`number`** — always a C `double`. Supports integer and floating-point values,
  hex literals, and arithmetic.
- **`array`** — a heterogeneous `Value[]` sequence that can hold numbers and
  sub-arrays freely. The empty array `[]` is the only falsey value and serves
  as `nil`/`null`/`false`.

There is no dedicated boolean, null, string, or pointer type.

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

### Strings (syntactic sugar for byte arrays)

Strings are syntactic sugar for arrays of byte values. There is no string type —
`"abc"` compiles to `[97, 98, 99]`. The `print` function special-cases arrays
whose elements are all printable ASCII by printing them as readable text rather
than `[104, 101, 108, 108, 111]`. `print` does **not** add a trailing newline —
include `\n` in your strings when you want one.

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
function   return   if   elif   else   while   nil   include
```

Built-in function names (`print`, `input`, `thispath`) are **not** keywords.

### Truthiness & Negation

- **Only `[]` (empty array) is falsey.** Zero, empty string, `[[]]`, `[0]` are
  all truthy.
- The canonical truth value is the number `1`.
- `!` is prefix negation: turns truthy to `[]`, `[]` to `1`.

| `x` | `!x` |
|-----|------|
| `[]` | `1` |
| `0` | `[]` |
| `[1,2]` | `[]` |
| `1` | `[]` |
| `""` | `[]` |

```tinylang
print(![])      // 1
print(!0)       // []
print(!1)       // []
print(!5)       // []
print(!!5)      // 1
print(!![])     // []
```

### Operators

#### Arithmetic

| Operator | Operation | Notes |
|----------|-----------|-------|
| `+` | Addition / array concat | Numbers add; arrays concatenate; string + number converts number to string then concats |
| `-` | Subtraction | Requires two numbers |
| `*` | Multiplication / array repeat | Number × number, or array × number |
| `/` | Division | Halts on division by zero |
| `%` | Modulo | Uses `fmod`, halts on zero |
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
| `=` | Equal (assignment in statement context) |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

Array equality is **deep** — element-by-element recursive comparison.

```tinylang
print(5 = 5)          // 1
print(5 = 6)          // []
print(5 < 6)          // 1
print(5 > 3)          // 1
print(3 <= 5)         // 1
print(5 <= 5)         // 1
print([1, 2] = [1, 2]) // 1 (deep equal)
print([1, 2] = [1, 3]) // []
print(5 = "hello")    // [] (mixed type = not equal)
```

> **Note:** `=` means **assignment** when it appears at the start of a statement
> (`x = 5`), but **comparison** everywhere else (`if x = 5 { }`, `print(x = 5)`).
> This is context-determined at compile time.

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

```tinylang
print(-5)             // -5
print(#[])            // 0
print(#nil)           // 0 (nil = [])
print(#"abc")         // 3
print(#[[1,2],[3,4]]) // 2
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
| 5 | `=` `!=` | Equality |
| 1 | `&&` | Logical AND |
| 0 | `\|\|` | Logical OR |

Unary operators (`!` `-` `#`) and indexing `[]` bind tighter than any binary
operator. Parentheses override.

```tinylang
print(5 + 3 * 2)        // 11   (5 + 6)
print((5 + 3) * 2)      // 16   (8 * 2)
print(10 - 4 <= 3 * 2)  // 1    (6 <= 6)
print(5 + 3 > 6 = 4 + 2 > 5) // 1  ((8>6) = (6>5))
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
function foo() {
    x = 20           // creates NEW local variable 'x' in function scope
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
m[i, j] = 42            // multi-index sugar: m[i][j] = 42
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

### Functions

```tinylang
// Simple function
function double(x) {
    return x * 2
}
print(double(5))         // 10

// Multiple parameters
function add(a, b) {
    return a + b
}
print(add(3, 4))         // 7

// No return → returns [] (nil)
function noop() { }
print(#noop())           // 0

// Recursion
function fact(n) {
    if n = 0 { return 1 }
    return n * fact(n - 1)
}
print(fact(5))           // 120

// Tail recursion (TCO — no stack growth)
function fact_tco(n, acc) {
    if n = 0 { return acc }
    return fact_tco(n - 1, n * acc)
}
print(fact_tco(1000, 1)) // inf (no stack overflow)
```

Key rules:
- `function` keyword — defined only at top level
- Define-before-use (no forward references)
- Recursion works; tail calls are optimized (TCO)
- Not first-class — cannot be stored in variables or passed as arguments
- Extra arguments are silently ignored; missing arguments default to `[]`

### Control Flow

#### if/elif/else

```tinylang
function classify(n) {
    if n < 0 {
        return -1
    } elif n = 0 {
        return 0
    } else {
        return 1
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
while i < 5 {
    i = i + 1
}
print(i)                 // 5

// Nested while
i = 0
result = []
while i < 3 {
    j = 0
    while j <= i {
        result = result + [i]
        j = j + 1
    }
    i = i + 1
}
print(result)            // [0, 1, 1, 2, 2, 2]
```

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

#### Array Repetition

```tinylang
zeros = [0] * 10          // 10 zeros
pairs = [1, 2] * 3       // [1, 2, 1, 2, 1, 2]
zero_repeat = [5] * 0    // []
neg_repeat = [5] * (-1)  // []
```

#### Push Optimization

`x = x + [elem]` is detected at compile time and compiled to a `OC_PUSH`
instruction — appends the element directly in amortized O(1) instead of
copying the entire array. Multi-element array literals `[a, b, c]` also
use `OC_PUSH` (one per element), avoiding the temporary array entirely.

The compiler also detects `x = x + expr` where `x` is a known array and
the RHS is any expression yielding an array (function call, variable, slice,
chained `+`, etc.) and emits `OC_PUSH_ALL`. This pushes all elements of the
RHS array into `x` in-place, avoiding the final concatenated copy and
store-back. A runtime fallback handles non-array RHS (e.g., string concat).

```tinylang
// OC_PUSH (O(1) each):
arr = []
i = 0
while i < 1000 {
    arr = arr + [i]
    i = i + 1
}
print(#arr)              // 1000

// Multi-element: also OC_PUSH, one per element
multi = [1]
multi = multi + [2, 3, 4]  // 3 OC_PUSH instructions

// OC_PUSH_ALL — push all from any array expression
arr = arr + fn_that_returns_array()
arr = arr + ([1, 2] + [3, 4]) + another_fn()
```

### Value Semantics (Copy-on-Write)

TinyLang uses **reference counting with copy-on-write** to implement value
semantics. When you copy an array, both variables share the same underlying
data. A deep copy only happens when someone tries to **mutate** shared data.
This preserves value semantics (mutating a copy never affects the original)
while keeping read-only access O(1).

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
```

**Key insight:** Sharing is invisible. There is no "borrow" concept. Every value
always has a valid refcount. When you read `arr[i]`, you get a value whose
refcount was incremented. When the variable holding it goes out of scope, the
refcount is decremented. Everything is automatic and transparent.

### Built-in Functions

#### `print(x)`

Writes `x` to stdout with **no trailing newline**. Numbers print in decimal.
Arrays whose elements are all printable ASCII are printed as text strings rather
than `[104, 101, ...]`. Use `print("\n")` or embed `\n` in your strings to
produce newlines — the user is in full control of line breaks.

```tinylang
print(42)                // 42 (no newline)
print(3.14)              // 3.14 (no newline)
print("hello")           // hello (no newline)
print("hello\n")         // hello (with trailing newline)
print("line1\nline2\n")  // multi-line output
print([1, 2, 3])         // [1, 2, 3]
```

#### `input()`

Reads a line from stdin, returns as a byte array (string).

```tinylang
name = input()
print(name)
```

#### `thispath()`

Returns the source file path where the call appears, as a byte array. Inside
`include` expressions, `thispath()` returns the directory of the current file
(with trailing `/`), so concatenation with a relative path resolves to the
correct location.

```tinylang
print(thispath())        // e.g., /Users/mimi/project/test.tl
```

### Include System

Load and execute another TinyLang source file at compile time.

#### String literal include

```tinylang
// main.tl
include "lib/utils.tl"
print(greet("world"))

// lib/utils.tl
function greet(name) {
    return "hello " + name
}
```

File resolution is relative to the including file's directory.

#### Expression include

The include path can also be a compile-time expression using `thispath()`
and `+` concatenation. Inside include expressions, `thispath()` returns
the directory of the current file (with trailing `/`), so concatenating
a relative path resolves to the correct sibling file.

```tinylang
include thispath() + "utils.tl"
include thispath() + "../lib/helpers.tl"
include thispath() + "sub" + "/nested.tl"
```

Only `thispath()`, string literals, and `+` are supported — the expression
is evaluated entirely at compile time.

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

### Tail Call Optimization (TCO)

When a function ends with `return f(args...)` where `f` is the function itself,
the compiler replaces the call with `OC_TCO` — a direct instruction-pointer reset
that rebinds parameters without allocating a new C stack frame.

```tinylang
// Tail-recursive: never overflows the C stack
function countdown(n) {
    if n = 0 { return 0 }
    return countdown(n - 1)   // TCO
}

// NOT tail-recursive: still uses C stack
function broken(n) {
    if n = 0 { return 0 }
    return 1 + broken(n - 1)  // needs to multiply after return
}
```

TCO detection happens at compile time by scanning the last instructions of each
function body for `OC_CALL` to the same function in tail position.

### nil

`nil` is syntactic sugar for the empty array `[]`.

```tinylang
n = nil
print(n = [])            // 1
print(n != [])           // []
print(!n)                // 1
```

Truth table for `nil`: it is falsey, negates to `1`, and is deeply equal to `[]`.

### Multi-Index Sugar

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

### Dynamic Index Chain

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

### `include` Directive

`include "path"` loads and compiles another TinyLang source file in place.
Paths are relative to the including file's directory.

```tinylang
include "helpers.tl"
include "../lib/math.tl"
```

The include path can also be a compile-time expression:

```tinylang
include thispath() + "helpers.tl"
include thispath() + "../lib/math.tl"
```

When using an expression, `thispath()` returns the directory of the current
file, so concatenation resolves relative paths correctly. The expression is
evaluated at compile time — only `thispath()`, string literals, and `+`
concatenation are supported.

---

## Optimizations & VM Internals

TinyLang is a bytecode interpreter with a deliberately minimal implementation
(~1,190 lines of C). Despite its simplicity, it incorporates several
optimizations that dramatically improve performance over a naive interpreter.

Because every variable has exactly one type and every function has exactly one
scope, the compiler can:

- **Resolve variables to integer slots** at compile time — O(1) runtime access
  with zero string comparisons.
- **Inline array append** into a single `OC_PUSH` opcode — the compiler
  recognises `x = x + [e]` and emits an O(1) amortised append.
- **Detect tail calls** by scanning the last few emitted instructions — no
  separate analysis pass needed.
- **Dispatch opcodes via computed goto** — the VM's jump table is a flat array
  of label addresses, compiled once and shared across all executions.
- **Pre-allocate scopes** with the exact number of variables known at compile
  time — no hash tables, no dynamic growth.

### 1. Computed Goto Dispatch (Threaded Code)

The VM's main execution loop uses **computed goto** (GNU C extension `&&`
address-of-label and indirect `goto *ptr`) instead of a `while` + `switch` loop.

```c
// Instead of:
while (1) {
    switch (c->code[ip].op) {
        case OC_NUM: /* ... */ break;
        case OC_VAR: /* ... */ break;
    }
    ip++;
}

// TinyLang uses computed goto:
void exec(Code *c) {
    static void *dispatch[] = {
        [OC_NUM] = &&op_num,
        [OC_VAR] = &&op_var,
        // ... one label per opcode
    };
    int ip = 0;
    goto *dispatch[c->code[ip].op];

op_num:
    // ... handler ...
    ip++; goto *dispatch[c->code[ip].op];

op_var:
    // ... handler ...
    ip++; goto *dispatch[c->code[ip].op];
}
```

**Why it's faster:** A switch-based interpreter does 3 jumps per bytecode
(dispatch → switch → handler → back to while check → dispatch). Computed goto
does 1 jump — direct handler-to-handler. This yields **~15% speedup** across
all workloads.

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
variable-heavy loops. Combined with computed goto, the total speedup over a
naive switch+strcmp VM is **55–85% across benchmarks.**

### 3. Compile-Time Type Tracking

The compiler maintains a parallel `comp_types[]` array alongside `comp_vars[]`,
tracking whether each variable holds a number (`T_NUM_TYPE`) or an array
(`T_ARR_TYPE`). A `peek_expr_type()` function walks the token stream ahead
of compilation to infer expression types from literals, variables, function
calls, and binary operators.

Key properties:
- **First assignment determines type.** `set_var_type()` records the type on
  first assignment; any subsequent attempt to assign a different type halts
  at compile time with `"type mismatch"`.
- **Function return types are checked.** Each `return expr` infers its type;
  all returns in a function must agree. Functions with no return get
  `ret_type = T_ARR_TYPE` (they return `[]`).
- **Indexed expressions are unknown.** `arr[i]` returns `T_UNKNOWN` — element
  types are not tracked at compile time.
- **Binary operator inference.** `num + num = num`; anything else with `+` =
  array; `arr * num = array`; all other ops produce numbers. Unknown
  operands produce unknown results.

The type information drives the push optimization decisions and enables
slot initialization at scope creation time.

### 4. Push Optimization (`OC_PUSH` / `OC_PUSH_ALL`)

The compiler detects `x = x + [...]` by lookahead on the token stream.
Single and multi-element bracket literals each get their own `OC_PUSH`
instruction — no temporary array is allocated.

For non-literal RHS expressions (function calls, variables, slices, chained
`+`), the compiler emits `OC_PUSH_ALL`. This reads the RHS array from the
stack and pushes all its elements into `x` in-place. A runtime fallback
handles non-array RHS for edge cases like string concatenation.

```tinylang
// OC_PUSH — per-element append:
arr = arr + [i]          // single element
arr = arr + [1, 2, 3]    // multi-element: 3 OC_PUSH instructions

// OC_PUSH_ALL — push all from any array expression:
arr = arr + fn_that_returns_array()
arr = arr + ([1, 2] + [3, 4]) + another_fn()
```

**Why it matters:** Normal array concatenation (`OC_OP` with `T_PL`) allocates
a new array, copies all elements from both sides, then decrements the source
references — O(n) time and O(n) memory. `OC_PUSH` appends directly with
amortized O(1) — it calls `amake_uniq` (COW if shared), doubles capacity on
realloc, and writes one element.

`OC_PUSH_ALL` eliminates the final concatenated copy and store-back.
For `x = x + a + b + c`, the naive path creates 3 temporary arrays and one
store-back copy; push-all creates only 1 temporary (from the RHS expression
tree) and zero store-back copies.

Both optimizations are guarded by compile-time type tracking — they only fire
when `x` is known to be an array.

### 5. Copy-on-Write (COW) Arrays

Arrays use reference counting with copy-on-write to implement value semantics
without unnecessary copying:

```c
void amake_uniq(Value *v) {
    if (v->type != VAL_ARR || !v->arr) return;
    if (v->arr->refcount > 1) {
        Arr *old = v->arr;
        v->arr = adeep_copy(old);    // deep copy only when shared
        arelease(old);
    }
}
```

**Key behavior:**
- **Read-sharing is free** — reading `arr[i]` on a shared array increments the
  refcount, no copy.
- **Mutation triggers copy** — writing `arr[i] = x` calls `amake_uniq`, which
  deep-copies the array only if `refcount > 1`.
- **Nested COW** — mutating a sub-array deep-copies only that sub-array, not
  the entire tree.

This avoids GC pauses (deterministic cleanup) while keeping memory usage low.
In benchmarks, TinyLang uses **3–12× less memory than Node.js** for the same
computation.

### 6. Tail Call Optimization (TCO)

Detected at compile time: if the last instruction before `OC_END` (or `OC_RET`)
is a call to the same function, the compiler mutates it to `OC_TCO`.

```c
case OC_TCO:
    // 1. Pop arguments from stack
    // 2. Rebind parameters by slot index (same scope, no allocation)
    // 3. ip = 0; restart function body — no C stack growth!
    break;
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

### Cumulative Optimization Impact

| Optimization | Speedup vs Naive | Description |
|-------------|-----------------|-------------|
| Computed goto dispatch | ~15% | 1 jump/bytecode vs 3 (switch) |
| Slot-indexed variables | ~50% on var access | O(1) array index vs O(n) strcmp |
| Compile-time type tracking | Enables all below | Static types eliminate runtime dispatch |
| Push optimization | O(n²)→O(n) on array builds | Amortized O(1) append vs full copy |
| Push-all (`OC_PUSH_ALL`) | Eliminates final copy+store | In-place mutation for any RHS array expr |
| COW sharing | Variable, workload-dependent | Zero-copy reads, copy only on write |
| Slot initialization | Eliminates runtime guards | Array slots pre-initialized to `[]` |
| Single-pass compiler | ~0 (constant factor) | No AST allocation overhead |
| Combined (dispatch + slots + types) | **55–85%** | Across all benchmarks |

---

## Performance Comparisons: TinyLang vs C vs Node.js vs Python

We ported four benchmarks from the [Computer Language Benchmarks
Game](https://benchmarksgame-team.pages.debian.net/benchmarksgame/) to
TinyLang, C, Node.js (V8), and Python (CPython) and compared their
performance on an Apple MacBook Air M1 (16GB, macOS 15.7.5).

| Benchmark | Description | Computation Pattern |
|-----------|-------------|-------------------|
| **spectral-norm** | Matrix eigenvalue via power iteration | O(N²) per iter, float multiply-add |
| **n-body** | Solar system simulation (5 bodies) | Gravity interactions, sqrt per pair |
| **mandelbrot** | Fractal set generation | Per-pixel float iteration (50 max) |
| **fasta** | Random DNA sequence generation | PRNG + table lookup + buffered I/O |

### Full-Size Results

At the standard benchmark-game sizes (where TinyLang is too slow to complete in
reasonable time, Python included for reference):

| Benchmark | Size | C (-O2) | Node.js | Python 3 | TinyLang |
|-----------|------|---------|---------|----------|----------|
| spectral-norm | N=5500 | **2.49s** | **1.92s** | 212.70s | N/A |
| n-body | N=5M | **0.41s** | **0.52s** | 93.10s | N/A |
| mandelbrot | 200×200 | **<0.01s** | **0.05s** | 0.47s | **0.26s** |
| fasta | N=25000 | **<0.01s** | **0.06s** | 0.31s | **0.15s** |

> **Note:** Node.js is faster than C on these workloads because V8's
> JIT compiler inlines tiny functions, vectorizes loops via ARM NEON, and
> uses typed arrays (`Float64Array`) for near-native memory access.
> Python is 77–158× slower than C at full sizes.
> TinyLang is too slow for the full N=5500/5M problem sizes.

### Matching-Size Results (Fair Comparison)

To compare TinyLang fairly, we reduced the problem sizes so TinyLang completes
in seconds:

| Benchmark | Size | C (-O2) | Node.js | Python 3 | TinyLang |
|-----------|------|---------|---------|----------|----------|
| spectral-norm | N=100 | **<0.01s** | **0.05s** | 0.35s | **0.07s** |
| n-body | N=5000 | **<0.01s** | **0.05s** | 0.18s | **0.19s** |
| mandelbrot | 200×200 | **<0.01s** | **0.05s** | 0.47s | **0.26s** |
| fasta | N=25000 | **<0.01s** | **0.06s** | 0.31s | **0.15s** |

All results shown as **real (wall-clock) time** — lower is better:

```
                  faster ◄──────────────────────────────────────────► slower

spectral-norm N=100 :
  C       █ 0.00s
  JS      ████████████████████ 0.05s
  Python  ███████████████████████████████████████████████████████████████████ 0.35s
  TL      █████████████████████████████████████████████████████████████████████████ 0.19s

n-body N=5000 :
  C       █ 0.00s
  JS      ██████████████████ 0.05s
  Python  ██████████████████████████████████████████████████████████████ 0.18s
  TL      ██████████████████████████████████████████████████████████████████████████████████████████ 0.45s

mandelbrot 200×200 :
  JS      ████████████████████ 0.05s
  C       ███████████████████████████████████████████████████████ 0.18s
  Python  ████████████████████████████████████████████████████████████████████████████████████████ 0.47s
  TL      ██████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████ 1.58s

fasta N=25000 :
  JS      █████████████████████████ 0.07s
  C       ███████████████████████████████████████████████████████ 0.16s
  Python  ████████████████████████████████████████████████████████████████████████████████████ 0.31s
  TL      ████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████████ 0.57s
```

### Ratio Summary

| Comparison | Spectral-Norm | N-Body | Mandelbrot | Fasta |
|-----------|:-----------:|:------:|:----------:|:----:|
| **Python / TinyLang** | 1.8× faster | 2.5× faster | 3.4× faster | 1.8× faster |
| **TinyLang / Node.js** | 3.8× slower | 9.0× slower | 31.6× slower | 8.1× slower |
| **C / TinyLang** | ~190× faster | ~760× faster | 8.8× faster | 3.6× faster |

**Key takeaway:** TinyLang is **~2× faster than CPython** on the mandelbrot and
fasta benchmarks (at matching sizes), but **~2× slower** on spectral-norm and
n-body where it lacks hardware `sqrt` and uses a 100-iteration Newton-Raphson
approximation. It is **4–31× slower than Node.js** and **4–760× slower than C**
depending on workload locality and dispatch overhead.

### Root-Cause Analysis

#### Why TinyLang is ~2× faster than Python on some workloads

| Factor | TinyLang | CPython |
|--------|----------|---------|
| Dispatch | Computed goto (1 jump/bytecode) | Switch-based (3 jumps/bytecode) |
| Variables | Slot-indexed O(1) | Dict lookup O(log n) |
| Number ops | Direct `double` in register | Object boxing/unboxing per op |
| Arrays | Contiguous `Value[]` | Object array (PyObject*) |
| Loops | `OC_JZ`/`OC_JMP` (tight) | Heavy iterator protocol |

Python's overhead comes from its dynamic type system (every operation checks
type at runtime), object boxing (every `int` is a full `PyObject`), and
hash-table variable lookups. TinyLang's static types and slot-indexed variables
avoid all of this.

#### Why TinyLang is 4–31× slower than Node.js

| Factor | Approx Impact | Details |
|--------|---------------|---------|
| **Interpreted vs JIT** | 10–50× | TinyLang: bytecode dispatch. Node: native code via V8's TurboFan JIT |
| **Array overhead** | 3–10× | TinyLang: refcount+COW, type dispatch per element. Node: TypedArray or monomorphic inline caches |
| **No hardware sqrt** | 2–5× | TinyLang: 100-iteration Newton-Raphson. Node/V8: `Math.sqrt` → single `fsqrt` instruction |
| **Function call cost** | 3–6× | TinyLang: stack save/restore + scope creation. Node/V8: inlined after warmup |
| **Memory model** | 2–4× | TinyLang: heap-allocated arrays with COW copies. Node: generational GC, object pooling |

#### Why TinyLang is 4–760× slower than C

| Factor | Approx Impact | Details |
|--------|---------------|---------|
| **Dispatch overhead** | 10–100× | C: native instructions. TinyLang: 5–15 bytecodes per source op |
| **No inlining** | 2–10× | C: function inlining, loop unrolling, SIMD vectorization |
| **Memory indirection** | 2–5× | TinyLang: heap-allocated `Arr` structs. C: stack-allocated locals |
| **No hardware sqrt** | 2–5× | Newton-Raphson iteration vs single `fsqrt` |

The C version is especially faster on tight loops (n-body, spectral-norm)
because the entire computation stays in registers and the CPU executes 4+ float
operations per cycle. TinyLang's interpreter spends most of its time in dispatch
and memory indirection.

### Where TinyLang Excels

| Property | TinyLang | Node.js | Python | C |
|----------|----------|---------|--------|---|
| **Memory efficiency** | 1.2–5.7 MB | 14–17 MB | 25–40 MB | 1–2 MB |
| **Startup time** | ~2ms | ~40ms | ~30ms | ~0ms |
| **Implementation size** | ~1,190 lines | Millions | Millions | ~100–150/bench |
| **Deterministic cleanup** | ✓ Refcount | ✗ GC pauses | ✗ GC pauses | ✓ Manual |
| **No dependencies** | ✓ Single .c | ✗ Node runtime | ✗ Python runtime | ✓ |

TinyLang uses **3–12× less memory** than Node.js for the same computation,
because its compact `Value` structs and refcount-based cleanup don't need
generational GC overhead. Startup time is ~2ms vs Node.js's ~40ms V8
initialization. And the entire implementation fits in a single ~1,190 line C
file that compiles in under a second.

---

## Running Benchmarks

```sh
cd benchmarks
./run.sh
```

This compiles and runs all four benchmarks for C, Node.js, Python, and
TinyLang, and reports wall-clock time. See [`benchmarks/REPORT.md`](benchmarks/REPORT.md)
for the full analysis.

Benchmark source files:
- `benchmarks/tl_src/` — TinyLang versions
- `benchmarks/c_src/` — C versions
- `benchmarks/js_src/` — JavaScript (Node.js) versions
- `benchmarks/py_src/` — Python versions

---

## Implementation

- ~1,555 lines of C, single file
- Optional GNU Readline/libedit integration for line editing and history
- Pre-lexed token array → single-pass compiler → flat bytecode (`Instr[]`)
- Stack-based VM: computed goto dispatch, `Value istk[4096]` stack
- Slot-indexed variable access: O(1) instead of O(n) strcmp
- Compile-time type tracking: `comp_types[]` parallel to `comp_vars[]`
- Function return type inference and consistency checking
- `OC_PUSH` for per-element array append (single and multi-element literals)
- `OC_PUSH_ALL` for push-all from any array expression (function, variable, slice, chained `+`)
- Deep copy on assignment, refcount+COW arrays
- Tail call optimization: parameter rebinding + ip reset (no C stack growth)
- Slot initialization at scope creation for array-typed variables
- Comprehensive test suite (30+ happy-path tests, 20 error tests)

For detailed implementation notes, see [`IMPLEMENTATION.md`](IMPLEMENTATION.md).
For design rationale and language semantics, see [`DESIGN.md`](DESIGN.md).

### Portability Notes

The VM uses **computed goto dispatch** (address-of-label `&&label` and indirect
`goto *ptr`) for its main execution loop — a GNU C extension not in C99.
The entire dispatch is driven by a jump table (`dispatch[]`) filled with
label addresses, and each opcode handler ends with an indirect goto. This
makes the code fast (no costly switch/jump chains), but ties it to GCC and
Clang — it will not compile with MSVC, ICC, or strict C99-only compilers.

The code also uses `strdup()` which is a POSIX function, not part of C99.
macOS, Linux, and BSDs all provide it; strict C99-or-only platforms may not.

To check for these and other non-standard extensions at build time:

```sh
cc -std=c99 -Wall -pedantic -o tiny tinylang.c -lm
```

This will flag the GNU label-as-value and indirect-goto extensions as
warnings (50+ of them). They are expected and intentional.

---

## Grammar

```
program       := top-level statements

statement     := assignment | if_stmt | while_stmt | func_def | ret_stmt | include_stmt | expr_stmt
include_stmt  := "include" include_path
include_path  := string | include_expr
include_expr  := thispath "(" ")" ("+" string)*

assignment    := lvalue ("=" | "+=" | "-=" | "*=" | "/=" ) expr
lvalue        := identifier ("[" slice_or_index "]")*
if_stmt       := "if" expr block ("elif" expr block)* ("else" block)?
while_stmt    := "while" expr block
func_def      := "function" identifier "(" params ")" block
ret_stmt      := "return" expr

block         := "{" stmt_list "}"
expr          := logical_or
logical_or    := logical_and ("||" logical_and)*
logical_and   := comparison ("&&" comparison)*
comparison    := shift (("==" | "!=" | "<" | ">" | "<=" | ">=") shift)?
shift         := addition (("<<" | ">>") addition)*
addition      := multiplication (("+" | "-") multiplication)*
multiplication := primary (("*" | "/" | "%") primary)*

primary       := number | identifier | "nil" | string | array_literal
               | call | index | slice | "(" expr ")" | "!" primary | "-" primary | "#" primary

slice         := primary "[" expr? ":" expr? (":" expr?)? "]"
```

## Design

- [`DESIGN.md`](DESIGN.md) — Language design, types, semantics
- [`IMPLEMENTATION.md`](IMPLEMENTATION.md) — C implementation details, bytecode VM
