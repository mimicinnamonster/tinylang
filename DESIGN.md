# TinyLang — Design Notes

---

## 1. Type System

- **Three runtime types:** `number` (all floats conceptually), `array`, and `ptr` (FFI)
- No null/bool type — `[]` (empty array) serves as nil/false/absence
- `nil` is syntactic sugar for `[]`
- First assignment determines variable type permanently
- `x = 0; x = "hello"` → type error, halts
- Numbers stored as C `double` internally, with int optimization when no fractional part
- Arrays are **heterogeneous** — can hold numbers, strings, sub-arrays freely
- `ptr` holds C `void*` pointers — exclusively for FFI handles from `dlopen`/`dlsym`
- Array elements accessed by `arr[idx]` — 0-indexed, bounds checked, halts on OOB

---

## 2. Value Semantics — Reference Counting + Copy-on-Write

Arrays are shared via reference counting. A deep copy only happens when someone tries to **mutate** shared data. This preserves value semantics (mutating a copy never affects the original) while keeping read-only access O(1).

### Internal structure

```c
typedef enum {
    NK_U8, NK_U16, NK_U32, NK_U64,
    NK_I8, NK_I16, NK_I32, NK_I64,
    NK_F32, NK_F64
} NumKind;

typedef enum {
    ARR_U8, ARR_U16, ARR_U32, ARR_U64,
    ARR_I8, ARR_I16, ARR_I32, ARR_I64,
    ARR_F32, ARR_F64, ARR_VAL
} ArrKind;

typedef struct Arr {
    int refcount, len, cap;
    ArrKind kind;
    union {
        struct Value *val;   // ARR_VAL: heterogeneous Value[]
        uint8_t *u8;         // ARR_U8:  raw uint8_t[]
        int16_t *i16;        // ARR_I16: raw int16_t[]
        float *f32;          // ARR_F32: raw float[]
        double *f64;         // ARR_F64: raw double[]
        /* ... all 11 pointer types */
    } as;
} Arr;

typedef struct Value {
    Type type;           // VAL_NUM, VAL_ARR, or VAL_PTR
    NumKind nkind;       // storage kind (meaningful for VAL_NUM)
    union {
        uint8_t u8; int8_t i8;      // compact number storage
        uint16_t u16; int16_t i16;
        uint32_t u32; int32_t i32;
        uint64_t u64; int64_t i64;
        float f32; double f64;
        Arr *arr;                    // array pointer
        void *ptr;                   // FFI pointer
    } as;
} Value;  // 16 bytes (same as before)
```

The `nkind` field fits in what was padding — the struct stays 16 bytes.
`vnum()` detects the narrowest kind on creation, `val_num()` widens to
`double` on every read through the VM and `apply()` operators.

Numbers own no heap memory — they're inline in the Value union.
Arrays point to a shared `Arr` with a refcount.

### Compact Array Promotion

Arrays are created with a `kind` determined by `detect_kind()` at runtime:
- All non-negative integers → unsigned: U8 → U16 → U32 → U64
- Any negative integers → signed: I8 → I16 → I32 → I64
- Non-integer numbers → F32 if exact, else F64
- Non-numeric → ARR_VAL (heterogeneous)

Concatenation (`+`) and repetition (`*`) propagate and promote types
via `promote_kind()`, which finds the smallest type that can exactly
represent both inputs:

| Left | Right | Result | Why |
|------|-------|--------|-----|
| U8 `[200]` | I8 `[-50]` | **I16** | I8 can't hold 200, U8 can't hold -50 |
| U16 `[50000]` | I8 | **I32** | I16 max 32767 < 50000 |
| I32 `[2e9]` | F32 `[1.5]` | **F64** | F32's 24-bit mantissa can't hold 2e9 exactly |
| F32 | F64 | **F64** | Wider float wins |
| U64 | I64 | **VAL** | Neither fits in the other |

On mutation (`arr[i] = val`), compact arrays are promoted to ARR_VAL in
place via `amake_uniq()` — the compact layout is a read-only optimization.
This keeps the write path simple while making iteration and copying
maximally compact.

### Inspecting storage kinds with `type()`

The built-in `type(x)` function exposes the internal storage kind for
testing and debugging:

