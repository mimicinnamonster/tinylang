# TinyLang — Design Notes

---

## 1. Type System

- **Three types:** `number` (double), `array` (Value[]), and `string` (byte array
  with a compile-time marker, implemented as an array under the hood)
- No null/bool type — `[]` (empty array) serves as nil/false/absence
- `nil` is syntactic sugar for `[]`
- No `ptr` type — FFI has been removed
- First assignment determines variable type permanently
- `x = 0; x = "hello"` → compile error: `"type mismatch"`
- Compiler tracks types in `comp_types[]` — first-assigned type per variable
- Function return types inferred and checked across all `return` statements
- `peek_expr_type()` infers expression types at compile time (literals,
  variables, function calls, binary operators)
- Indexed expressions (`arr[i]`) are `T_UNKNOWN` — element types not tracked
- Numbers are always C `double` (no compact type optimizations)
- Arrays are **heterogeneous** — can hold numbers and sub-arrays freely
- Array elements accessed by `arr[idx]` — 0-indexed, bounds checked, halts on OOB

---

## 2. Value Semantics — Reference Counting + Copy-on-Write

Arrays are shared via reference counting. A deep copy only happens when someone
tries to **mutate** shared data. This preserves value semantics (mutating a copy
never affects the original) while keeping read-only access O(1).

### Internal structure

```c
typedef struct Arr {
    int refcount, len, cap;
    struct Value *val;     // always Value[], no compact backing stores
} Arr;

typedef struct Value {
    Type type;             // VAL_NUM or VAL_ARR
    double num;            // VAL_NUM: always double
    Arr *arr;              // VAL_ARR
} Value;                   // 24 bytes
```

### Key insight: sharing is invisible

There is no "borrow" concept. Every Value always has a valid refcount. When you
read `arr[i]`, you get a Value whose refcount was incremented. When the variable
holding it goes out of scope, the refcount is decremented. Everything is
automatic and transparent.

A deep copy only happens when mutation would break value semantics — never on
reads.

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

## 4. Scoping

- Variables are always local to the current function or top-level scope
- Functions never access variables from outside their own scope — no closures,
  no globals
- Each function call gets a fresh scope with pre-allocated slots
- Functions can only see: their parameters + variables assigned within their body
- Parameter binding is O(1) via slot index, not O(n) via name lookup
- Variable reads/writes inside function bodies are O(1) slot-indexed operations

### Function Table

- Function names live in a global function table, separate from variable scopes
- Variable and function names are in different namespaces

---

## 5. Operators

### Assignment (statement level only)

```
assignment := lvalue ("=" | "+=" | "-=" | "*=" | "/=" ) expr
lvalue     := identifier ("[" expr "]")*
```

- `x = 5` — variable assignment
- `arr[0] = 5` — array element mutation (in-place via COW)
- `matrix[i][j] = 5` — nested mutation via lvalue chain
- `x += 5` — compound assignment: desugars to `x = x + 5`
- `arr += [elem]` — push optimization: same O(1) append as `arr = arr + [elem]`

### Compound Assignment

`+=`, `-=`, `*=`, `/=` are syntactic sugar that desugars at compile time into
the equivalent simple assignment. No new bytecodes or VM changes are needed —
the compiler emits the same instruction sequence as `x = x op y`.

**How it works:** All compound assignments (`+=`, `-=`, `*=`, `/=`) desugar by
rewriting the token stream at compile time. The compound operator token is
replaced with `=`, and the variable name (plus any index brackets) is copied
after it, followed by the arithmetic operator. The result is identical to the
user having written `x = x op expr` — the exact same parser, push optimization,
and bytecode are used.

For example, `a[i] += 10` rewrites the token stream from:
```
T_ID("a") T_LB T_EXPR T_RB T_PL_ASSIGN T_NUM(10)
```
to:
```
T_ID("a") T_LB T_EXPR T_RB T_ASSIGN T_ID("a") T_LB T_EXPR T_RB T_PL T_NUM(10)
```
which is parsed identically to `a[i] = a[i] + 10`.

