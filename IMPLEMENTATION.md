# TinyLang — Implementation Guide

Target: **under 1000 lines of C**. Single-pass tree-walk interpreter with refcount+COW.

---

## 1. Value Representation

```c
typedef enum { VAL_NUM, VAL_ARR } ValueType;

// Heap-allocated array data — shared via refcount
typedef struct ArrayData {
    int refcount;
    int len;
    int cap;
    struct Value items[];   // flexible array member
} ArrayData;

// A single value — 16 bytes on x86-64
typedef struct Value {
    ValueType type;
    union {
        double num;
        ArrayData *data;    // owns a refcounted reference
    } as;
} Value;
```

Numbers: `type = VAL_NUM`, `as.num = double`.
Arrays: `type = VAL_ARR`, `as.data = malloc'd ArrayData`.

### Refcount helpers

```c
ArrayData *array_alloc(int cap) {
    ArrayData *d = malloc(sizeof(ArrayData) + cap * sizeof(Value));
    d->refcount = 1;
    d->len = 0;
    d->cap = cap;
    return d;
}

void array_retain(ArrayData *d) {
    if (d) d->refcount++;
}

void array_release(ArrayData *d) {
    if (!d) return;
    if (--d->refcount > 0) return;
    // Recursively release sub-arrays
    for (int i = 0; i < d->len; i++)
        if (d->items[i].type == VAL_ARR)
            array_release(d->items[i].as.data);
    free(d);
}

// Deep copy for COW — duplicates the entire tree
ArrayData *array_deep_copy(ArrayData *src) {
    ArrayData *d = array_alloc(src->cap);
    d->len = src->len;
    for (int i = 0; i < src->len; i++) {
        d->items[i] = src->items[i];
        if (d->items[i].type == VAL_ARR)
            array_retain(d->items[i].as.data);     // share sub-arrays (they'll COW on their own mutation)
    }
    return d;
}
```

### Value lifecycle

```c
// Assign one Value to another — manages refcounts
void val_assign(Value *dst, Value src) {
    if (dst->type == VAL_ARR)
        array_release(dst->as.data);
    *dst = src;
    if (src.type == VAL_ARR)
        array_retain(src.as.data);
}

// Ensure dst has exclusive ownership — COW
void val_make_unique(Value *dst) {
    if (dst->type != VAL_ARR || dst->as.data->refcount <= 1)
        return;
    ArrayData *old = dst->as.data;
    dst->as.data = array_deep_copy(old);
    array_release(old);    // decrement old — may free if we had the last ref
}
```

---

## 2. Token Types

```c
typedef enum {
    TOK_EOF,
    TOK_NUMBER, TOK_IDENT, TOK_STRING,
    TOK_NIL,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACK, TOK_RBRACK,
    TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_AMPERSAND, TOK_PIPE, TOK_CARET, TOK_AT,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT,
    TOK_BANG,
    TOK_ASSIGN,
    TOK_IF, TOK_ELIF, TOK_ELSE, TOK_WHILE,
    TOK_FUNCTION, TOK_RETURN,
    TOK_NEWLINE,
} TokenType;

typedef struct {
    TokenType type;
    union { double num; char *str; } val;
    int line;
} Token;
```

Note: `TOK_EQ` is `=` (comparison). `TOK_ASSIGN` is also `=` but emitted by the statement parser when `=` appears at statement-start context. Actually — simpler: the parser knows context. The lexer returns `TOK_EQ` for `=` always. The parser knows: "if I'm at statement-start and see `identifier TOK_EQ`, this is assignment."

### Semicolon insertion (Go-style)

In the lexer, after emitting any token that can end a statement, check if the next character is a newline. If so, emit `TOK_NEWLINE`.

Tokens that trigger semicolon insertion: `TOK_NUMBER`, `TOK_IDENT`, `TOK_STRING`, `TOK_RPAREN`, `TOK_RBRACK`, `TOK_RBRACE`, `TOK_NIL`.

