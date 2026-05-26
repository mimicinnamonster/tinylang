# TinyLang

A tiny, statically-typed programming language implemented in ~1090 lines of C.
Single-pass compiler to bytecode with a stack-based VM, refcount+COW, and tail
call optimization. No AST, no GC, no closures, no pointers.

## Features

- **Three types:** `number` (all floats), `array` (heterogeneous, deep equality), and `ptr` (FFI)
- **Value semantics:** No references, no aliasing, no GC — refcount+COW sharing
- **Tail call optimization:** Recursive functions don't blow the C stack
- **No operator precedence:** All binary ops require explicit `()`
- **Statement separation:** Newlines (Go-style inference) or explicit `;` separate statements
- **Functions:** Pure (no globals), define-before-use, no closures, recursion OK
- **Control flow:** `if`/`elif`/`else`, `while`
- **Arrays:** Nested, heterogeneous, `[val] * n` repetition, `arr + arr` concatenation
- **Multi-index:** `arr[i, j, k]` desugars to `arr[i][j][k]`
- **Strings:** Syntactic sugar for byte arrays, escape sequences supported
- **Operators:** `+ - * / %`, `& | ^ @` (shift), `= != < > <= >=`, `!`, `#` (array length prefix)
- **Built-ins:** `print()`, `input()`, `assert()`, `thispath()`
- **FFI:** `dlopen()`, `dlsym()`, `dlclose()`, `ffi_call()` — optional (requires libffi)

## Quick start

```sh
cc -Wall -Wextra -lm -o tinylang tinylang.c
./tinylang tests/test.tl
```

> **Note:** `-lm` is required on Linux and Android/Termux to link the math library (`fmod`). On macOS it links automatically and can be omitted.

### REPL

```sh
./tinylang          # bare REPL, no line editing
rlwrap ./tinylang   # with history and arrow keys (brew install rlwrap)
./repl.sh           # restarts on error (Ctrl+C to exit)
```

The REPL reads until braces balance before executing, so multi-line functions and blocks work naturally:

```
> function hello() {
    print("hi")
  }
> hello()
hi
```

Runtime errors print a message with source file and line number, plus a stack trace showing the call chain. Execution halts for the current input. In script mode the process exits with status 1; in the REPL it continues to the next input, preserving the current scope.

### FFI build

```sh
cc -Wall -DTL_FFI `pkg-config --cflags libffi` -o tinylang-ffi tinylang.c `pkg-config --libs libffi` -lm
```

Requires [libffi](https://github.com/libffi/libffi) (`brew install libffi` on macOS).
The FFI build registers four built-in functions: `dlopen()`, `dlsym()`, `dlclose()`, and
`ffi_call()` — enabling dynamic loading and calling of C functions at runtime.

The FFI tests include a demo that creates an actual SDL2 window:

```sh
./tinylang-ffi tests/ffi_sdl_window.tl
```

This requires [SDL2](https://www.libsdl.org/) (`brew install sdl2` on macOS).
The test loads SDL2 at runtime via `dlopen`, initializes the video subsystem,
creates a 640×480 window titled "TinyLang SDL Test", keeps it visible for 5
seconds with event pumping, then cleans up.

### Tests

```sh
./run_tests.sh      # runs all happy-path + error + FFI tests
```

- **Happy-path tests** — `tests/test_*.tl` (assertions, function calls, linked lists, TCO, arrays)
- **Benchmarks** — `tests/bench_*.tl` (backwards traversal, COW, push optimization)
- **Error tests** — `tests/e_*.tl` (expected runtime errors, each tested individually)
- **FFI tests** — `tests/ffi_*.tl` (library loading, symbol lookup, C function calls, SDL2 window demo)

All tests must pass before committing.

> **Note:** The SDL window test (`ffi_sdl_window.tl`) requires SDL2 to be installed.
> It is skipped automatically when SDL2 is not available (dlopen returns nil).

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
    if n = 0 {
        return acc
    }
    return fact(n - 1, n * acc)    // TCO: no stack growth
}

print(fact(5, 1))                   // 120
print(fact(1000, 1))                // inf (no stack overflow)

// manual heap pattern
nodes = [[0, -1]] * 10
nodes[0][0] = 42
print(nodes[0][0])                  // 42
```

## Implementation

- ~1090 lines of C, single file
- Optional FFI extension via libffi
- Pre-lexed token array → single-pass compiler → flat bytecode (`Instr[]`)
- Stack-based VM: 22 opcodes, `Value istk[4096]` stack
- Deep copy on assignment, refcount+COW arrays with push optimization
  (`x = x + [elem]` compiles to O(1) `OC_PUSH`, no array copy)
- Tail call optimization: parameter rebinding + ip reset (no C stack growth)
- `assert()` error catching via `setjmp`/`longjmp`
- FFI via `OC_CFUNC` opcode: registered C functions called at zero dispatch cost
- Comprehensive test suite (40+ tests: happy-path, benchmarks, error cases)

## Grammar

```
program       := top-level statements

statement     := assignment | if_stmt | while_stmt | func_def | ret_stmt | include_stmt | expr_stmt
include_stmt := "include" string

assignment    := lvalue "=" expr
lvalue        := identifier ("[" index_list "]")*
if_stmt       := "if" expr block ("elif" expr block)* ("else" block)?
while_stmt    := "while" expr block
func_def      := "function" identifier "(" params ")" block
ret_stmt      := "return" expr

block         := "{" stmt_list "}"
expr          := primary | primary op primary
primary       := number | identifier | "nil" | string | array_literal
               | call | index | "(" expr ")" | "!" primary | "-" primary

op            := "+" | "-" | "*" | "/" | "%" | "&" | "|" | "^" | "@"
               | "=" | "!=" | "<" | ">" | "<=" | ">="

index_list    := expr ("," expr)*
```

## Design

- [`DESIGN.md`](DESIGN.md) — Language design, types, semantics
- [`IMPLEMENTATION.md`](IMPLEMENTATION.md) — C implementation details, bytecode VM
