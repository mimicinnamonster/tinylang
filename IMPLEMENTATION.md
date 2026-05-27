# TinyLang — Implementation Guide

**~1,555 lines of C.** Single-pass compiler to bytecode with stack-based VM,
computed goto dispatch, slot-indexed variable access, compile-time type
tracking, refcount+COW, and tail call optimization.

---

## 1. Value Representation

### Types

```c
typedef enum { VAL_NUM, VAL_ARR } Type;

typedef struct Arr {
    int refcount, len, cap;
    Value *val;              // always Value[], no compact backing stores
} Arr;

typedef struct Value {
    Type type;
    double num;              // VAL_NUM: always double
    Arr *arr;                // VAL_ARR
} Value;                     // 24 bytes
```

### Key helpers

```c
Value vnum(double n) { return (Value){ .type = VAL_NUM, .num = n }; }
double val_num(Value v) { return v.num; }
```

No `detect_num_kind`, no `NumKind`, no 10-way union switch. Numbers are always
doubles. Arrays are always `Value[]` — no `ArrKind`, no compact backing stores.

### Refcount helpers

```c
Arr *aalloc(int cap);        // allocate Value[] array
void aretain(Arr *a);         // bump refcount
void arelease(Arr *a);        // decrement, free if 0 (recursive for sub-arrays)
Arr *adeep_copy(Arr *s);      // deep copy (for COW)
void amake_uniq(Value *v);    // COW: deep copy if refcount > 1
void vassign(Value *d, Value s);  // assign with proper retain/release
Value arr_item(Arr *a, int i);    // read element (always a->val[i])
```

---

## 2. Lexer

Produces a flat `Tok[]` array from source.

- Skip whitespace, handle `//` comments
- Every newline becomes `T_NL` (parser skips at entry points)
- Numbers: `5`, `5.0`, `.5`, `5.`, `0xFF` (hex)
- Identifiers + keywords (`if`, `elif`, `else`, `while`, `function`, `return`, `nil`, `include`)
- Strings: `"..."` with escape sequences (`\n`, `\t`, `\\`, `\"`, `\xHH`)
- Single-char tokens: `( ) [ ] { } , ; + - * / % ! = < > # :`
- Multi-char: `&&`, `||`, `!=`, `<=`, `>=`, `<<`, `>>`

Not supported: binary literals (`0b101`), octal literals (`0123`), scientific
notation (`1e-3`).

---

## 3. Bytecode Compiler

A single-pass recursive descent compiler that walks the token array and emits
`Instr[]` bytecode. No intermediate AST — each parser function emits instructions
directly.

### Instruction Set (23 opcodes)

```c
typedef enum {
    OC_NUM, OC_NIL, OC_STR, OC_MAKE_ARR,    // value producers
    OC_VAR,                                 // name-based variable access (top-level, read-only)
    OC_VAR_SLOT, OC_STORE_SLOT,             // slot-indexed variable access (functions)
    OC_OP, OC_UNARY,                        // arithmetic / unary
    OC_INDEX,                               // array indexing
    OC_CALL, OC_TCO,                        // function call + tail call
    OC_JZ, OC_JMP, OC_RET, OC_POP,          // control flow
    OC_LVALS, OC_PUSH, OC_SLICE,            // array operations
    OC_PRINT, OC_INPUT,                     // built-in I/O
    OC_DUP, OC_JNZ,                         // short-circuit && ||
    OC_PUSH_ALL,                            // push all elements from any array expr
    OC_END,                                 // terminator
} OC;
```

### Instruction format

```c
typedef struct Instr {
    OC op; int a, b; double num; Arr *arr; char *name; Code *sub; int line; char *file;
} Instr;
```

- `a`, `b`: general-purpose (slot indices, jump targets, argument counts, etc.)
- `num`: for `OC_NUM`
- `arr`: for `OC_STR` (pre-lexed string data)
- `name`: for `OC_VAR`/`OC_STORE` (variable name, top-level only)
- `sub`: unused

### Compile-time variable and type tracking

The compiler maintains a global `comp_vars[]` mapping variable names to slot
indices, and a parallel `comp_types[]` array tracking whether each variable
holds a number (`T_NUM_TYPE`) or an array (`T_ARR_TYPE`). Inside function
bodies, all variable reads/writes use `OC_VAR_SLOT`/`OC_STORE_SLOT` with the
integer slot index — O(1) runtime access with no strcmp. The mapping is
saved/restored around function compilation to isolate each function's variable
namespace.

