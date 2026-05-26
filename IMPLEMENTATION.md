# TinyLang — Implementation Guide

**~1090 lines of C.** Single-pass compiler to bytecode with stack-based VM,
refcount+COW, tail call optimization, and array push optimization.

---

## 1. Value Representation

### Type system (three independent axes)

- **Size:** 8 → 16 → 32 → 64 bit
- **Signedness:** unsigned (preferred) → signed
- **Kind:** integer (precise) → float (F32/F64) → VAL (Value[])

```c
typedef enum { VAL_NUM, VAL_ARR, VAL_PTR } Type;

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
```

### Structs (16 bytes each, no waste)

```c
typedef struct Arr {
    int refcount, len, cap;
    ArrKind kind;
    union {
        struct Value *val;    // ARR_VAL: heterogeneous Value[]
        uint8_t *u8;          // ARR_U8:  raw uint8_t[]
        uint16_t *u16;        // ARR_U16: raw uint16_t[]
        uint32_t *u32;        // ARR_U32: raw uint32_t[]
        uint64_t *u64;        // ARR_U64: raw uint64_t[]
        int8_t *i8;           // ARR_I8:  raw int8_t[]
        int16_t *i16;         // ARR_I16: raw int16_t[]
        int32_t *i32;         // ARR_I32: raw int32_t[]
        int64_t *i64;         // ARR_I64: raw int64_t[]
        float *f32;           // ARR_F32: raw float[]
        double *f64;          // ARR_F64: raw double[]
    } as;
} Arr;

typedef struct Value {
    Type type;
    NumKind nkind;           // fits in what was padding
    union {
        uint8_t u8; int8_t i8;
        uint16_t u16; int16_t i16;
        uint32_t u32; int32_t i32;
        uint64_t u64; int64_t i64;
        float f32; double f64;
        Arr *arr;
        void *ptr;
    } as;
} Value;  // still 16 bytes
```

### Key: `vnum()` compresses, `val_num()` widens

```c
Value vnum(double n) {
    Value v = { .type = VAL_NUM, .nkind = NK_F64, .as.f64 = n };
    v.nkind = detect_num_kind(n);  // find narrowest fit
    switch (v.nkind) {             // store compactly
        case NK_I8:  v.as.i8 = (int8_t)n; break;
        case NK_F32: v.as.f32 = (float)n; break;
        // ...
    }
    return v;
}

double val_num(Value v) {
    switch (v.nkind) {
        case NK_I8:  return v.as.i8;
        case NK_F32: return v.as.f32;
        case NK_F64: return v.as.f64;
        // ...
    }
}
```

Every operation site in the VM and `apply()` uses `val_num()` to read
numbers transparently. `vassign()` uses a `*d = s` struct copy which
carries `nkind` automatically.

### Compact Array Detection (`detect_kind`)

At `OC_MAKE_ARR` runtime, the values on the stack are inspected:

1. All non-negative integers → narrowest unsigned (U8→U16→U32→U64)
2. Any negative integers → narrowest signed (I8→I16→I32→I64)
3. Non-integer → F32 if exact in float, else F64
4. Non-numeric → ARR_VAL

### Promotion Between Kinds (`promote_kind`)

`kind_exactly_covers()` checks whether one kind can losslessly represent
another. `promote_kind(a, b)` finds the smallest kind covering both by
iterating candidates from smallest to largest:

```
U8 → I8 → U16 → I16 → U32 → I32 → F32 → U64 → I64 → F64 → VAL
```

This correctly handles cross-type scenarios:
- U8 + I8 → I16 (neither can hold the other)
- U16 + I8 → I32 (I16 max 32767 < U16 max 65535)
- I32 + F32 → F64 (F32 can't exactly hold all I32 values)
- U64 + I64 → VAL (neither fits in the other)

### On mutation → VAL promotion

`amake_uniq()` promotes any compact array to ARR_VAL before writing.
This means compact types are a read-only optimization — writes always
fall back to heterogeneous Value[] storage.

### Refcount helpers

```c
Arr *aalloc(int cap, ArrKind k);  // kind-aware allocation
void aretain(Arr *a);
void arelease(Arr *a);            // kind-aware free
Arr *adeep_copy(Arr *s);          // kind-aware memcpy
void amake_uniq(Value *v);        // + promotes compact to VAL
void vassign(Value *d, Value s);  // struct copy (carries nkind)
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

### Instruction Set (23 opcodes)

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
    OC_TYPE,     // built-in type(): pop, inspect nkind/arrkind, push result
    OC_SLICE,    // pop arr, start, stop, step; push new sliced array
    OC_SLICE_ASSIGN, // pop start, stop, step; slice var[name] in-place or copy
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

### `type()` built-in

The `type()` built-in is handled directly in the compiler (before CReg and
function table lookup) and compiled to the `OC_TYPE` opcode. At runtime, it
pops one value from the stack and pushes a number representing the storage
kind:

```c
case OC_TYPE: {
    Value v = istk[isp--];
    if (v.type == VAL_NUM)
        istk[++isp] = vnum((double)v.nkind);          // 0-9
    else if (v.type == VAL_ARR) {
        if (v.as.arr)
            istk[++isp] = vnum((double)(100 + v.as.arr->kind));  // 100-110
        else
            istk[++isp] = vnum(-1);                    // nil
    } else
        istk[++isp] = vnum(-2);                        // ptr
    break;
}
```

This is purely for testing and debugging — it exposes the internal type
hierarchy that is otherwise invisible to the user.

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

### Slice compilation (`OC_SLICE` and `OC_SLICE_ASSIGN`)

The compiler detects Python-style slice syntax (`arr[start:stop]` or
`arr[start:stop:step]`) by scanning the tokens inside `[...]` for a `:` at
depth 0. The scan checks for stop conditions (`]` or `,` at depth 0) before
decrmenting depth, preventing false positives from a `:` in a later statement.

When a slice is detected, the compiler emits:
1. The array reference (via `OC_VAR`)
2. The `start`, `stop`, and `step` expressions (or defaults: `OC_NIL` for
   omitted bounds, `OC_NUM 1` for omitted step)
3. `OC_SLICE` — pops all four operands, pushes a new array with the sliced
   elements

**Slice assignment optimization:** The compiler also detects `x = x[slice]`
(same variable on both sides) before compiling the rvalue. Instead of
`OC_VAR x + slice operands + OC_SLICE + OC_STORE`, it emits the slice
operands directly followed by `OC_SLICE_ASSIGN name`. At runtime,
`OC_SLICE_ASSIGN` checks refcount:
- If `refcount == 1 && step == 1`: modifies the array in-place (memmove
  elements, adjust `len`) — O(1) for truncation, O(N) for shift but no
  allocation.
- Otherwise: allocates a new array (same as `OC_SLICE`) and assigns it to
  the variable via `vassign`, preserving value semantics.

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

**Total:** ~1160 lines.

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
