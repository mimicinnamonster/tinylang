# TinyLang — Implementation Guide

**~1090 lines of C.** Single-pass compiler to bytecode with stack-based VM,
refcount+COW, tail call optimization, and array push optimization.

---

## 1. Value Representation

```c
typedef enum { VAL_NUM, VAL_ARR } Type;
typedef struct Arr { int refcount, len, cap; struct Value *items; } Arr;
typedef struct Value { Type type; union { double num; Arr *arr; } as; } Value;
```

Numbers: `type = VAL_NUM`, `as.num = double`.
Arrays: `type = VAL_ARR`, `as.arr = malloc'd Arr` with len/cap and flexible `items[]`.

### Refcount helpers

```c
Arr *aalloc(int cap) {
    Arr *a = calloc(1, sizeof(Arr));
    a->refcount = 1;
    a->cap = cap;
    a->items = cap ? calloc(cap, sizeof(Value)) : NULL;
    return a;
}
void aretain(Arr *a) { if (a) a->refcount++; }
void arelease(Arr *a) {
    if (!a) return;
    if (--a->refcount > 0) return;
    for (int i = 0; i < a->len; i++)
        if (a->items[i].type == VAL_ARR) arelease(a->items[i].as.arr);
    free(a->items); free(a);
}
Arr *adeep_copy(Arr *s);  // full tree copy for COW
void amake_uniq(Value *v); // COW: deep copy if refcount > 1
void vassign(Value *d, Value s);  // release old, retain new
```

---

## 2. Lexer

Produces a flat `Tok[]` array from source.

- Skip whitespace, handle `//` comments
- Every newline becomes `T_NL` (parser skips them at every entry point)
- Numbers: `5`, `5.0`, `.5`, `5.`
- Identifiers + keywords (`if`, `elif`, `else`, `while`, `function`, `return`, `nil`, `include`)
- Strings: `"..."` with escape sequences (`\n`, `\t`, `\\`, `\"`, `\xHH`)
- Single-char tokens: `( ) [ ] { } , ; + - * / % & | ^ @ ! = < > #`

The token array is the sole input to the compiler phase.

---

## 3. Bytecode Compiler

A single-pass recursive descent compiler that walks the token array and emits
`Instr[]` bytecode. No intermediate AST — each parser function emits instructions
directly.

### Instruction Set (22 opcodes)

```c
typedef enum {
    OC_NUM,      // push number constant
    OC_NIL,      // push empty array (nil)
    OC_STR,      // push pre-lexed string array
    OC_MAKE_ARR, // pop N values, build array, push
    OC_VAR,      // push variable value (by name)
    OC_STORE,    // pop and assign to variable (by name)
    OC_OP,       // pop r, pop l, apply binary op, push result
    OC_UNARY,    // pop, apply unary op (!, -, #), push
    OC_INDEX,    // pop idx, pop arr, push arr[idx]
    OC_LVALS,    // lvalue store: walk COW chain, assign
    OC_CALL,     // function call: pop args, exec body, push result
    OC_TCO,      // tail call: rebind params, restart body
    OC_JZ,       // pop, jump if falsy
    OC_JMP,      // unconditional jump
    OC_RET,      // set return flag and value, exit function
    OC_POP,      // discard top of stack
    OC_PRINT,    // built-in print
    OC_INPUT,    // built-in input
    OC_ASSERT,   // built-in assert (error-catching)
    OC_CFUNC,    // call registered C function (FFI)
    OC_PUSH,     // x = x + [elem]: pop elem, append to var array in-place
    OC_END,      // terminator
} OC;
```

### Instruction format

```c
typedef struct Instr {
    OC op; int a, b; double num; Arr *arr; char *name; Code *sub;
} Instr;

typedef struct Code { Instr *code; int len, cap; } Code;
```

- `a`, `b`: general-purpose (jump targets, argument counts, operator types)
- `num`: for `OC_NUM`
- `arr`: for `OC_STR` (pre-lexed string data)
- `name`: for `OC_VAR`/`OC_STORE`/`OC_LVALS` (variable name)
- `sub`: for `OC_ASSERT` (sub-code for the assertion expression)

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
| `comp_fn(c)` | Function definition + body compilation |

### Control flow compilation

**While loop:**
```
  [condition]       ← comp_expr
  JZ exit           ← placeholder, patched later
  [body]            ← comp_block
  JMP loop          ← jump back to condition
exit:               ← JZ target patched here
```

**If/elif/else:**
```
  [cond1]
  JZ elif1          ← patch to elif1 condition
  [body1]
  JMP end
elif1:
  [cond2]
  JZ else           ← or to end if no else
  [body2]
  JMP end
else:
  [body_else]
end:
```

### Function compilation

Functions are **pre-registered** in the function table before body compilation,
enabling recursive self-calls. Body is compiled to its own `Code` object:

```c
Fn *f = &fs[fc++];
f->n = name; f->p = params; f->a = arity;
f->code = NULL;              // placeholder

Code *body = new_code();
comp_block(body);
emit(body, OC_END);          // terminator (not OC_RET — see §4)
f->code = body;
```