**Push optimization works with compound assignment too.** `arr += [elem]`
rewrites to `arr = arr + [elem]`, triggering the same O(1) push as the
explicit form. Multi-element `arr += [a, b]` also rewrites identically, so
it performs array concatenation (not push).

**Index expressions are evaluated twice** — once for the store index (LHS)
and once for the read (RHS via `a[i] + expr`). This is the same double
evaluation that occurs when writing `a[i] = a[i] + expr` explicitly. For
simple constant indices like `0` or `i` this is invisible; for complex
index expressions with side effects (e.g. function calls), the side effect
occurs twice.

### Arithmetic

`+` `-` `*` `/` `%` `<<` `>>`

- `+` on two arrays = **concatenation** (returns new array)
- `+` on a string and a number = **string concatenation** — converts the number to its
  decimal string representation, then concatenates as string + string. Only works when
  the array side contains printable ASCII bytes (the same heuristic `print()` uses to
  display arrays as text). Generic arrays with non-printable bytes produce a type error.
  Examples: `"hello" + 42` → `"hello42"`, `99 + " bottles"` → `"99 bottles"`
- `+` on two numbers = numeric addition
- `*` on array × number = **repetition**
- `<<` left shift: `1 << 8` → `256` (integer bit shift on doubles)
- `>>` right shift: `256 >> 4` → `16`
- Push optimization: `x = x + [expr]` compiles to `OC_PUSH` — appends directly
  in O(1) instead of copying the entire array

### Comparison

| Op | Meaning | Returns |
|----|---------|---------|
| `=` | equal | `1` or `[]` |
| `!=` | not equal | `1` or `[]` |
| `<` | less than | `1` or `[]` |
| `>` | greater than | `1` or `[]` |

- Array equality is **deep** — element-by-element recursive comparison
- Mixed-type comparison (`5 = "hello"`) returns `[]`, not an error

### Logical (short-circuit)

`&&` `||`

- `&&` evaluates left; if falsy, returns it without evaluating right
- `||` evaluates left; if truthy, returns it without evaluating right
- Compiled to OC_DUP + OC_JZ/JNZ + OC_POP pattern

### Unary

- `!` negation (prefix): `[]` → `1`, anything else → `[]`
- `-` numeric negation (prefix): `-5` negates 5
- `#` array length (prefix): `#arr` returns element count, type error on numbers

### Context determines `=` meaning

| Position | What `=` means |
|----------|---------------|
| `x = 5` at statement start | **assignment** |
| `x, y = [1, 2]` comma-separated idents | **destructure assignment** |
| `arr[0] = 5` lvalue chain | **assignment** |
| `if x = 5 { }` | **comparison** |
| `print(x = 5)` inside `()` | **comparison** |
| `y = x = 5` right-side `=` | **comparison** (result assigned to `y`) |

---

## 6. Operator Precedence

Binary operators chain with standard precedence (highest to lowest,
left-associative):

| Level | Operators | Category |
|-------|-----------|----------|
| 9 | `*` `/` `%` | Multiplicative |
| 8 | `+` `-` | Additive |
| 7 | `<<` `>>` | Shift |
| 6 | `<` `>` `<=` `>=` | Relational |
| 5 | `=` `!=` | Equality |
| 1 | `&&` | Logical AND |
| 0 | `\|\|` | Logical OR |

Unary operators (`!` `-` `#`) and array indexing `[]` bind tighter than any
binary operator.

---

## 7. Control Flow

```
if condition { ... } elif condition { ... } else { ... }
while condition { ... }
```

- `{}` on bodies are always required
- All conditions use `[]`-is-falsy rule

---

## 8. Functions

```
function name(param=value, ...) { statements }
```