```tl
// Numbers: nkind (0-9)
type(5)              // 0  = NK_U8   (1 byte)
type(300)            // 1  = NK_U16  (2 bytes)
type(5000000000)     // 3  = NK_U64  (8 bytes)
type(-100)           // 4  = NK_I8
type(-30000)         // 5  = NK_I16
type(1.5)            // 8  = NK_F32  (4 bytes)
type(3.14159265358979) // 9 = NK_F64 (8 bytes)

// Arrays: 100 + ArrKind
type([1, 2, 3])          // 100 = ARR_U8
type([-100, 0])          // 104 = ARR_I8
type([1.5, 2.5])         // 108 = ARR_F32
type([1, "hello"])       // 110 = ARR_VAL (heterogeneous)

// nil / empty
type([])             // -1

// Mutation promotes to VAL
x = [1, 2, 3]
type(x)              // 100 = ARR_U8
x[0] = 99
type(x)              // 110 = ARR_VAL (promoted on write)
```

### Rules

| Operation | What happens | Cost |
|-----------|-------------|:---:|
| `y = x` (both arrays) | `y.data = x.data`; `x.data->refcount++` | **O(1)** |
| `arr[i]` (reads element) | Returns the Value at `items[i]`; if array, `result.data->refcount++` | **O(1)** |
| `x = arr[i]` (store element) | `arr[i]` shares, `=` captures it with the refcount already incremented | **O(1)** |
| `return arr[i]` (return element) | Transfers shared reference to caller, refcount already incremented by `arr[i]` | **O(1)** |
| `foo(arr[i])` (pass element) | Argument shares, refcount already incremented by `arr[i]` | **O(1)** |
| `arr[i] = val` (mutate element) | If `arr.data->refcount > 1`: **deep copy** `arr.data` first. Then write `val` into the private copy. | **O(N)** only when shared |
| variable goes out of scope | `data->refcount--`; if 0, free `data` and all its sub-arrays recursively | **O(depth)** only on last release |

### Key insight: sharing is invisible

There is no "borrow" concept. Every Value always has a valid refcount. When you read `arr[i]`, you get a Value whose refcount was incremented. When the variable holding it goes out of scope, the refcount is decremented. Everything is automatic and transparent.

A deep copy only happens when mutation would break value semantics — never on reads.

### Example

```
matrix = [[1, 2], [3, 4]]

// Read: just increments refcounts silently
val = matrix[0][1]       // matrix[0] → share sub-array, refcount++
                          // [1] → share number, refcount trivial (no heap)
                          // = captures val, matrix released at scope end
                          // All O(1), no deep copies

// Share: matrix and row point to the same data
row = matrix[0]           // share: refcount of [1,2] is now 2
                          // O(1)

// COW: row tries to mutate shared data
row[0] = 99               // row.data->refcount == 2 > 1 → deep copy [1,2]
                          // row now points to its own private copy
                          // original [1,2]'s refcount → 1 (matrix still holds it)
                          // row's private copy [99, 2], matrix's copy unchanged
print(matrix[0][0])       // still 1

// Mutation via lvalue chain on unshared data
matrix[0][1] = 99         // matrix.data->refcount == 1 (row was COW'd away)
                          // matrix[0]'s refcount == 1 → no copy needed
                          // write 99 in-place
print(matrix[0][1])       // 99
```

### What this means for nested linked lists

```
// Deep copy model: O(N²)
current = list
while current != nil {
    current = current[1]   // each iteration deep-copies remaining tail
}

// Refcount+COW model: O(N)
current = list             // share, refcount++
while current != nil {
    current = current[1]   // share the tail, refcount++
                           // release old current, refcount--
                           // all O(1), no heap allocs
}
```

The O(N²) problem is eliminated entirely. Linked list traversal is always O(N), same as any other pattern.

### Copy-on-write in detail

COW ensures value semantics are preserved while keeping reads cheap:

```
x = arr              // share: refcount = 2
x[0] = 5             // x.data->refcount > 1 → deep copy x's data
                      // new data has refcount = 1
                      // old data refcount-- → 1 (arr still holds it)
                      // now x's private copy[0] = 5

// arr is unaffected
print(arr[0])        // original value
```

Mutation of a sub-array also triggers COW at that level:

```
x = matrix[0]        // share sub-array [1,2], refcount = 2
x[0] = 99            // sub-array's refcount > 1 → deep copy sub-array
                      // x[0] refers to x's private copy now
                      // matrix[0]'s original sub-array unchanged
```

### Recursive release on last deref

When a variable goes out of scope and holds the last reference to an array (`refcount` hits 0), freeing the array also decrements refcounts on its elements. If any of those hit 0, they're freed too, and so on. This replaces GC — deterministic cleanup with no cycle detection.