Tokens that DO NOT trigger it (expression continues): operators like `TOK_PLUS`, `TOK_MINUS`, etc., and opening `(` `[` `{`.

### Lexer functions

```c
Token lex_next();                // returns next token
void lex_init(const char *src); // set up lexer state
```

Lexer handles:
- Skipping whitespace (spaces, tabs)
- Newline → maybe TOK_NEWLINE (based on semicolon rule)
- `//` → skip to end of line
- Digits → TOK_NUMBER (handles `5`, `5.0`, `.5`, `5.`)
- `[a-zA-Z_]` → keyword or TOK_IDENT
- `"` → TOK_STRING with escape processing
- Single-char tokens by character

Keywords: `if`, `elif`, `else`, `while`, `function`, `return`, `nil`.

Identifiers: `[a-zA-Z_][a-zA-Z0-9_]*`.

String escapes: `\n` → 10, `\t` → 9, `\\` → 92, `\"` → 34, `\xHH` → hex byte.

---

## 3. Parser / Interpreter

### Environment (Scope)

```c
typedef struct {
    char **names;
    Value *values;
    int count, cap;
} Scope;

Scope *scope_new();
void scope_free(Scope *s);
Value  scope_get(Scope *s, const char *name);
void   scope_set(Scope *s, const char *name, Value val);  // O(1) overwrite or add
```

Simple dynamic arrays. Linear scan for lookups — fine for <1000 lines.

### Function table

```c
typedef struct {
    char *name;
    char **params;
    int arity;
    // Store source to re-parse on call
    // Option A: token stream (pre-lexed)
    // Option B: source position (start_line, start_col)
} Function;
```

Functions are stored in a global linked list or dynamic array.