**`set_var_type(slot, type)`** — Records the type on first assignment. Any
subsequent attempt to assign a different type halts with `"type mismatch"` at
compile time.

**`peek_expr_type(int *pn)`** — Infers the type of an expression by walking
the token stream ahead of compilation, without consuming tokens. Handles
literals, variables, function calls, `+` (num+num=num, else=array), `*
(arr*num=array), all other ops (num), indexed expressions (unknown), and
chained binary operators.

**Function return types** — Tracked per function in `Fn.ret_type`. Each
`return expr` infers its type; mismatches between returns halt at compile
time. Functions with no return get `ret_type = T_ARR_TYPE` (they return `[]`).

**Slot initialization** — Array-typed slots are pre-initialized to `[]` at
scope creation. Function scopes use `Fn.init_types[]` (saved from
`comp_types[]` before freeing); top-level/REPL scopes are initialized in
`main()` after compilation. This eliminates runtime type guards in
`op_push` and `op_push_all`.

### Compiler functions

| Function | Compiles |
|----------|----------|
| `comp_program(c)` | Top-level statement loop |
| `comp_stmt(c)` | One statement (if, while, fn, return, assign, expr) |
| `comp_block(c)` | `{ stmt_list }` |
| `comp_expr(c)` | Expression (leaves one value on stack) |
| `comp_prim(c)` | Primary expression (literal, ident, call, array, unary) |
| `comp_if(c)` | `if`/`elif`/`else` with backpatching |
| `comp_while(c)` | `while` with loop/exit jumps |
| `comp_fn(c)` | Function definition + body + TCO detection |
| `comp_include(c)` | `include` directive (string literal or expression) |
| `eval_include_path()` | Compile-time evaluation of include path expressions |

### Compound assignment desugaring

Compound assignment operators (`+=`, `-=`, `*=`, `/=`) are desugared entirely
at the token level — no new bytecodes are required. The lexer recognizes
`+=`, `-=`, `*=`, `/=` as single tokens (`T_PL_ASSIGN` etc.) by peeking ahead
one character after `+`, `-`, `*`, `/`.

In `comp_stmt`, when the `T_ID` case detects a compound assignment token, it
rewrites the token stream in-place so that the plain `=` assignment handler
parses it identically to the user having written `x = x op expr`.

**Token rewrite:** For `x += RHS`, the compiler:
1. Records the position of the variable name token (`name_tp`)
2. Parses any index brackets (compiling them as LHS store indices)
3. Calculates the LHS token count: `lhs = tp - name_tp` (name + brackets)
4. Shifts the RHS tokens right by `lhs + 1` slots using `memmove`
5. Sets `ts[tp].t = T_ASSIGN` (replaces compound token with `=`)
6. Copies the LHS tokens (name + brackets) from `name_tp` into the gap
7. Inserts the arithmetic operator token (`T_PL`, `T_MI`, etc.) after the
   copied LHS
8. Falls through to the plain `=` assignment handler

This means the exact same code handles both `arr += [elem]` and
`arr = arr + [elem]` — including the push optimization, operator precedence
in the RHS expression, and indexed lvalue chains. Everything is coupled.

**Trace for `a[i] += 10`:**
```
Original tokens:  a  [  i  ]  +=  10
                                ↑tp
LHS tokens copied from name_tp:  a  [  i  ]
Rewritten:        a  [  i  ]  =  a  [  i  ]  +  10
                                ↑tp
                                T_ASSIGN (plain path)