```c
void array_release(struct ArrayData *data) {
    data->refcount--;
    if (data->refcount > 0) return;
    for (int i = 0; i < data->len; i++) {
        if (data->items[i].type == VAL_ARR)
            array_release(data->items[i].as.data);
    }
    free(data->items);  // or free(data) if items is flexible
    free(data);
}
```

---

## 3. Truthiness & Negation

- **Only `[]` is falsey** — `0`, `""`, `[0]`, `[[]]` are all truthy
- The **canonical truth value** is the number `1`
- `!` is unary negation — turns truthy to `[]`, `[]` to `1`:

| `x` | `!x` |
|-----|------|
| `[]` | `1` |
| `0` | `[]` |
| `[1,2]` | `[]` |
| `1` | `[]` |

- Check for nil: `x == nil` or `x == []` (same thing)
- `if !x {}` enters block only when `x` is `[]`

---

## 4. No Nested `{}` Scopes

- `{ }` is purely for grouping statements in `if`/`while`/function bodies
- No block-level lexical scoping — variables declared in a block are in the enclosing function or top-level scope

---

## 5. Scope — Fully Local

- `x = anything` is **always local** to the current function or top-level script
- Functions **never** access variables from outside their own scope — no closure, no globals
- Each function call gets a fresh, empty scope with only its parameters and local assignments
- Functions can only see: their parameters + variables assigned within their body

### Function Table

- Function names live in a **global function table**, separate from variable scopes
- Calling `foo(...)` looks up `foo` in the function table, not in variable scope
- Functions can call other functions by name but cannot read variables from outside
- Variable and function names are in different namespaces — `x = 5` (variable) and `function x() {}` (function) do not conflict

### Implication: Pure-Function-By-Design

All data flows through arguments and return values:

```
name = "world"
function greet(n) {
    print("hello ")
    print(n)
}
greet(name)
```

No shared mutable state:

```
function counter(n) {
    n = n + 1
    return n
}
print(counter(0))  // 1
print(counter(0))  // 1 (independent)
```

---

## 6. Operators

### Assignment (statement level only)

```
assignment := lvalue "=" expr
lvalue     := identifier ("[" expr "]")*
```

- `x = 5` — variable assignment
- `arr[0] = 5` — array element mutation (in-place, no copy)
- `matrix[i][j] = 5` — nested mutation via lvalue chain

`=` at the start of a statement (after lvalue chain) is **assignment**.
`=` anywhere else is **equality comparison**.

### Math (binary, infix)

`+`  `-`  `*`  `/`  `%`

`+` on two arrays = **concatenation** (returns new deep-copied array).
`+` on two numbers = numeric addition.
`*` on array × number = **repetition** (returns new array with elements repeated `n` times).
`+` / `*` on mixed types = type error.

**Push optimization:** `x = x + [expr]` (same variable, single-element literal)
is compiled to `OC_PUSH` — the element is appended directly to `x`'s array
in O(1) instead of copying the entire array. This enables O(N) sequential
appends:

```
arr = []
i = 0
while i < 1000 {
  arr = arr + [i]    // O(1) per iteration with OC_PUSH
  i = i + 1
}
```

`y = x + [1]` (different variable) falls through to the general
concatenation path, preserving value semantics.

### Bitwise (binary, infix)

`&`  `|`  `^`  `@`

- `@` shift: `(x @ 4)` = shift **left** 4 bits, `(x @ -4)` = shift **right** 4 bits
- Shift amount clamped to [0, 63]
- Bitwise ops require integer values (no fractional part) — truncates toward zero if fractional, runtime type error on arrays

### Comparison (binary, infix)

| Op | Meaning | Returns |
|----|---------|---------|
| `=`  | equal | `1` or `[]` |
| `!=` | not equal | `1` or `[]` |
| `<`  | less than | `1` or `[]` |
| `>`  | greater than | `1` or `[]` |

- Array equality is **deep** — element-by-element recursive comparison. `[] = []` returns `1`.
- Mixed-type comparison (`5 = "hello"`) returns `[]` (not equal), not an error.

### Unary

- `!`  negation (prefix): `[]` → `1`, anything else → `[]`
- `-`  numeric negation (prefix): `-5` negates 5
- `#`  array length (prefix): `#arr` returns number of elements in `arr`, type error on numbers
### Context determines `=` meaning

| Position | What `=` means |
|----------|---------------|
| `x = 5` at statement start | **assignment** |
| `arr[0] = 5` lvalue chain | **assignment** |
| `if x = 5 { }` | **comparison** |
| `print(x = 5)` inside `()` | **comparison** |
| `[x = 5]` inside `[]` | **comparison** |
| `y = x = 5` right-side `=` | **comparison** (result assigned to `y`) |