**Storage choice:** Since we're single-pass, when we see `function foo(x, y) { ... }`, we:
1. Note the function name and parameter list
2. Store the position (line+col) of the `{` and `}` in the source
3. Skip the body (don't execute it yet)
4. On call: jump back to that source position, parse + execute the body with a fresh scope

Or simpler: store the tokenized body as a buffer of Tokens:

```c
typedef struct {
    char *name;
    char **params;
    int arity;
    Token *body;        // tokenized body
    int body_len;
} Function;
```

### Expression parser (typed by context)

```c
Value parse_expr();              // normal expression — = is comparison
Value parse_primary();           // literal, identifier, array literal, function call, (expr), unary
Value parse_condition();         // for if/while — same as expr but stops at { or }
```

`parse_expr`:
```
expr := primary
      | primary op primary       // exactly one binary op
```

After parsing `primary op primary`, check if another `op` follows → error (chaining requires `()`).

### Statement parser

```c
void parse_statement();
    // dispatches by current token:
    // TOK_IF        → parse_if()
    // TOK_WHILE     → parse_while()
    // TOK_FUNCTION  → parse_function()
    // TOK_RETURN    → parse_return()
    // TOK_LBRACE    → parse_block()
    // TOK_IDENT     → peek ahead: if followed by TOK_EQ or TOK_LBRACK → parse_assignment()
    //                   else → parse_expr() as expression statement
    // TOK_NEWLINE   → skip
    // else           → parse_expr() as expression statement
```

### Lvalue parsing

```c
typedef struct { Value *slot; } LValue;

LValue parse_lvalue();
    // Consumes identifier + optional index chain
    // Returns pointer to the slot that would be written to
    // For simple variable: &scope->values[idx]
    // For arr[i][j]: walks through ArrayData->items pointers
```

`parse_assignment()`:
```c
LValue target = parse_lvalue();     // identifier ([" index_list "])*
expect(TOK_ASSIGN);                 // consume =
Value val = parse_expr();
val_assign(target.slot, val);
```

### Lvalue index chain (COW-safe)

When walking `arr[i][j]` as an lvalue:

```c
Value *walk_lvalue_chain(Value *root) {
    // root = pointer to the array Value in scope
    while (peek() == TOK_LBRACK) {
        advance(); // consume [
        Value idx = parse_expr();
        expect(TOK_RBRACK);
        
        // COW: ensure the array we're about to index is uniquely owned
        val_make_unique(root);
        if (root->type != VAL_ARR) error("not an array");
        int i = (int)num_val(idx);
        if (i < 0 || i >= root->as.data->len) error("index out of bounds");
        
        root = &root->as.data->items[i];  // pointer into the array's buffer
    }
    return root;
}
```

Each level of the chain triggers COW independently. After walking `matrix[i][j]`, if `matrix` was shared, `matrix`'s data is deep-copied before any mutation.

### Index expression (rvalue)

```c
Value parse_index_expr(Value arr) {
    // We've already consumed the identifier and are inside [...]
    // Parse index_list
    Value indices[256];
    int n = 0;
    
    do {
        indices[n++] = parse_expr();
    } while (match(TOK_COMMA));
    
    expect(TOK_RBRACK);
    
    if (n == 1) {
        // Single index: might be number or array (dynamic chain)
        if (indices[0].type == VAL_NUM) {
            return array_index(arr, (int)num_val(indices[0]));
        } else if (indices[0].type == VAL_ARR) {
            Value cur = arr;
            for (int i = 0; i < indices[0].as.data->len; i++) {
                Value idx = indices[0].as.data->items[i];
                cur = array_index(cur, (int)num_val(idx));
            }
            return cur;
        } else {
            error("index must be number or array");
        }
    }
    
    // Multiple indices: chain them (static depth)
    Value cur = arr;
    for (int i = 0; i < n; i++) {
        cur = array_index(cur, (int)num_val(indices[i]));
    }
    return cur;
}
```

### Single element access

```c
Value array_index(Value arr, int idx) {
    if (arr.type != VAL_ARR) error("not an array");
    if (idx < 0 || idx >= arr.as.data->len) error("index out of bounds");
    return arr.as.data->items[idx];  // returns by value, refcount already correct
}
```

Returns a borrowed Value — the caller is responsible for calling `array_retain` if they store it.

---

## 4. Binary Operations

```c
Value apply_op(Token op, Value left, Value right) {
    switch (op.type) {
        case TOK_PLUS:
            if (left.type == VAL_NUM && right.type == VAL_NUM)
                return val_num(left.as.num + right.as.num);
            if (left.type == VAL_ARR && right.type == VAL_ARR)
                return array_concat(left, right);
            error("'+' on mismatched types");
        case TOK_STAR:
            if (left.type == VAL_NUM && right.type == VAL_NUM)
                return val_num(left.as.num * right.as.num);
            if (left.type == VAL_ARR && right.type == VAL_NUM)
                return array_repeat(left, (int)num_val(right));
            error("'*' on mismatched types");
        case TOK_EQ:
            return val_bool(val_equal(left, right));
        case TOK_NEQ:
            return val_bool(!val_equal(left, right));
        // ... etc for all ops
    }
}
```

`val_bool(x)`: returns `val_num(1.0)` if x is truthy, `val_empty_array()` if falsy.

Array equality: recursive deep comparison. Two arrays are equal if they have the same length and every element is equal.

### Array concatenation

```c
Value array_concat(Value a, Value b) {
    int new_len = a.as.data->len + b.as.data->len;
    ArrayData *d = array_alloc(new_len);
    d->len = new_len;
    for (int i = 0; i < a.as.data->len; i++)
        val_assign(&d->items[i], a.as.data->items[i]);
    for (int i = 0; i < b.as.data->len; i++)
        val_assign(&d->items[i + a.as.data->len], b.as.data->items[i]);
    Value result = { .type = VAL_ARR, .as.data = d };
    return result;
}
```

### Array repeat

```c
Value array_repeat(Value arr, int count) {
    if (count <= 0) return val_empty_array();
    int base_len = arr.as.data->len;
    int new_len = base_len * count;
    ArrayData *d = array_alloc(new_len);
    d->len = new_len;
    for (int i = 0; i < count; i++)
        for (int j = 0; j < base_len; j++)
            val_assign(&d->items[i * base_len + j], arr.as.data->items[j]);
    Value result = { .type = VAL_ARR, .as.data = d };
    return result;
}
```

---

## 5. If / While Parsing

```c
void parse_if() {
    expect(TOK_IF);
    Value cond = parse_condition();  // reads until {
    expect(TOK_LBRACE);
    parse_block();
    expect(TOK_RBRACE);
    
    while (match(TOK_ELIF)) {
        Value elif_cond = parse_condition();
        expect(TOK_LBRACE);
        parse_block();
        expect(TOK_RBRACE);
    }
    
    if (match(TOK_ELSE)) {
        expect(TOK_LBRACE);
        parse_block();
        expect(TOK_RBRACE);
    }
}

void parse_while() {
    expect(TOK_WHILE);
    Value cond = parse_condition();
    expect(TOK_LBRACE);
    
    while (is_truthy(cond)) {   // loop: re-check condition each iteration
        // Execute body
        // But wait — in a single-pass interpreter, how do we loop?
    }
}
```

Wait — `while` is tricky in a single-pass tree-walk. The body needs to be re-executed. Options:

**Option A: store the body tokens.** When entering `while`, record the body's token stream. Execute it in a loop, re-checking the condition each time.

```c
void parse_while() {
    expect(TOK_WHILE);
    Value cond = parse_condition();  // evaluate condition once
    expect(TOK_LBRACE);
    
    // Save position for re-execution
    int body_start = token_pos;
    int brace_depth = 1;
    // Count tokens until matching }
    while (brace_depth > 0) { ... }
    int body_end = token_pos;
    
    // Loop
    while (is_truthy(cond)) {
        execute_tokens(body_start, body_end);
        cond = re_eval_condition();  // need to store the condition expression too
    }
}
```

**Option B: store the condition as a token stream too.** Both condition and body are tokenized regions that get re-executed.

Simpler: store everything as token positions:

```c
int while_cond_start, while_cond_end;
int while_body_start, while_body_end;

// Parse condition: record start, parse tokens until {, record end
while_cond_start = token_pos;
cond = parse_condition();   // parses tokens, advancing token_pos
while_cond_end = token_pos;
expect(TOK_LBRACE);

// Parse body: record start, skip balanced braces, record end
while_body_start = token_pos;
skip_balanced_block();
while_body_end = token_pos;

// Loop: re-execute condition + body
while (1) {
    Token saved = set_token_pos(while_cond_start);
    Value c = parse_condition();
    set_token_pos(saved);
    if (!is_truthy(c)) break;
    
    saved = set_token_pos(while_body_start);
    execute_block();
    set_token_pos(saved);
}
```

Function bodies work the same way — store token positions for the body, parse+execute on call.

---

## 6. Function Call

```c
Value call_function(const char *name, Value *args, int argc) {
    Function *f = func_table_lookup(name);
    if (!f) error("undefined function '%s'", name);
    
    // Check built-ins first
    if (is_builtin(name)) return call_builtin(name, args, argc);
    
    // Create new scope
    Scope *scope = scope_new();
    
    // Bind parameters
    for (int i = 0; i < f->arity; i++) {
        Value v = (i < argc) ? args[i] : val_empty_array();
        scope_set(scope, f->params[i], v);  // v is already retained by caller
    }
    
    // Execute body
    Value result = execute_tokens(f->body_start, f->body_end, scope);
    
    scope_free(scope);
    return result;
}
```

---

## 7. Built-in Functions

```c
Value call_builtin(const char *name, Value *args, int argc) {
    if (!strcmp(name, "print")) {
        if (argc < 1) error("print requires 1 argument");
        print_value(args[0]);
        return val_empty_array();
    }
    if (!strcmp(name, "len")) {
        if (argc < 1) error("len requires 1 argument");
        if (args[0].type != VAL_ARR) error("len requires array");
        return val_num(args[0].as.data->len);
    }
    if (!strcmp(name, "input")) {
        char buf[1024];
        if (!fgets(buf, sizeof(buf), stdin)) return val_empty_array();
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') len--;   // strip newline
        // Build byte array
        ArrayData *d = array_alloc(len);
        d->len = len;
        for (int i = 0; i < len; i++)
            d->items[i] = val_num((unsigned char)buf[i]);
        return (Value){ .type = VAL_ARR, .as.data = d };
    }
    error("unknown built-in '%s'", name);
}
```

### print_value

```c
void print_value(Value v) {
    if (v.type == VAL_NUM) {
        if (v.as.num == (double)(int64_t)v.as.num)
            printf("%lld", (int64_t)v.as.num);    // no .0 for integers
        else
            printf("%g", v.as.num);
    } else if (v.type == VAL_ARR) {
        // Check if all elements are printable ASCII
        int all_printable = 1;
        for (int i = 0; i < v.as.data->len; i++) {
            if (v.as.data->items[i].type != VAL_NUM) { all_printable = 0; break; }
            int c = (int)v.as.data->items[i].as.num;
            if (c < 32 && c != '\n' && c != '\t' && c != '\r') { all_printable = 0; break; }
            if (c > 126) { all_printable = 0; break; }
        }
        
        if (all_printable) {
            for (int i = 0; i < v.as.data->len; i++)
                putchar((int)v.as.data->items[i].as.num);
        } else {
            putchar('[');
            for (int i = 0; i < v.as.data->len; i++) {
                if (i > 0) printf(", ");
                print_value(v.as.data->items[i]);
            }
            putchar(']');
        }
    }
}
```

---

## 8. Main / REPL

```c
int main(int argc, char **argv) {
    if (argc >= 2) {
        // Run file
        char *src = read_file(argv[1]);
        lex_init(src);
        execute_program();
        free(src);
    } else {
        // REPL
        printf("TinyLang v0.1\n");
        char line[4096];
        while (1) {
            printf("> ");
            if (!fgets(line, sizeof(line), stdin)) break;
            lex_init(line);
            execute_program();
        }
    }
    return 0;
}
```

`execute_program()` loops calling `parse_statement()` until `TOK_EOF`, maintaining the global scope.

---

## 9. Error Handling

```c
void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, " at line %d\n", current_line);
    va_end(args);
    exit(1);
}
```

---

## 10. Line Count Estimate

| Component | Lines |
|-----------|-------|
| Value + ArrayData + refcount helpers | ~60 |
| Lexer (tokenizer + semicolon inference) | ~150 |
| Parser — primary expressions | ~80 |
| Parser — binary ops + index chain | ~80 |
| Parser — statements (if, while, function, return) | ~120 |
| Parser — lvalue/assignment | ~60 |
| Parser — block + program | ~40 |
| Scope + function table | ~80 |
| Built-in functions + print_value | ~100 |
| Main / REPL | ~40 |
| Error handling | ~20 |
| **Total** | **~830** |

Well under 1000 lines.

---

## 11. Implementation Order

1. `Value` struct + `ArrayData` + retain/release/COW — test with ad-hoc calls
2. Lexer — test by printing token stream
3. Scope + function table — test with hardcoded values
4. Expression parser (numbers, identifiers, binary ops, unary) — test eval
5. Array expressions (literals, index, multi-index) — test creation + access
6. Statements (assignment, if, while, blocks) — test control flow
7. Functions (definition + call, return) — test recursion
8. Built-in functions (print, len, input) — test I/O
9. Main/REPL — wire everything together