```
The plain path then:
1. Uses the first `a[i]` as the LHS store target (compiles `i` for `OC_LVALS`)
2. `comp_expr(c)` compiles the RHS `a[i] + 10` (reads `a`, indexes with `i`,
   adds 10)
3. Emits `OC_LVALS` to store the result

The `i` in the LHS and the `i` in the RHS are separate — each is compiled
from its own set of tokens, so the index expression is evaluated twice at
runtime. This is identical to writing `a[i] = a[i] + 10` explicitly.

**Push optimization:** Because `arr += [elem]` rewrites to `arr = arr + [elem]`
before the plain handler sees it, the same lookahead fires and emits
`OC_PUSH` (for bracket literals) or `OC_PUSH_ALL` (for other array
 expressions).

**`is_bracket_literal` helper:** Checks whether tokens at a given position
form a `[...]` bracket pair (any contents). Used by push optimization
lookahead.

### Push optimization detection

The compiler detects `x = x + [...]` by lookahead on the token stream.

**Three tiers, in order of preference:**

1. **Per-element push (`OC_PUSH`):** Fires when the RHS is a bare bracket
   literal `[a, b, ...]` with no chained operators following. Each element
   gets its own `OC_PUSH` — zero temporary arrays allocated. Works for both
   single and multi-element literals.

2. **Push-all (`OC_PUSH_ALL`):** Fires when the RHS starts with `x + ` and
   `x` is a known array (from `comp_types[]`). The compiler skips `x + `
   and compiles the remaining RHS expression normally. At runtime,
   `op_push_all` reads the RHS array from the stack and pushes all its
   elements into `x` in-place. A fallback handles non-array RHS via
   `apply(T_PL, ...)` for edge cases like string concatenation.

3. **Normal `+`:** Everything else — `apply(T_PL, ...)` creates a new
   concatenated array and stores it back.

### TCO detection

After compiling a function body, the compiler scans the last few instructions
for `OC_CALL` to the same function in tail position. If found, it is mutated
to `OC_TCO` — parameter rebinding + instruction pointer reset, no C stack
growth.

### Include expressions

Normally `include` takes a string literal whose path is resolved relative to
the including file's directory. The compiler also supports compile-time
expressions in the include path via `eval_include_path()`.

The evaluator handles three cases:
- **String literals** — extracted directly from the token stream.
- **`thispath()`** — returns the directory of the current source file (with
  trailing `/`), computed from `comp_file` at compile time.
- **`+` concatenation** — left-to-right string concatenation of the above.

When the include path comes from an expression (rather than a plain string
literal), `include_dir` is not prepended — the expression result is used
directly. This makes `include thispath() + "foo.tl"` resolve the path
relative to the current file's directory.

---

## 4. VM Executor (Computed Goto)

### Dispatch mechanism

The VM uses **computed goto** (GNU C extension `&&` address-of-label) instead
of a `while`+`switch` loop:

```c
void exec(Code *c) {
    static void *dispatch[] = {
        [OC_NUM] = &&op_num,
        [OC_VAR] = &&op_var,
        [OC_OP]  = &&op_op,
        // ...
    };
    int ip = 0;
    goto *dispatch[c->code[ip].op];  // start

op_num:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    istk[++isp] = vnum(c->code[ip].num);
    ip++; goto *dispatch[c->code[ip].op];

op_var_slot: {
    int slot = c->code[ip].a;
    Value v = cs->v[slot];           // O(1), no strcmp
    istk[++isp] = v; if (v.type == VAL_ARR) aretain(v.arr);
    ip++; goto *dispatch[c->code[ip].op];
}
// ...
}
```

Each opcode handler sets the error location, executes its operation, advances
`ip`, and dispatches directly to the next handler — 1 jump per bytecode instead
of 3 (switch + while check + branch back).

### Slot-indexed variable access

Variables inside function bodies are accessed by integer slot index:

```c
// OC_VAR_SLOT: push variable value (O(1) array lookup)
case OC_VAR_SLOT:
    istk[++isp] = cs->v[ins->a];
    break;

// OC_STORE_SLOT: assign to variable (O(1) array store)
case OC_STORE_SLOT:
    vassign(&cs->v[ins->a], istk[isp--]);
    break;
```

Scopes are pre-allocated with `snew_sized(nvars)` — the exact number of
variables is known after compilation. Parameters are bound by slot index
directly, bypassing name lookup entirely.

### Stack save/restore around calls

The VM uses a single global `istk` array. When a function call is made, the
caller saves `istk[0..saved_isp]` to a local array before calling `exec(f->code)`
and restores them after. This prevents callee overwrites. With a max of 64
saved entries, this is a 1KB memcpy per call — a known bottleneck for heavy
function call workloads.

### Function calls

```c
case OC_CALL:
    // 1. Pop args from stack
    // 2. Save caller's stack (memcpy 64 Values)
    // 3. Create pre-sized scope: snew_sized(f->nvars)
    // 4. Bind params by slot index (O(1) each, no strcmp)
    // 5. Save/restore rf/rv, call_depth
    // 6. exec(f->code) — recursive C call
    // 7. Restore everything
    break;
```

### TCO

```c
case OC_TCO:
    // 1. Pop args
    // 2. Bind params by slot index (same scope, no allocation)
    // 3. ip = 0; restart function body
    break;
