# TinyLang — Design Notes

---

## 1. Type System

- **Two runtime types:** `number` (all floats conceptually) and `array`
- No null/bool type — `[]` (empty array) serves as nil/false/absence
- `nil` is syntactic sugar for `[]`
- First assignment determines variable type permanently
- `x = 0; x = "hello"` → type error, halts
- Numbers stored as C `double` internally, with int optimization when no fractional part
- Arrays are **heterogeneous** — can hold numbers, strings, sub-arrays freely
- Array elements accessed by `arr[idx]` — 0-indexed, bounds checked, halts on OOB

---

## 2. Value Semantics — Reference Counting + Copy-on-Write

Arrays are shared via reference counting. A deep copy only happens when someone tries to **mutate** shared data. This preserves value semantics (mutating a copy never affects the original) while keeping read-only access O(1).

### Internal structure

```c
struct ArrayData {
    int refcount;      // how many Values point to this
    int len;
    int cap;
    Value items[];     // flexible array member
};

typedef struct {
    ValueType type;    // VAL_NUM or VAL_ARR
    union {
        double num;
        struct ArrayData *data;  // pointer to heap-allocated array
    } as;
} Value;
```

Numbers own no heap memory — they're just a `double` in the Value struct. Arrays point to a shared `ArrayData` with a refcount.

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

## 12. Statement Separation (Go-style)

- Newlines act as statement separators
- Semicolons inferred by lexer at newlines (never written manually)
- A newline is NOT a separator if the last token before it is an operator or opening `(`, `[`, `{`
- Consecutive blank lines are fine

```
x = 5
y = 10
print(x + y)

z = (x +            // x continues: + expects more
     y)

a = 10 b = 20       // ERROR: need newline between statements
```

Expression statements (evaluated for side effect, result discarded):

```
x + y               // valid: evaluates and discards
print(5)            // valid: calls print, discards return value
42                  // valid: literal expression, no-op
```

---

## 13. Literals & Identifiers

### Numbers

```
5            // integer
5.0          // float
.5           // 0.5
5.           // 5.0
```

Decimal only. No hex, octal, or binary literals.

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

Runtime errors halt with a message to stderr. No recovery, no try/catch.

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

expr          := primary
               | primary op primary       // exactly one binary op

primary       := number_literal
               | identifier
               | "nil"
               | string_literal
               | "[" "]"                              // empty array
               | "[" expr ("," expr)* "]"             // array literal
               | identifier "(" args ")"              // function call
               | identifier "[" index_list "]"          // rvalue array index chain
               | "(" expr ")"                         // grouping
               | "!" primary                          // negation
               | "-" primary                          // unary minus
               | "#" primary                          // array length

op            := "+" | "-" | "*" | "/" | "%"
               | "&" | "|" | "^" | "@"
               | "=" | "!=" | "<" | ">"

index_list    := expr ("," expr)*                    // comma-separated indices: arr[i,j,k]

params        := /* empty */ | identifier ("," identifier)*
args          := /* empty */ | expr ("," expr)*
```

Note: `lvalue` and `primary` both have `identifier "[" index_list "]"`. The parser distinguishes them by context: if followed by `"=""`, it's an lvalue; otherwise it's an rvalue.

---

## 17. Built-in Functions

| Name | Signature | Semantics |
|------|-----------|-----------|
| `print` | `print(x)` | Writes `x` to stdout. Numbers: decimal (no `.0` if integer). Arrays: `[e1, e2, ...]`. Empty array: `[]`. Strings (printed as text, not byte arrays). |
| `input` | `input()` | Reads a line from stdin, returns as byte array (string) |

`print` special-cases arrays whose elements are all printable ASCII or common control characters (10, 13, 9) — these are printed as the text string rather than `[104, 101, ...]`.

---

## 18. JIT-Ability Summary

Properties that make a future vectorizing JIT simpler than typical dynamic languages:

| Property | How the language provides it |
|----------|------------------------------|
| No aliasing | Refcount+COW guarantees isolation — JIT knows a refcount of 1 = exclusive ownership, safe for in-place mutation |
| Fixed variable types | First assignment locks type — no polymorphic guards |
| Pure functions | No global access — side-effect analysis is trivial |
| Monomorphic call sites | No first-class functions — every call targets one definition |
| Shallow expressions | No chaining without `()` — IR is small and local |
| `[]` = nil | Single concrete sentinel — one null check, cheap |
| Pre-allocation | `[0] * n` + refcount 1 = known-size flat buffer, exclusive ownership — JIT relayouts to `double[]` with zero guarding |
| No closures | Scope is flat per function — no captured environment |
| Simple CFG | Only `if`/`elif`/`else`/`while` — no switch, goto, exceptions |
| Error = halt | JIT can speculate without having to recover on error — just deopt to interpreter |