- `function` keyword
- `return expr` exits and returns `expr`; `return e1, e2, e3` is sugar for
  `return [e1, e2, e3]` (implicit array); no return → returns `[]`
- Defined only at top level (no nested functions)
- Define-before-use (no forward references)
- Recursion works; tail calls are optimized (TCO) — no stack growth for
  `return f(...)` in tail position
- Not first-class — cannot be stored in variables or passed as arguments
- **Every parameter must have a default value.** This ensures all parameter
  types are known at compile time. Missing arguments use the default value
  instead. Defaults are compile-time constants — numbers, strings, `nil`,
  or array literals with constant elements:

  ```tinylang
  function add(a=0, b=10) { return a + b }
  add(5)       // 15  (b=10 from default)
  add()        // 0   (a=0, b=10 both from defaults)

  function first(arr=[10, 20, 30]) { return arr[0] }
  first()      // 10

  // Error: parameter without default
  // function bad(a=5, b) { }  // compile-time error
  ```

---

## 9. Arrays

```
empty = []
nums  = [1, 2, 3]
matrix = [[1, 2], [3, 4]]
mixed = [1, "hello", [], [5, 6]]
```

- Fixed size determined at first assignment
- Out-of-bounds access → error, halts
- Multi-index: `arr[i, j, k]` → `arr[i][j][k]`
- Dynamic chain: `arr[idx_arr]` where `idx_arr = [i, j, k]`
- Strings are sugar: `"abc"` ≡ byte array `[97, 98, 99]`
- Escape sequences: `\n` (10), `\t` (9), `\\` (92), `\"` (34), `\xHH`
- Slicing: `arr[start:stop:step]` with Python semantics (negative indices,
  omitted bounds, clamping)

### Array creation: repetition with `*`

```
zeros = [0] * 100     // array of 100 zeros
pairs = [1, 2] * 3    // [1, 2, 1, 2, 1, 2]
```

### Push optimization

`x = x + [expr]` is detected at compile time. For bracket literals, each
element gets its own `OC_PUSH` — appends in O(1) amortized instead of O(n)
copy. Multi-element literals `[a, b, c]` emit one `OC_PUSH` per element,
avoiding the temporary array entirely.

For non-literal RHS expressions (function calls, variables, slices, chained
`+`), the compiler emits `OC_PUSH_ALL` — reads the RHS array from the stack
and pushes all its elements in-place, avoiding the final concatenated copy
and store-back. A runtime fallback handles non-array RHS for string concat.

Both optimizations are guarded by compile-time type tracking — they only
fire when the target variable is known to be an array.

### Slice in-place optimization

`x = x[slice]` is similarly detected at compile time (same lookahead pattern
as push detection) and emits a single fused opcode `OC_SLICE_INPLACE` instead
of the normal variable-read + slice + store-back sequence.

At runtime:
- **Exclusive ownership + numbers + step==1:** The array is mutated in-place
  via `memmove` + trim. No allocation, no view chaining.
- **Shared array, sub-arrays, or step!=1:** Falls back to creating a zero-copy
  view (or full copy for strided slices).

This prevents view chain accumulation in loops like `while i < n { x = x[1:] }`
when `x` is an all-number array.

---

## 9.5. Hashmaps (String-Keyed Indexing)