### Modulo / Remainder

`%` follows C/`fmod` semantics:
- `5 % 2` → `1`
- `(-5) % 2` → `-1`
- `5.5 % 2.0` → `1.5`

---

## 7. Binary Ops and Parens — Unified Rule

**A single binary operation `expr op expr` is valid at any expression level without `()`.**
**Two or more binary ops in sequence require `()` for disambiguation.**

```
x = x + y           // valid: one binary op
x = x + y * z       // ERROR: two binary ops in sequence
x = (x + y) * z     // valid
x = x + (y * z)     // valid

print(x + y)        // valid
print(x + y * z)    // ERROR
print(x + (y * z))  // valid

if x + y { }        // valid
if x + y * z { }    // ERROR
if x + (y * z) { }  // valid

arr = [x + y]       // valid
arr = [x + y * z]   // ERROR
```

### Gotcha: `y = x = 5` is not chained assignment

```
y = x = 5           // y = (x = 5) → comparison right side
                    // y gets 1 (if x == 5) or [] (if x != 5)
                    // x is NOT assigned 5!
```

Use two statements:

```
x = 5
y = 5
```

---

## 8. Control Flow

```
if condition {
    ...
}

if condition {
    ...
} elif condition {
    ...
} elif condition {
    ...
} else {
    ...
}

while condition {
    ...
}
```

- `if`/`while` conditions **do not require explicit `()`** — implicitly wrapped
- `{ }` on bodies are **always required**
- All conditions use `[]`-is-falsy rule: `while 1 {}` loops forever
- `elif` chains arbitrary length; `else` is optional

---

## 9. Functions

```
function name(params) {
    statements
}
```

- `function` keyword
- `return expr` exits and returns `expr`
- No `return` → returns `[]`
- **Arity checking**: too few args → extra params bound to `[]`. Too many → extras ignored.
- `return` outside a function → parse error
- Defined only at top level (no nested functions)
- Define-before-use (no forward references)
- Parameters bound in function's local scope
- **Not first-class** — cannot be stored in variables or passed as arguments
- **Not closures** — no access to outer variable scopes
- Recursion works (self-call via function table lookup)

---

## 10. Arrays

```
empty = []
nums  = [1, 2, 3]
matrix = [[1, 2], [3, 4]]
mixed = [1, "hello", [], [5, 6]]
```

- Fixed size determined at first assignment
- Out-of-bounds access → error, halts. Negative indices are out of bounds (no wrap-around).
- Index: `arr[idx]` (0-indexed)
- Multi-index: `arr[i, j, k]` → `arr[i][j][k]` — chain via comma-separated indices
- Dynamic chain: `arr[idx_arr]` where `idx_arr = [i, j, k]` → `arr[i][j][k]` — same as multi-index, but chain depth is runtime
- Mutation: `arr[idx] = val` or `arr[i, j] = val` (lvalue chain)
- Nested mutation: `matrix[i][j] = val` (lvalue chain, no intermediate copies)
- **Any expression that evaluates to an array can be indexed** — not just variables:
  ```
  [1, 2, 3][0]               // array literal indexing → 1
  fn_returning_arr()[i]      // function call result indexing
  (arr)[i]                   // parenthesized expression indexing
  "hello"[0]                 // string literal indexing → 104
  (arr1 + arr2)[i]           // concatenation result indexing
  ([0] * 10)[i]              // repetition result indexing
  arr[1:3][0]                // chained slice then index
  ```
- **Heterogeneous** — elements can be any type, mixed freely
- Strings are syntactic sugar: `"abc"` ≡ `[97, 98, 99]`
- Escape sequences: `\n` (10), `\t` (9), `\\` (92), `\"` (34), `\xHH` (arbitrary byte)

### Multi-index syntax: `arr[i, j, k]`

Comma-separated indices inside `[]` desugar to a chained index:

```
arr[1, 2, 3]           // = arr[[1, 2, 3]] = arr[1][2][3]
arr[ptr, 0]            // = arr[[ptr, 0]] = arr[ptr][0]
arr[idx_arr]           // = arr[[i, j, k]] = arr[i][j][k]  if idx_arr = [i,j,k]
```

**Static chain (comma-separated):** `arr[i, j, k]` — the chain depth is known at parse time (3 levels). The parser emits a chain of index operations directly.

