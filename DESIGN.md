# TinyLang — Design Notes

---

## 1. Type System

- **Two runtime types:** `number` (double) and `array` (Value[])
- No null/bool type — `[]` (empty array) serves as nil/false/absence
- `nil` is syntactic sugar for `[]`
- No `ptr` type — FFI has been removed
- First assignment determines variable type permanently
- `x = 0; x = "hello"` → type error, halts
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

### Compound Assignment

`+=`, `-=`, `*=`, `/=` are syntactic sugar that desugars at compile time into
the equivalent simple assignment. No new bytecodes or VM changes are needed —
the compiler emits the same instruction sequence as `x = x op y`.

**Simple variables:** `x += expr` compiles to read `x`, evaluate `expr`, apply
op, store to `x`.

**Indexed targets:** `a[i] += expr` compiles to:
- Evaluate index `i` once (for the store via `OC_LVALS`)
- Read variable `a`, re-evaluate index `i`, apply `OC_INDEX`
- Evaluate RHS `expr`, apply op
- Store through `OC_LVALS` using the first index copy

This means the index expression is evaluated twice at runtime. For simple
constant indices like `0` or `i` this is invisible; for complex index
expressions with side effects (e.g. function calls), the side effect occurs
twice.

**Compound assignment does not trigger the push optimization.** `x += [y]`
performs array concatenation (`x = x + [y]`), not push — use `x = x + [y]`
explicitly for the O(1) push optimization.

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
function name(params) { statements }
```

- `function` keyword
- `return expr` exits and returns `expr`; no return → returns `[]`
- Defined only at top level (no nested functions)
- Define-before-use (no forward references)
- Recursion works; tail calls are optimized (TCO) — no stack growth for
  `return f(...)` in tail position
- Not first-class — cannot be stored in variables or passed as arguments

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

`x = x + [expr]` is detected at compile time and compiled to `OC_PUSH` —
appends the element to the array in O(1) amortized instead of O(n) copy.

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
lvalue        := identifier ("[" expr "]")*

if_stmt       := "if" expr block ("elif" expr block)* ("else" block)?
while_stmt    := "while" expr block
func_def      := "function" identifier "(" params ")" block
ret_stmt      := "return" expr
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

params        := /* empty */ | identifier ("," identifier)*
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
- **Push optimization** — `x = x + [e]` is O(1) amortized
- **TCO** — tail-recursive functions reuse the same C stack frame
- **Memory** — arrays are always `Value[]` (no compact backing stores), each
  Value is 24 bytes
- **TinyLang is ~2× faster than CPython** on comparable workloads, and
  4–31× slower than Node.js V8