```

---

### String + Number Concatenation

The `apply()` function for `T_PL` handles three mixed-type cases beyond the core
number+number (addition) and array+array (concatenation):

- **string + number**: when the left operand is a `VAL_ARR` whose contents look
  like a printable ASCII string (same heuristic as `print_val()`), the number
  is converted to its decimal string representation via `num_to_string_arr()`
  and the two are concatenated as arrays.
- **number + string**: symmetric case — right operand must look like a string.
- **anything else**: dies with `"'+' type mismatch"`.

The `is_string_arr()` helper checks that all array elements are `VAL_NUM` and
fall in the printable ASCII range (32–126), plus tab (9), newline (10), and
carriage return (13). This matches the heuristic `print_val()` uses to decide
whether to display an array as text. Generic arrays like `[1, 2, 3]` contain
non-printable bytes and correctly produce a type error.

The `num_to_string_arr()` helper formats numbers identically to `print_val()`:
integers print as decimal without a decimal point, floats use `%g`.

### Removed opcodes and dead code

| What | Why |
|------|-----|
| `OC_STORE` / `op_store` / `sset()` | Never emitted — all stores use `OC_STORE_SLOT` |
| `Fn.p` field | Never written or read |
| `vempty()` function | Same as `nilv()` macro, never called |
| `op_push` LHS type guard | Slot is always initialized to `[]` for array-typed vars |
| `op_push_all` LHS type guard | Same |
| Slot-based op else-branches | Name lookup fallback was dead — `comp_vars` persists across REPL lines |

---

## 6. Array Operations

### COW on mutation

```c
void amake_uniq(Value *v) {
    if (v->type != VAL_ARR || !v->arr) return;
    if (v->arr->refcount > 1) {
        Arr *old = v->arr;
        v->arr = adeep_copy(old);      // copy on write
        arelease(old);
    }
}
```

### Lvalue chain (arr[i][j] = val)

`OC_LVALS` walks a chain starting from the root variable's slot. At each level,
`amake_uniq` ensures exclusive ownership before advancing to the indexed
element. The final slot receives the assigned value via `vassign`.

### Push (arr = arr + [x]) / Push-all (arr = arr + expr)

`OC_PUSH` is emitted when the compiler detects `x = x + [elem]`. It pops the
element, calls `amake_uniq` on the target array (COW if shared), grows capacity
if needed (doubles each realloc), and writes the element into the new slot.
Amortized O(1). Multi-element bracket literals `[a, b, c]` emit one `OC_PUSH`
per element.

`OC_PUSH_ALL` handles any array expression on the RHS. It reads the RHS array
from the stack, calls `amake_uniq` on the target, then iterates the RHS
pushing each element in a loop. If the RHS is not an array at runtime, it falls
back to `apply(T_PL, ...)` for correct type-error behavior.

### Slice (arr[start:stop:step])

`OC_SLICE` pops the array, start, stop, and step values from the stack, clamps
bounds (handling negative indices and omitted bounds per Python semantics),
copies elements into a new `Value[]` array, and pushes the result.

---

## 7. Line Count

**Total:** ~1,555 lines.

| Component | Lines |
|-----------|-------|
| Includes, types, enums, globals | ~60 |
| Value + Array helpers | ~70 |
| `apply()` (operators) | ~85 |
| `die()` (error handling) | ~50 |
| `print_val()`, `truthy()`, `veq()` | ~40 |
| Scope (snew, sfree, sget, snew_sized) | ~25 |
| Bytecode helpers + Type tracking | ~45 |
| Lexer | ~100 |
| Compiler — all functions (incl. type inference) | ~320 |
| VM executor — exec() with computed goto | ~310 |
| Main / REPL | ~70 |
| Include handling + expression eval | ~60 |

---

## 8. Performance Model

- **Computed goto dispatch:** ~15% faster than switch (1 jump/bytecode vs 3)
- **Slot-indexed variables:** ~50% reduction in variable access cost (O(1) vs
  O(n) strcmp). Biggest win for variable-heavy loops.
- **Combined speedup over original switch+strcmp VM:** 55-85% across benchmarks.
- **TinyLang vs CPython:** ~2× faster on numeric workloads.
- **TinyLang vs Node.js V8:** 4-31× slower (V8 JIT-compiles to native code).
- **TinyLang vs naive C:** ~10-200× slower (interpreted dispatch overhead).
