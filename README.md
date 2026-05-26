# TinyLang

A tiny, statically-typed programming language implemented in ~530 lines of C. No AST, no
GC, no closures, no pointers — just a single-pass tree-walk interpreter with
refcount+COW and tail call optimization.

## Features

- **Two types:** `number` (all floats) and `array` (heterogeneous, deep equality)
- **Value semantics:** No references, no aliasing, no GC — refcount+COW sharing
- **Tail call optimization:** Recursive functions don't blow the C stack
- **No operator precedence:** All binary ops require explicit `()`
- **Go-style newlines:** Statements separated by newlines with semicolon inference
- **Functions:** Pure (no globals), define-before-use, no closures, recursion OK
- **Control flow:** `if`/`elif`/`else`, `while`
- **Arrays:** Nested, heterogeneous, `[val] * n` repetition, `arr + arr` concatenation
- **Multi-index:** `arr[i, j, k]` desugars to `arr[i][j][k]`
- **Strings:** Syntactic sugar for byte arrays, escape sequences supported
- **Operators:** `+ - * / %`, `& | ^ @` (shift), `= != < > <= >=`, `!`, `#` (array length prefix)
- **Built-ins:** `print()`, `input()`

## Quick start

```sh
cc -Wall -Wextra -o tinylang tinylang.c
./tinylang test.tl
```

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

Errors kill the REPL process — `repl.sh` wraps it in a restart loop.

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

## Grammar

```
program       := top-level statements

statement     := assignment | if_stmt | while_stmt | func_def | ret_stmt | expr_stmt

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
- [`IMPLEMENTATION.md`](IMPLEMENTATION.md) — C implementation details, data structures

## Implementation

- ~530 lines of C, single file
- No external dependencies (ISO C + math.h)
- Pre-lexed token array, single-pass recursive descent parser
- Deep copy on assignment, refcount+COW for arrays
- Token-stream replay for while/function bodies
- TCO via parameter rebinding in tail position