**Dynamic chain (array index):** `arr[idx_arr]` — the chain depth depends on `idx_arr`'s length at runtime. Works the same way: the interpreter iterates over `idx_arr`'s elements and walks the chain. Also works in lvalue context for assignment:

```
indices = [2, 1]
arr[indices] = 99         // arr[[2, 1]] → arr[2][1] = 99
```

This is especially useful for the manual heap pattern:

```
nodes = [[0, -1]] * 100
ptr = 0
while ptr != -1 {
    nodes[ptr, 0] = nodes[ptr, 0] * 2   // nodes[ptr][0]
    ptr = nodes[ptr, 1]                  // nodes[ptr][1]
}
```

### Slice syntax: `arr[i:j]`, `arr[i:j:k]`

Colons inside `[]` denote a **slice** — creating a new array from a
contiguous or strided range. The syntax follows Python semantics:

| Expression | Meaning |
|------------|---------|
| `arr[start:stop]` | Elements from `start` (inclusive) to `stop` (exclusive) |
| `arr[start:stop:step` | Elements from `start` to `stop` with step `step` |
| `arr[:stop]` | From beginning to `stop` |
| `arr[start:]` | From `start` to end |
| `arr[:]` | Full copy of array |
| `arr[::step]` | Full array with step |
| `arr[::-1]` | Reverse array |

**Slice rules:**
- **Negative indices** count from the end: `-1` is the last element, `-2` second-to-last, etc.
- **Omitted bounds** default to `0` (start) and `len(arr)` (stop) for positive step, or `len-1` (start) and `-1` (stop) for negative step.
- **Out-of-bounds** values are clamped to the array bounds — never an error.
- **Step must be non-zero** — a zero step halts with a runtime error.
- The result is a **new array** with the same backing kind as the source (compact kinds preserved).

**Disambiguation with multi-index:**
A `:` at the top level inside `[...]` triggers slice mode. Commas still denote
multi-index chains. They are mutually exclusive per bracket group:

```
arr[1:5]       // slice: elements 1 through 4
arr[1, 2]      // multi-index: arr[1][2]
arr[1:5, 2]    // error: `,` and `:` conflict in same bracket group
```

### Slice assignment optimization: `x = x[start:stop:step]`

Similar to the push optimization (`x = x + [elem]`), the compiler detects the
pattern `x = x[slice]` (same variable on both sides) and emits the specialised
`OC_SLICE_ASSIGN` opcode instead of `OC_SLICE + OC_STORE`. This enables
two optimisations:

1. **Exclusive ownership + step=1:** The array is modified in-place. Truncation
   (`x = x[:n]`) is O(1) — it just adjusts `len`. Shift (`x = x[n:]` or
   `x = x[n:m]`) uses `memmove` to shift elements, avoiding allocation.
2. **Shared or step≠1:** Falls through to a copy — the result is a new array,
   assigned via `vassign`, preserving value semantics.

This makes heavily slice-oriented code efficient without sacrificing
correctness:

```
arr = build_large_array()    // some big list
arr = arr[:len(arr)/2]       // O(1) — just truncate
y = arr                      // share
arr = arr[:10]               // O(1) in-place (exclusive after y was created? No,
                              // y shares → copy path triggered)
print(#y)                    // still len/2 — y's copy unaffected
```

### Compact array backing stores

When an array literal contains only numbers of a uniform type, the VM
selects a compact backing store instead of `Value[]`. This is transparent
— all reads return numbers as usual — but memory drops 2–16× and iteration
is cache-friendly.

| Literal | Backing store | Bytes vs `Value[]` |
|---------|--------------|:------------------:|
| `[1, 2, 3]` | `int8_t[3]` | 48 → 3 (**16×**) |
| `[1000, 2000]` | `int16_t[2]` | 32 → 4 (**8×**) |
| `[70000, 80000]` | `uint32_t[2]` | 32 → 8 (**4×**) |
| `[1.5, 2.5]` | `float[2]` | 32 → 8 (**4×**) |
| `[1, "hello"]` | `Value[2]` | 32 → 32 (no savings) |

Promotion follows a one-way chain **I8 → I32 → F64 → VAL** during
mutation. Reading from a compact array wraps elements into Values on
the fly (`arr_item()` helper) — the overhead of constructing a Value
is negligible compared to the cache win from dense storage.

### Array creation: repetition with `*`

```
zeros = [0] * 100     // array of 100 zeros
nils  = [] * 100      // array of 100 nils
chars = ["x"] * 5     // array of 5 strings
pairs = [1, 2] * 3    // -> [1, 2, 1, 2, 1, 2]
```