Tail call optimization is detected by scanning backwards past `OC_END`/`OC_RET`
in the compiled body for a trailing `OC_CALL` to the same function. If found,
the `OC_CALL` is mutated to `OC_TCO`.

---

## 4. Stack-Based VM

### Execution loop

```c
void exec(Code *c) {
    int ip = 0;
    while (ip < c->len && !rf) {     // rf checked each iteration
        Instr *ins = &c->code[ip];
        switch (ins->op) {
            case OC_NUM:  istk[++isp] = vnum(ins->num); break;
            case OC_VAR:  istk[++isp] = sget(cs, ins->name); break;
            case OC_STORE: sset(cs, ins->name, istk[isp--]); break;
            case OC_OP: {
                Value r = istk[isp--], l = istk[isp--];
                Value res = apply(ins->a, l, r);
                if (l.type==VAL_ARR) arelease(l.as.arr);
                if (r.type==VAL_ARR) arelease(r.as.arr);
                istk[++isp] = res;
                break;
            }
            case OC_JZ: {
                Value v = istk[isp--];
                if (!truthy(v)) { ip = ins->a; continue; }
                break;
            }
            case OC_JMP: { ip = ins->a; continue; }
            case OC_RET: rv = istk[isp--]; rf = 1; break;
            case OC_END: return;
            // ...
        }
        ip++;
    }
}
```

- `istk[4096]`: Value stack with `isp` pointer
- `cs`: current scope (dynamic, name-based lookup via sset/sget)
- `rf`/`rv`: return flag and value
- Jumps use `continue` to skip the `ip++` at the bottom of the while loop
- Stack values are saved/restored around function calls to prevent callee
  overwrites (§5)

### Function calls

```c
case OC_CALL: {
    int fi = ins->a, ac = ins->b;
    Fn *f = &fs[fi];
    Value args[64];
    for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
    int saved_isp = isp;
    /* save caller's stack below saved_isp */
    Value saved[64];
    for (int j = 0; j <= saved_isp; j++) saved[j] = istk[j];
    Scp *saved_cs = cs; cs = snew();
    int saved_cur_fi = cur_fi; cur_fi = fi;
    for (int j = 0; j < f->a; j++)
        sset(cs, f->p[j], (j < ac) ? args[j] : nilv());
    int saved_rf = rf; rf = 0; isp = -1;
    exec(f->code);
    Value result = rf ? rv : nilv();  // no return → nil
    sfree(cs); cs = saved_cs; cur_fi = saved_cur_fi;
    rf = saved_rf; isp = saved_isp;
    for (int j = 0; j <= saved_isp; j++) istk[j] = saved[j];  // restore
    istk[++isp] = result;
    break;
}
```

### Tail Call Optimization (TCO)

```c
case OC_TCO: {
    int ac = ins->a;
    Fn *f = &fs[cur_fi];
    Value args[64];
    for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
    isp = -1; rf = 0;
    for (int j = 0; j < f->a; j++)
        sset(cs, f->p[j], (j < ac) ? args[j] : nilv());
    ip = 0; continue;  // restart function body
}
```

Instead of recursive C calls, TCO rebinds parameters in the current scope and
resets the instruction pointer to 0. No C stack growth.

---

## 5. Key Implementation Details

### Stack save/restore around calls

The VM uses a single global `istk` array. When a function call is made, the
callee's `exec` pushes/pops on the same array, potentially overwriting the
caller's stack values below `saved_isp`. To prevent this, the caller saves
all values at indices `0..saved_isp` before calling `exec` and restores them
after. The callee starts with `isp = -1`.

### `return` vs implicit function exit

- Explicit `return expr`: compiles as `[expr] OC_RET`. OC_RET pops the
  expression result, stores it in `rv`, and sets `rf = 1`.
- No return: the function body ends with `OC_END`. `rf` stays `0`, and
  `OC_CALL` returns `nil` (via `nilv()` macro, typed as `VAL_ARR` with `NULL` arr).

### `assert()` implementation

The assert built-in compiles its argument expression into a separate `Code`
sub-block. At runtime, `OC_ASSERT` sets up `setjmp`/`longjmp` around
`exec(sub)`. If the sub-expression triggers `die()`, the error is caught,
and the error message string is pushed as the result.

### Array mutation (COW)

Array lvalue assignment (`arr[i] = val`, `matrix[i][j] = val`) is handled by
`OC_LVALS`. The instruction stores the root variable name and the number of
index expressions. At runtime, it walks the chain:

1. Start with root variable slot
2. For each index: `amake_uniq(slot)` (COW if shared), then
   `slot = &slot->as.arr->items[idx]`
3. `vassign(slot, val)` — write the value into the final (private) slot

### LVALS walk

Array lvalue assignment walks from the root variable through each index.
For each index, `amake_uniq(slot)` ensures copy-on-write, then the slot
advances to the indexed element. The result is always correct regardless
of sharing — no cursor caching is needed.

### Push optimization (`OC_PUSH`)