Any array can be used as a hashmap by indexing with a string key: `arr["key"]`.
The string's hash determines the array index, computed using the **FNV-1a**
algorithm (same as Git's `strhash`):

```c
unsigned int fnv1a_hash(Arr *a) {
    unsigned int hash = 0x811c9dc5u;  // FNV32_BASE
    for (int i = 0; i < a->len; i++) {
        unsigned int c = (unsigned int)val_num(a->val[i]);
        hash = (hash * 0x01000193u) ^ c;  // FNV32_PRIME
    }
    return hash;
}
```

The index is `hash(key) % len(array)`. This is deterministic — the same string
always maps to the same slot for a given array size.

### Buckets

The array acts as a fixed-size hash table. Each slot is a **bucket**:

```tinylang
// Array of 100 buckets for hashing
map = [[]] * 100

// These may collide (map to the same bucket)
map["foo"] = "first"
map["bar"] = "second"  // overwrites if same bucket as "foo"
```

On collision, the value is simply **overwritten** — the last write wins:

```tinylang
arr = [0, 0, 0]
arr["hello"] = 111
arr["world"] = 222   // overwrites if same bucket
print(arr[1])         // whichever key hashed to slot 1
```

### Collision-Safe Append Pattern

To store multiple values per key without overwriting on collision:

```tinylang
// Initialize: each bucket is an empty array
map = [[]] * 100

// Append to the bucket — arr[key] += [value] desugars to
// arr[key] = arr[key] + [value], which pushes into the bucket's array
map["foo"] += ["first_value"]
map["foo"] += ["second_value"]
map["bar"] += ["other"]

// Even if "foo" and "bar" collide to the same bucket, the values
// are accumulated in that bucket's array:
print(map[bucket])  // ["first_value", "second_value", "other"]
```

Key points:
- `arr["key"] += [val]` uses the same push optimization as `x = x + [elem]`
- If two keys collide, their values simply share the same bucket array
- Without `+=`, colliding keys silently overwrite — use `+=` when you need
  collision-safe storage

### Hash Caching

The FNV-1a hash is computed **once per unique string value** and cached on the
underlying `Arr` struct. Subsequent accesses with the same string (whether
literal or variable) use the cached hash in O(1):

```tinylang
key = "expensive"
while i < 10000 {
    print(map[key])   // hash computed once, cached for 9999 iterations
    i = i + 1
}
```

### Distinction from Numeric Indexing

String-keyed access is detected **at runtime**: if the index value is an array
whose bytes are all printable ASCII (a "string"), it uses hash-based indexing.
Otherwise, it uses the existing chain-of-numeric-indices behavior:

```tinylang
arr["abc"]     // hash-based (string: printable ASCII)
arr[[0, 1]]    // chain-based (numbers 0,1 — non-printable)
arr[[10, 20]]  // chain-based (10=newline, 20=not printable → chain)
```

This means `arr[[104, 101, 108, 108, 111]]` ("hello" as raw bytes) is also
treated as a hash key, since all bytes are printable. For dynamic index chains,
always use literal or constructed arrays of numbers.

### Supported Syntax

```tinylang
// Read
val = arr["key"]

// Write
arr["key"] = val

// Multi-index (chained hash)
arr["a", "b"]        // arr[hash("a")][hash("b")]
arr["a"]["b"]        // same, chained brackets

// Compound assignment (collision-safe append)
arr["key"] += [val]   // push val into bucket
arr["key"] -= [val]   // arr[key] = arr[key] - [val]
arr["key"] *= n       // arr[key] = arr[key] * n

// Variable as key
k = input()
arr[k] = val
```

---

## 10. Literals & Identifiers

### Numbers

```
5            // integer
5.0          // float
.5           // 0.5
5.           // 5.0
0xFF         // hex: 255 (0x prefix, case-insensitive)
```

- Hex literals: `0x` or `0X` prefix
- Binary (`0b`) and octal (`0o`/leading-zero) literals are not supported

### Identifiers

```
foo  foo_bar  _private  abc123
```

Regex: `[a-zA-Z_][a-zA-Z0-9_]*`

### Reserved Keywords

```
function   return   if   elif   else   while   nil   include
```

Built-in function names (`print`, `input`) are NOT keywords.

---

## 11. Include System

Load and execute another TinyLang source file at compile time. The include
path can be a string literal or an expression that evaluates at compile time.

### String literal include

```tinylang
include "lib/utils.tl"
```

Paths are resolved relative to the including file's directory.

### Expression include

```tinylang
include thispath() + "utils.tl"
```

`thispath()` inside an `include` expression returns the directory of the
current file (with trailing `/`), so concatenating with a filename includes
sibling files. The expression is evaluated at compile time — only `thispath()`,
string literals, and `+` concatenation are supported.

Nested includes work arbitrarily deep.

---

## 12. Error Handling

Runtime errors halt execution with a message to stderr and a stack trace.
In script mode the process exits with status 1. In the REPL, errors are caught
and the REPL continues with the next input, preserving the current scope.

No recovery, no try/catch, no assert().

---

## 13. Built-in Functions

| Name | Signature | Description |
|------|-----------|-------------|
| `print` | `print(x)` | Writes `x` to stdout (no trailing newline). Numbers: decimal. Strings: as text. Arrays: `[e1, e2, ...]` |
| `input` | `input()` | Reads a line from stdin, returns as byte array (string) |
| `thispath` | `thispath()` | Returns the source file path where the call appears. Inside `include` expressions, returns the directory of the current file |

`print` special-cases arrays whose elements are all printable ASCII — they are
printed as text strings rather than `[104, 101, ...]`. `print` does **not** add
a trailing newline — include `\n` in your strings to get one.

---

## 14. Grammar

```
program       := top-level statements

statement     := assignment | if_stmt | while_stmt | func_def | ret_stmt
               | include_stmt | expr_stmt
include_stmt  := "include" include_path
include_path  := string | include_expr
include_expr  := thispath "(" ")" ("+" string)*

assignment    := lvalue ("=" | "+=" | "-=" | "*=" | "/=" ) expr
               | destructure
lvalue        := identifier ("[" expr "]")*
destructure   := identifier "," identifier ("," identifier)* "=" rhs_list
rhs_list      := expr ("," expr)*

if_stmt       := "if" expr block ("elif" expr block)* ("else" block)?
while_stmt    := "while" expr block
func_def      := "function" identifier "(" params ")" block
ret_stmt      := "return" expr ("," expr)*
expr_stmt     := expr

block         := "{" stmt_list "}"
expr          := logical_or

logical_or    := logical_and ("||" logical_and)*
logical_and   := comparison ("&&" comparison)*
comparison    := shift (("==" | "!=" | "<" | ">" | "<=" | ">=") shift)?
shift         := addition (("<<" | ">>") addition)*
addition      := multiplication (("+" | "-") multiplication)*
multiplication := primary (("*" | "/" | "%") primary)*

primary       := number_literal | identifier | "nil" | string_literal
               | "[" expr ("," expr)* "]" | call | "(" expr ")"
               | "!" primary | "-" primary | "#" primary

call          := identifier "(" args ")"
index         := primary "[" expr "]"
slice         := primary "[" expr? ":" expr? (":" expr?)? "]"

params        := /* empty */ | param ("," param)*
param         := identifier "=" expr
args          := /* empty */ | expr ("," expr)*
```

---

## 15. Performance Characteristics

- **Computed goto dispatch** — bytecode interpreter uses threaded code for ~15%
  faster dispatch vs switch
- **Slot-indexed variables** — O(1) access via integer slot index instead of
  O(n) strcmp over scope names
- **COW+refcounting** — array copies only on shared mutation; read-only sharing
  is O(1)
- **Zero-copy slice views** — contiguous slices are O(1) views into the parent's
  backing store; flattened to owned copies only on mutation
- **Compile-time type tracking** — first-assigned type per variable; inferred
  function return types; drives push optimization decisions
- **Push optimization** — `x = x + [e]` is O(1) amortized; `OC_PUSH_ALL` handles
  any array expression in-place
- **Slot initialization** — array-typed slots pre-initialized to `[]` at scope
  creation, eliminating runtime type guards
- **TCO** — tail-recursive functions reuse the same C stack frame
- **Memory** — arrays are always `Value[]` (no compact backing stores), each
  Value is 24 bytes
- **TinyLang is ~2× faster than CPython** on comparable workloads, and
  4–31× slower than Node.js V8