`*` on array × number = repeat the array that many times. Zero or negative count returns empty `[]`. Fractional truncates toward zero.
`*` on number × array is a type error — write `[val] * n`, not `n * [val]`.

This enables pre-allocation for the manual heap pattern:

```
nodes = [[0, -1]] * 100   // 100 node slots, each [value, next]
head = -1                 // empty list

function push(value) {
    head = head + 1
    nodes[head][0] = value
    nodes[head][1] = head
}
```

No new syntax needed — `[0] * 5` is already `primary * primary` in the grammar. The `*` operator is simply defined for (array × number) in addition to (number × number).

---

## 11. Comments

```
// line comments — // to end-of-line is ignored
```

---

## 12. Statement Separation (Go-style + Explicit Semicolons)

- Newlines act as statement separators (Go-style inference)
- Explicit `;` may also be used to separate statements on the same line
- A newline is NOT a separator if the last token before it is an operator or opening `(`, `[`, `{`
- Consecutive blank lines are fine
- Multiple consecutive `;` are treated as empty statements (no-ops)

```
x = 5
y = 10
print(x + y)

z = (x +            // x continues: + expects more
     y)

// Multiple statements on one line via ;
a = 10; b = 20; print(a + b)

// Trailing ; is fine (empty statement)
print(x);

// Multiple ; are fine
x = 1;;;y = 2
```

### Expression statements vs REPL auto-print

In **script mode**, expression statements are evaluated for side effect and the result discarded:

```
x + y               // valid: evaluates and discards
print(5)            // valid: calls print, discards return value
42                  // valid: literal expression, no-op
```

In the **REPL**, a bare expression statement automatically prints its value instead of discarding it:

```
> 5
5
> x + 3
42
> "hello"
hello
> [1, 2, 3]
[1, 2, 3]
> nil
[]
```

This makes the REPL feel like a calculator — type an expression, see its result. Assignments (`x = 5`) produce no output. The built-in `print()` function works as expected and does not cause double-printing.

---

## 13. Literals & Identifiers

### Numbers

```
5            // integer
5.0          // float
.5           // 0.5
5.           // 5.0
0xff         // hex: 255
0xFF         // hex: 255 (case-insensitive)
0xdeadbeef   // hex: 3735928559
0b101        // binary: 5
0B11111111   // binary: 255 (case-insensitive prefix)
0123         // octal: 83 (leading zero)
0777         // octal: 511
00           // octal: 0
```

- **Hex literals:** `0x` or `0X` prefix, followed by one or more hex digits (`0-9`, `a-f`, `A-F`)
- **Binary literals:** `0b` or `0B` prefix, followed by one or more binary digits (`0`, `1`)
- **Octal literals:** Leading `0` followed by one or more octal digits (`0-7`). `0` followed by `8` or `9` is an error. A bare `0` is decimal.
- All non-decimal literals produce unsigned integer values (stored in the narrowest `NumKind` that fits).
- Invalid characters after a non-decimal literal prefix (e.g., `0xG`, `0b2`, `0b12`) produce a compile error.

### Identifiers

```
foo
foo_bar
_private
abc123
```

Regex: `[a-zA-Z_][a-zA-Z0-9_]*`

---

## 14. Reserved Keywords

These are reserved as token types — cannot be used as variable or parameter names:

```
function   return   if   elif   else   while   nil
```

Built-in function names (`print`, `input`) are NOT keywords — they live in the function table. You CAN name a variable `print` without conflicting with the function `print()` (different namespaces).

---

## 15. Error Handling

Runtime errors halt execution of the current input with a message to stderr. In script mode, the process exits with status 1. In the REPL, errors are caught and the REPL continues with the next input, preserving the current scope.

Every runtime error prints the error message followed by the call chain, each frame prefixed with `in `:

```
'+' type mismatch
in script.tl:3
in script.tl:5: foo()
in script.tl:2: bar()
```

In the REPL, file paths are omitted and the default `<top-level>` label is shown instead:

```
'+' type mismatch
in <top-level>
in foo()
in bar()
```

No recovery, no try/catch — except for the built-in `assert()` function (see §17), which wraps a single expression in an error-catching context and returns the error message as a string instead of halting.

### REPL auto-print

When a bare expression is used as a statement in the REPL, the VM emits `OC_PRINT` instead of `OC_POP`. This is controlled by a compile-time check on `comp_file` — when `comp_file` is `NULL` (REPL mode), bare expression statements compile to `OC_PRINT`; in script mode (`comp_file` set), they compile to `OC_POP`. The `OC_PRINT` handler safely no-ops on an empty stack, so calls to the built-in `print()` function leave nothing extra to print.