The compiler detects `x = x + [expr]` (same variable both sides, single-element
array literal) and emits `OC_PUSH` instead of `OC_VAR + OC_MAKE_ARR +
OC_OP + OC_STORE`. At runtime, `OC_PUSH`:

1. Pops the element from the stack (the raw `expr` result, not an array)
2. Creates the variable's array if it's nil (`arr = []` → first push)
3. Calls `amake_uniq` on the variable slot (COW if shared with other vars)
4. Grows capacity if needed (amortized O(1), doubles capacity)
5. Writes the element via `vassign` into the new slot

This avoids allocating a temporary 1-element array and the full
concatenation copy. The `x = x + [elem]` pattern runs in O(1) per element
instead of O(len(x)). Value semantics are preserved because the optimization
only fires when the destination variable matches the source — `y = x + [1]`
falls through to the general `+` path.

### Refcount leak fixes

`OC_STORE`, `OC_LVALS`, and `OC_MAKE_ARR` popped values from the internal
stack without releasing the retain from the corresponding `OC_VAR` push.
This leaked +1 refcount per variable store, which prevented the push
optimization from detecting exclusive ownership. These opcodes now properly
release the stack reference after assignment.

**OC_INDEX multi-index fix:** The multi-index traversal path (`arr[idx]` where
`idx` is an array) released the root array `arr` inside the traversal loop
as `cur` advanced, and then released it again after the loop. This double-
release caused a use-after-free crash when refcounting was otherwise correct.
The post-loop release was removed, and `idx` is properly released after use.

### Include handling

`include "path"` is handled at compile time. The compiler saves its token
state, lexes the included file, compiles its statements into the current
`Code`, then restores the previous token state. This is recursive — included
files can themselves include other files.

---

## 6. FFI Integration

FFI is optional and enabled by `-DTL_FFI` at build time, which pulls in
`<dlfcn.h>` and `<ffi.h>`.

### C Function Registration

```c
typedef Value (*CFunc)(int, Value*);
typedef struct { char *name; CFunc func; } CReg;
static CReg *cregs; static int creg_count, creg_cap;

void tl_register(const char *name, CFunc func);
```

C functions are registered before compilation. At compile time, `comp_prim()`
checks the `cregs[]` array before falling through to TL function lookup. If
a match is found, it emits `OC_CFUNC` with the CReg index.

### OC_CFUNC opcode

```c
case OC_CFUNC: {
    int ci = ins->a, ac = ins->b;
    Value args[64];
    for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
    Value result = cregs[ci].func(ac, args);
    istk[++isp] = result;
    break;
}
```

### Built-in FFI functions (behind TL_FFI)

| Function | Signature | Description |
|----------|-----------|-------------|
| `dlopen` | `dlopen(path) → ptr` | Load a shared library |
| `dlsym` | `dlsym(handle, name) → ptr` | Look up a symbol by name |
| `dlclose` | `dlclose(handle) → []` | Unload a shared library |
| `ffi_call` | `ffi_call(fn, sig, ...) → value` | Call a C function by pointer |

`ffi_call` uses a signature string where the first character is the return
type and the rest are argument types:

| Char | C type |
|------|--------|
| `v` | `void` |
| `i` | `int` |
| `d` | `double` |
| `p` | `void*` |
| `s` | `const char*` (NUL-terminated) |

Example:
```
lib = dlopen("libm.so")
sqrt_fn = dlsym(lib, "sqrt")
result = ffi_call(sqrt_fn, "dd", 9.0)   // → 3.0
```

---

## 7. Line Count

**Total:** ~1090 lines.

| Component | Lines |
|-----------|-------|
| Value + ArrayData + refcount helpers | ~50 |
| Lexer | ~110 |
| Compiler — expressions + primaries | ~65 |
| Compiler — statements (if, while, assign, block) | ~90 |
| Compiler — functions, return, TCO | ~45 |
| Compiler — include, program | ~35 |
| Scope + function table | ~25 |
| VM — exec loop, all opcodes | ~200 |
| Built-ins (print, input, assert) | ~40 |
| FFI helpers (tl_to_cstring, dlopen, dlsym, dlclose, ffi_call) | ~100 |
| Main / REPL | ~30 |
| apply (operators + comparisons) | ~65 |
| Includes + structs + enums + globals | ~60 |
| print_val + truthy + veq + die | ~40 |
| FFI registration (CReg, tl_register) | ~20 |
| Include + readf | ~55 |

---

## 8. Implementation Order

1. `Value` + `Arr` + retain/release/COW
2. Lexer
3. Scope + function table
4. Compiler: expressions (numbers, identifiers, binary ops, unary)
5. Compiler: array expressions (literals, index, multi-index)
6. Compiler: statements (assignment, if, while, blocks)
7. Compiler: function definitions + return
8. VM: exec loop with basic opcodes
9. Built-ins (print, input) + `#` operator
10. OC_ASSERT with error catching
11. Main/REPL, compilation to bytecode, execution
12. FFI: CReg system, `OC_CFUNC` opcode, dlopen/dlsym/dlclose/ffi_call