| Error | Message |
|-------|---------|
| Division by zero | `error: division by zero` |
| Modulo by zero | `error: modulo by zero` |
| Array index OOB | `error: index X out of bounds for array of length Y` (negative is always OOB) |
| Type mismatch on assignment | `error: cannot assign <type> to variable 'x' (declared as <type>)` |
| Type mismatch on operation | `error: cannot apply '+' to number and array` |
| Undefined variable | `error: undefined variable 'x'` |
| Undefined function | `error: undefined function 'foo'` |
| Bitwise on non-integer | `error: bitwise operation on non-integer value` |
| `return` outside function | `error: return outside function` |

---

## 16. Grammar

```
program       := top-level statements

statement     := assignment
               | if_stmt
               | while_stmt
               | func_def
               | ret_stmt
               | expr_stmt

assignment    := lvalue "=" expr

lvalue        := identifier ("[" index_list "]")*

if_stmt       := "if" expr block ("elif" expr block)* ("else" block)?
while_stmt    := "while" expr block
func_def      := "function" identifier "(" params ")" block
ret_stmt      := "return" expr
expr_stmt     := expr

block         := "{" stmt_list "}"
stmt_list     := (statement newline+)*

expr          := primary_index
               | primary_index op primary_index       // exactly one binary op

primary       := number_literal
               | identifier
               | "nil"
               | string_literal
               | "[" "]"                              // empty array
               | "[" expr ("," expr)* "]"             // array literal
               | identifier "(" args ")"              // function call
               | "(" expr ")"                         // grouping
               | "!" primary                          // negation
               | "-" primary                          // unary minus
               | "#" primary                          // array length

primary_index := primary ("[" index_list "]")*       // any primary followed by indexing or slicing

op            := "+" | "-" | "*" | "/" | "%"
               | "&" | "|" | "^" | "@"
               | "=" | "!=" | "<" | ">"

index_list    := expr ("," expr)*                    // comma-separated indices: arr[i,j,k]
               | expr? ":" expr? (":" expr?)?        // slice: arr[i:j] or arr[i:j:k]

params        := /* empty */ | identifier ("," identifier)*
args          := /* empty */ | expr ("," expr)*
```

Note: `lvalue` (see §6) uses `identifier "[" index_list "]"` for assignment targets.
The parser distinguishes lvalue from rvalue by context: if `identifier [...]` is followed by `=`, it's an lvalue; otherwise it's an rvalue on the primary.

### Expression indexing examples

Any expression that produces an array can be followed by `[...]` for indexing or slicing:

```
[1, 2, 3][0]               // literal index → 1
fn()[i]                    // function call index
(arr)[i]                   // parenthesized index
"hello"[0]                 // string literal index → 104
([1,2] + [3,4])[i]         // binary op result index
([0] * 10)[i]              // repetition result index
[1, 2, 3, 4][1:3]          // slice on literal → [2, 3]
get_vals()[0:2]            // slice on function result

// Chained indexing works as expected
[1, [2, 3]][1][0]          // → 2
fn_grid()[i, j]            // multi-index on function result
arr[1:3][0]                // slice then index
```

*Note: lvalue chains (`arr[i] = val`) are restricted to identifiers — only variable-based arrays can be mutated, not expression results.*

---

## 17. Built-in Functions

| Name | Signature | Semantics |
|------|-----------|-----------|
| `print` | `print(x)` | Writes `x` to stdout. Numbers: decimal (no `.0` if integer). Arrays: `[e1, e2, ...]`. Empty array: `[]`. Strings (printed as text, not byte arrays). |
| `input` | `input()` | Reads a line from stdin, returns as byte array (string) |
| `assert` | `assert(expr)` | Evaluates `expr` in an error-catching context. If evaluation succeeds and the result is truthy, returns `[]` (nil). If evaluation succeeds but the result is falsy, returns `"assertion failed"`. If evaluation produces a runtime error, the error is caught and the actual error message is returned as a string. |
| `type` | `type(x)` | Returns a number representing the internal storage kind of `x`. For numbers: 0=U8, 1=U16, 2=U32, 3=U64, 4=I8, 5=I16, 6=I32, 7=I64, 8=F32, 9=F64. For arrays: 100+ArrKind (0=U8…10=VAL). For nil/`[]`: -1. For ptr: -2. Useful for testing that the compact type system selected the expected backing store. |
| `thispath` | `thispath()` | Returns the source file path where the call appears, as a byte array (string). In the REPL, returns `[]` (nil). Works correctly across `include`d files — each call returns that file's own path. Useful for diagnostics and locating resources relative to the script. |

`print` special-cases arrays whose elements are all printable ASCII or common control characters (10, 13, 9) — these are printed as the text string rather than `[104, 101, ...]`.

## 18. FFI (Optional, requires libffi)

When built with `-DTL_FFI`, four extra built-in functions are registered via
the CReg system (`tl_register()`), enabling dynamic loading and calling of C
functions at runtime.

| Function | Description |
|----------|-------------|
| `dlopen(path)` | Load a shared library, returns `ptr` handle (or `[]` on failure) |
| `dlsym(handle, name)` | Look up a symbol in a loaded library, returns `ptr` (or `[]` on failure) |
| `dlclose(handle)` | Unload a library, returns `[]` |
| `ffi_call(fn, sig, ...)` | Call a C function pointer with libffi type marshalling |

`ffi_call` expects:
1. `fn` — a `ptr` from `dlsym`
2. `sig` — signature string (first char = return type, rest = arg types)
3. One value per character in the signature (excluding return type)

Signature characters:
- `v` — `void` (return only, not valid for args)
- `i` — `int` (marshals from/to TL number)
- `d` — `double` (marshals from/to TL number)
- `p` — `void*` (marshals from/to TL ptr)
- `s` — `const char*` (marshals TL string to NUL-terminated C string)

### Real-world FFI example — SDL2 window

See `tests/ffi_sdl_window.tl` for a complete working example that creates
an actual desktop window using the SDL2 library:

```
lib = dlopen("/opt/homebrew/lib/libSDL2-2.0.0.dylib")

sdl_init = dlsym(lib, "SDL_Init")
result = ffi_call(sdl_init, "ii", 32)       // SDL_INIT_VIDEO = 0x20
assert(result = 0)

sdl_create_window = dlsym(lib, "SDL_CreateWindow")
window = ffi_call(sdl_create_window, "psiiiii", "Hello", 200, 200, 640, 480, 0)
// sig: p=return ptr, s=title, i=x, i=y, i=w, i=h, i=flags

sdl_pump = dlsym(lib, "SDL_PumpEvents")
sdl_delay = dlsym(lib, "SDL_Delay")

i = 0
while i < 10 {
    ffi_call(sdl_pump, "v")        // keep window responsive
    ffi_call(sdl_delay, "vi", 500)  // wait 500ms
    i = i + 1
}

sdl_destroy_window = dlsym(lib, "SDL_DestroyWindow")
ffi_call(sdl_destroy_window, "vp", window)

sdl_quit = dlsym(lib, "SDL_Quit")
ffi_call(sdl_quit, "v")            // just "v" — no args, void return
```

Key observations from real-world FFI usage:
- The `!= nil` check compares types, not pointer values — a `VAL_PTR` null
  and `VAL_ARR` nil are always "not equal" because their types differ, so
  this is not a reliable null check for pointers.
- macOS windowing requires `SDL_PumpEvents()` to be called regularly for the
  window to remain interactive.
- Strings (`s` sig) are automatically marshalled from TL byte arrays to
  NUL-terminated C strings and freed after the call.
- Use `SDL_GetWindowID()` (sig `"ip"`) to verify a window pointer — returns
  0 for null/invalid, positive integer otherwise.

---

## 19. JIT-Ability Summary

Properties that make a future vectorizing JIT simpler than typical dynamic languages:

| Property | How the language provides it |
|----------|------------------------------|
| No aliasing | Refcount+COW guarantees isolation — JIT knows a refcount of 1 = exclusive ownership, safe for in-place mutation |
| Fixed variable types | First assignment locks type — no polymorphic guards |
| Pure functions | No global access — side-effect analysis is trivial |
| Monomorphic call sites | No first-class functions — every call targets one definition |
| Shallow expressions | No chaining without `()` — IR is small and local |
| `[]` = nil | Single concrete sentinel — one null check, cheap |
| Pre-allocation | `[0] * n` produces a compact `int8_t[]` backing store — known-size flat buffer, exclusive ownership, no type tags per element. JIT sees raw C arrays directly. |
| No closures | Scope is flat per function — no captured environment |
| Simple CFG | Only `if`/`elif`/`else`/`while` — no switch, goto, exceptions |
| Error = halt | JIT can speculate without having to recover on error — just deopt to interpreter |
