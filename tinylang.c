/* tinylang.c — VM with goto-to-switch dispatch + slot-indexed variables
 * Removed: compact types, FFI, assert, type(), 0b/0123 / OC_SLICE_ASSIGN
 * Kept: TCO, COW+refcounting, push optimization, short-circuit && ||,
 *        0x hex, slices, input, REPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <setjmp.h>
#include <unistd.h>
#include <glob.h>
#include <termios.h>
#ifdef READLINE
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

typedef enum { VAL_NUM, VAL_ARR } Type;
typedef struct Arr { int refcount, len, cap; struct Value *val; int is_slice; unsigned int hash_cache; struct Arr *parent; int is_string; } Arr;
typedef struct Value {
    Type type;
    double num;
    Arr *arr;
} Value;

typedef enum {
    T_EOF, T_NUM, T_ID, T_STR, T_NIL,
    T_LP, T_RP, T_LB, T_RB, T_LC, T_RC, T_CM, T_SEMI,
    T_PL, T_MI, T_ST, T_SL, T_PC,
    T_AM, T_PI, T_CA, T_SHL, T_SHR, T_BN, T_TILDE,
    T_ASSIGN, T_PL_ASSIGN, T_MI_ASSIGN, T_ST_ASSIGN, T_SL_ASSIGN,
    T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_HASH, T_COLON,
    T_NL, T_IF, T_ELIF, T_ELSE, T_WH, T_FN, T_RT, T_INCLUDE,
    T_AND, T_OR,
} TK;

typedef struct { TK t; double n; char *s; int l; } Tok;
typedef struct { char **n; Value *v; int c, m; } Scp;

typedef enum {
    OC_NUM, OC_NIL, OC_STR, OC_MAKE_ARR,
    OC_VAR,
    OC_VAR_SLOT, OC_STORE_SLOT,
    OC_OP, OC_ADD_NUM, OC_SUB_NUM, OC_MUL_NUM, OC_DIV_NUM, OC_UNARY, OC_INDEX,
    OC_CALL, OC_TCO,
    OC_JZ, OC_JMP, OC_RET, OC_POP,
    OC_LVALS, OC_PUSH, OC_LVALS_PUSH, OC_SLICE,
    OC_PRINT, OC_INPUT,
    OC_DUP, OC_JNZ,
    OC_PUSH_ALL,
    OC_SLICE_INPLACE,
    OC_DESTRUCTURE,
    OC_MUTATE_NUM,
    OC_CLEAR_SLOT,
    OC_PROFILE,
    OC_END,
} OC;

typedef struct Code { struct Instr *code; int len, cap; } Code;
typedef struct Instr {
    OC op; int a, b; double num; Arr *arr; char *name; int line; char *file;
} Instr;

Tok *ts; int tc, tp;
Scp *cs;
int rf; Value rv;
Value istk[4096]; int isp;
int cur_fi;

static int show_bytecode = 0;
static int profile_flag = 0;

/* Profile counters — incremented by OC_PROFILE opcodes (only in bytecode
 * when --profile is active; zero runtime overhead otherwise) */
static long profile_user_calls = 0;
static long profile_builtin_calls = 0;
static long profile_tco_calls = 0;
static long profile_cow_copies = 0;
static long profile_cow_bytes = 0;

static int saved_argc;
static char **saved_argv;

static char *include_dir;
char *comp_file; int comp_line;
char *err_file; int err_line;
typedef struct { int fi; int line; char *file; } CallFrame;
CallFrame call_stack[128]; int call_depth = -1;

typedef enum { T_UNKNOWN = 0, T_NUM_TYPE = 1, T_ARR_TYPE = 2, T_STR_TYPE = 3 } ExprType;
typedef Value (*NativeFn)(int ac, Value *args);
typedef struct { char *n; int a; int nvars, *p_slots; ExprType ret_type; Code *code; Value *def_vals; int is_builtin; NativeFn native_fn; int is_inlinable; int inline_pat; double inline_num; Arr *inline_arr; int inline_obj_slot; int inline_field_slot; double inline_index; } Fn;
Fn *fs; int fc, fm;

jmp_buf repl_jmp; int repl_catching;

/* Compile-time variable tracking: name → slot index + type */
static char **comp_vars;
static ExprType *comp_types;
static int comp_vc, comp_vm;

/* Compile-time return type tracking for current function */
static ExprType fn_ret_type;
static int fn_ret_seen;

/* Track the type of the most recently compiled expression (for numeric opcode optimization) */
static ExprType comp_last_type;

/* Hint for move-semantics optimization: if >= 0, the next function call's
 * first argument is expected to be this slot, and OC_CLEAR_SLOT should be
 * emitted to avoid unnecessary COW deep copies. Set by comp_stmt when
 * compiling x = f(x, ...). */
static int move_slot_hint = -1;

void die(const char *f, ...);

static void set_var_type(int slot, ExprType t) {
    if (comp_types[slot] != T_UNKNOWN && t != T_UNKNOWN && comp_types[slot] != t)
        die("type mismatch");
    if (t != T_UNKNOWN)
        comp_types[slot] = t;
}

/* ─── Value helpers ─── */

Value vnum(double n) { return (Value){ .type = VAL_NUM, .num = n }; }
double val_num(Value v) { return v.num; }
#define nilv() ((Value){ .type = VAL_ARR })

/* ─── Array runtime ─── */

Arr *aalloc(int c) {
    Arr *a = calloc(1, sizeof(Arr)); a->refcount = 1;
    a->cap = c; a->val = c ? calloc(c, sizeof(Value)) : NULL;
    a->is_slice = 0; a->parent = NULL; a->is_string = 0;
    return a;
}
void aretain(Arr *a) { if (a) a->refcount++; }
void arelease(Arr *a) {
    if (!a) return;
    if (--a->refcount > 0) return;
    if (a->is_slice) {
        /* View: val belongs to parent, sub-arrays belong to parent */
        arelease(a->parent);
        free(a);
    } else {
        for (int i = 0; i < a->len; i++)
            if (a->val[i].type == VAL_ARR) arelease(a->val[i].arr);
        free(a->val); free(a);
    }
}
Arr *adeep_copy(Arr *s) {
    if (!s) return NULL;
    int n = s->len;
    Arr *d = aalloc(n); d->len = n;
    for (int i = 0; i < n; i++) {
        d->val[i] = s->val[i];
        if (d->val[i].type == VAL_ARR) aretain(d->val[i].arr);
    }
    return d;
}
void amake_uniq(Value *v) {
    if (v->type != VAL_ARR || !v->arr) return;
    if (v->arr->is_slice) {
        /* View: always flatten to owned copy before mutation */
        Arr *old = v->arr;
        v->arr = adeep_copy(old);
        arelease(old);
    } else if (v->arr->refcount > 1) {
        Arr *old = v->arr;
        v->arr = adeep_copy(old);
        arelease(old);
    }
}
void vassign(Value *d, Value s) {
    if (d->type == VAL_ARR) arelease(d->arr);
    *d = s;
    if (s.type == VAL_ARR) aretain(s.arr);
}

static Arr *cstr_to_arr(const char *s, int len) {
    Arr *a = aalloc(len); a->len = len; a->is_string = 1;
    for (int i = 0; i < len; i++) a->val[i] = vnum((double)(unsigned char)s[i]);
    return a;
}

static void slice_bounds(int len, int step, Value start_v, Value stop_v,
                         int *start_out, int *stop_out, int *count_out)
{
    int start, stop;
    if (start_v.type == VAL_ARR && !start_v.arr) start = (step > 0) ? 0 : len - 1;
    else {
        start = (int)val_num(start_v);
        if (start < 0) start += len;
        if (step > 0) { if (start < 0) start = 0; if (start > len) start = len; }
        else { if (start < 0) start = 0; if (start >= len) start = len - 1; }
    }
    if (stop_v.type == VAL_ARR && !stop_v.arr) stop = (step > 0) ? len : -1;
    else {
        stop = (int)val_num(stop_v);
        if (stop < 0) stop += len;
        if (step > 0) { if (stop < 0) stop = 0; if (stop > len) stop = len; }
        else { if (stop < -1) stop = -1; if (stop > len) stop = len; }
    }
    int count;
    if (step > 0) { if (start >= stop) count = 0; else count = (stop - start + step - 1) / step; }
    else { if (start <= stop) count = 0; else count = (start - stop + (-step) - 1) / (-step); }
    *start_out = start; *stop_out = stop; *count_out = count;
}

/* Returns 1 if the array was created as a string literal or by string
 * operations (input, string+string, string+number, etc.)  Arrays built
 * from [...] literals or numeric operations do NOT have this flag.
 * This distinguishes text strings from generic byte arrays used as
 * multi-index chains.
 */
int is_string_arr(Arr *a) {
    return a && a->is_string;
}

/* ─── FNV-1a hash (Git's strhash algorithm) ─── */
unsigned int fnv1a_hash(Arr *a) {
    unsigned int hash = 0x811c9dc5u; /* FNV32_BASE */
    for (int i = 0; i < a->len; i++) {
        unsigned int c = (unsigned int)val_num(a->val[i]);
        hash = (hash * 0x01000193u) ^ c; /* FNV32_PRIME */
    }
    return hash;
}
unsigned int get_arr_hash(Arr *a) {
    if (!a->hash_cache)
        a->hash_cache = fnv1a_hash(a);
    return a->hash_cache;
}

/* Write a number's decimal representation into buf, return length */
static int num_to_buf(char *buf, double d) {
    if (d == (double)(int64_t)d)
        return snprintf(buf, 64, "%lld", (int64_t)d);
    else
        return snprintf(buf, 64, "%g", d);
}

/* Optimized string + number concat: writes number digits directly into result,
 * avoiding an intermediate Arr allocation. */
Arr *strcat_num(Arr *la, double d) {
    char buf[64];
    int rn = num_to_buf(buf, d);
    int ln = la ? la->len : 0;
    Arr *a = aalloc(ln + rn); a->len = ln + rn;
    a->is_string = la ? la->is_string : 1;
    for (int i = 0; i < ln; i++) {
        a->val[i] = la->val[i];
        /* is_string_arr guarantees all elements are VAL_NUM, but retain for safety */
        if (a->val[i].type == VAL_ARR) aretain(a->val[i].arr);
    }
    for (int i = 0; i < rn; i++)
        a->val[ln + i] = vnum((double)(unsigned char)buf[i]);
    return a;
}

/* Optimized number + string concat: same as strcat_num but number comes first. */
Arr *num_strcat(double d, Arr *ra) {
    char buf[64];
    int ln = num_to_buf(buf, d);
    int rn = ra ? ra->len : 0;
    Arr *a = aalloc(ln + rn); a->len = ln + rn;
    a->is_string = ra ? ra->is_string : 1;
    for (int i = 0; i < ln; i++)
        a->val[i] = vnum((double)(unsigned char)buf[i]);
    for (int i = 0; i < rn; i++) {
        a->val[ln + i] = ra->val[i];
        if (a->val[ln + i].type == VAL_ARR) aretain(a->val[ln + i].arr);
    }
    return a;
}

void print_val(Value v) {
    if (v.type == VAL_NUM) {
        double d = v.num;
        if (d == (double)(int64_t)d) printf("%lld", (int64_t)d);
        else printf("%g", d);
    } else if (v.type == VAL_ARR) {
        Arr *a = v.arr;
        if (is_string_arr(a)) {
            for (int i = 0; i < a->len; i++) putchar((int)val_num(a->val[i]));
            return;
        }
        putchar('[');
        if (a) for (int i = 0; i < a->len; i++) {
            if (i) printf(", "); print_val(a->val[i]);
        }
        putchar(']');
    }
}
int truthy(Value v) {
    if (v.type == VAL_NUM) return v.num != 0.0;
    if (v.type == VAL_ARR) return v.arr && v.arr->len > 0;
    return 0;
}
int veq(Value a, Value b) {
    if (a.type != b.type) return 0;
    if (a.type == VAL_NUM) return a.num == b.num;
    if (!a.arr && !b.arr) return 1;
    if (!a.arr || !b.arr || a.arr->len != b.arr->len) return 0;
    for (int i = 0; i < a.arr->len; i++)
        if (!veq(a.arr->val[i], b.arr->val[i])) return 0;
    return 1;
}

void die(const char *f, ...);

/* ─── Operators ─── */

Value apply(int op, Value l, Value r) {
    double ld = val_num(l), rd = val_num(r);
    int lf = l.type == VAL_NUM, rf = r.type == VAL_NUM;
    switch (op) {
        case T_PL:
            if (lf && rf) return vnum(ld + rd);
            if (l.type == VAL_ARR && r.type == VAL_ARR) {
                Arr *la = l.arr, *ra = r.arr;
                int ln = la ? la->len : 0, rn = ra ? ra->len : 0;
                Arr *a = aalloc(ln + rn); a->len = ln + rn;
    a->is_string = (la ? la->is_string : 0) || (ra ? ra->is_string : 0);
                for (int i = 0; i < ln; i++) { a->val[i] = la->val[i];
                    if (a->val[i].type == VAL_ARR) aretain(a->val[i].arr); }
                for (int i = 0; i < rn; i++) { a->val[ln + i] = ra->val[i];
                    if (a->val[ln + i].type == VAL_ARR) aretain(a->val[ln + i].arr); }
                return (Value){ .type = VAL_ARR, .arr = a };
            }
            if (l.type == VAL_ARR && rf && is_string_arr(l.arr)) {
                /* string + number: write number digits directly into result,
                 * no intermediate Arr allocation. */
                return (Value){ .type = VAL_ARR, .arr = strcat_num(l.arr, rd) };
            }
            if (lf && r.type == VAL_ARR && is_string_arr(r.arr)) {
                /* number + string: same direct-to-result optimization */
                return (Value){ .type = VAL_ARR, .arr = num_strcat(ld, r.arr) };
            }
            die("'+' type mismatch");
        case T_MI:
            if (!lf || !rf) die("'-' requires numbers");
            return vnum(ld - rd);
        case T_ST:
            if (lf && rf) return vnum(ld * rd);
            if (l.type == VAL_ARR && rf) {
                Arr *la = l.arr; int bl = la ? la->len : 0, n = (int)rd;
                if (n <= 0) return nilv();
                Arr *a = aalloc(bl * n); a->len = bl * n;
                a->is_string = la ? la->is_string : 0;
                for (int i = 0; i < bl; i++) { a->val[i] = la->val[i];
                    if (a->val[i].type == VAL_ARR) aretain(a->val[i].arr); }
                for (int i = 1; i < n; i++)
                    for (int j = 0; j < bl; j++) { a->val[i * bl + j] = la->val[j];
                        if (a->val[i * bl + j].type == VAL_ARR) aretain(a->val[i*bl+j].arr); }
                return (Value){ .type = VAL_ARR, .arr = a };
            } die("'*' type mismatch");
        case T_SL:
            if (!lf || !rf) die("'/' requires numbers");
            if (rd == 0) die("division by zero");
            return vnum(ld / rd);
        case T_PC:
            if (!lf || !rf) die("'%%' requires numbers");
            if (rd == 0) die("modulo by zero");
            return vnum(fmod(ld, rd));
        case T_EQ: return veq(l, r) ? vnum(1) : nilv();
        case T_NE: return veq(l, r) ? nilv() : vnum(1);
        case T_LT:
            if (!lf || !rf) die("'<' requires numbers");
            return ld < rd ? vnum(1) : nilv();
        case T_GT:
            if (!lf || !rf) die("'>' requires numbers");
            return ld > rd ? vnum(1) : nilv();
        case T_LE:
            if (!lf || !rf) die("'<=' requires numbers");
            return ld <= rd ? vnum(1) : nilv();
        case T_GE:
            if (!lf || !rf) die("'>=' requires numbers");
            return ld >= rd ? vnum(1) : nilv();
        case T_SHL:
            if (!lf || !rf) die("'<<' requires numbers");
            return vnum((double)((int64_t)ld << (int32_t)rd));
        case T_SHR:
            if (!lf || !rf) die("'>>' requires numbers");
            return vnum((double)((int64_t)ld >> (int32_t)rd));
        case T_AM:
            if (!lf || !rf) die("bitwise & requires numbers");
            return vnum((double)((int64_t)ld & (int64_t)rd));
        case T_PI:
            if (!lf || !rf) die("bitwise | requires numbers");
            return vnum((double)((int64_t)ld | (int64_t)rd));
        case T_CA:
            if (!lf || !rf) die("bitwise ^ requires numbers");
            return vnum((double)((int64_t)ld ^ (int64_t)rd));
        default: die("unknown op");
    } return nilv();
}

/* ─── Error handling ─── */

static ExprType peek_expr_type(int *pn);

void die(const char *f, ...) {
    va_list ap; va_start(ap, f);
    if (repl_catching) {
        vfprintf(stderr, f, ap); fputc('\n', stderr);
        for (int i = 0; i <= call_depth; i++) {
            if (call_stack[i].fi >= 0) {
                fprintf(stderr, "in ");
                if (call_stack[i].file) fprintf(stderr, "%s:%d: ", call_stack[i].file, call_stack[i].line);
                fprintf(stderr, "%s()\n", fs[call_stack[i].fi].n);
            } else if (call_stack[i].file)
                fprintf(stderr, "in %s:%d\n", call_stack[i].file, call_stack[i].line);
            else fprintf(stderr, "in <top-level>\n");
        }
        if (cur_fi >= 0) {
            fprintf(stderr, "in ");
            if (err_file) fprintf(stderr, "%s:%d: ", err_file, err_line);
            fprintf(stderr, "%s()\n", fs[cur_fi].n);
        } else if (err_file) fprintf(stderr, "in %s:%d\n", err_file, err_line);
        else fprintf(stderr, "in <top-level>\n");
        va_end(ap); longjmp(repl_jmp, 1);
    }
    vfprintf(stderr, f, ap); fputc('\n', stderr);
    for (int i = 0; i <= call_depth; i++) {
        if (call_stack[i].fi >= 0) {
            fprintf(stderr, "in ");
            if (call_stack[i].file) fprintf(stderr, "%s:%d: ", call_stack[i].file, call_stack[i].line);
            fprintf(stderr, "%s()\n", fs[call_stack[i].fi].n);
        } else if (call_stack[i].file)
            fprintf(stderr, "in %s:%d\n", call_stack[i].file, call_stack[i].line);
        else fprintf(stderr, "in <top-level>\n");
    }
    if (cur_fi >= 0) {
        fprintf(stderr, "in ");
        if (err_file) fprintf(stderr, "%s:%d: ", err_file, err_line);
        fprintf(stderr, "%s()\n", fs[cur_fi].n);
    } else if (err_file) fprintf(stderr, "in %s:%d\n", err_file, err_line);
    else fprintf(stderr, "in <top-level>\n");
    va_end(ap); exit(1);
}

/* ─── Scope ─── */

Scp *snew(void) { return calloc(1, sizeof(Scp)); }
Scp *snew_sized(int n) {
    Scp *s = calloc(1, sizeof(Scp));
    if (n > 0) { s->v = calloc(n, sizeof(Value)); s->n = calloc(n, sizeof(char*)); }
    s->c = n; s->m = n;
    return s;
}
void sfree(Scp *s) {
    for (int i = 0; i < s->c; i++) {
        if (s->v[i].type == VAL_ARR) arelease(s->v[i].arr);
        if (s->n[i]) free(s->n[i]);
    }
    free(s->n); free(s->v); free(s);
}
Value sget(Scp *s, const char *n) {
    for (int i = 0; i < s->c; i++) if (s->n[i] && !strcmp(s->n[i], n)) return s->v[i];
    die("undefined '%s'", n); return nilv();
}
int ffind(const char *n) { for (int i = 0; i < fc; i++) if (!strcmp(fs[i].n, n)) return i; return -1; }

/* ─── Bytecode helpers ─── */

void emit(Code *c, Instr ins) {
    if (c->len >= c->cap) { c->cap = c->cap ? c->cap*2 : 128;
        c->code = realloc(c->code, c->cap * sizeof(Instr)); }
    ins.line = comp_line; ins.file = comp_file ? strdup(comp_file) : NULL;
    c->code[c->len++] = ins;
}
Code *new_code(void) { return calloc(1, sizeof(Code)); }
void code_free(Code *c) {
    if (!c) return;
    for (int i = 0; i < c->len; i++) {
        free(c->code[i].name); free(c->code[i].file);
        if (c->code[i].arr) arelease(c->code[i].arr);
    }
    free(c->code); free(c);
}
char *readf(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *b = malloc(sz + 1); fread(b, 1, sz, f); b[sz] = 0;
    fclose(f); return b;
}
int var_find(const char *n) {
    for (int i = 0; i < comp_vc; i++)
        if (!strcmp(comp_vars[i], n)) return i;
    return -1;
}
int var_add(const char *n) {
    int s = comp_vc++;
    if (comp_vc > comp_vm) { comp_vm = comp_vm ? comp_vm*2 : 16;
        comp_vars = realloc(comp_vars, comp_vm * sizeof(char*));
        comp_types = realloc(comp_types, comp_vm * sizeof(ExprType)); }
    comp_vars[s] = strdup(n);
    comp_types[s] = T_UNKNOWN;
    return s;
}

/* ─── Compiler ─── */

void comp_stmt(Code *c);
void comp_expr(Code *c);
void comp_prim(Code *c);
void comp_block(Code *c);

static int op_prec(TK t) {
    switch (t) {
        case T_ST: case T_SL: case T_PC: return 9;
        case T_PL: case T_MI: return 8;
        case T_SHL: case T_SHR: return 7;
        case T_LT: case T_GT: case T_LE: case T_GE: return 6;
        case T_EQ: case T_NE: return 5;
        case T_AM: return 4;
        case T_CA: return 3;
        case T_PI: return 2;
        case T_AND: return 1;
        case T_OR:  return 0;
        default: return 0;
    }
}
static int is_binary_op(TK t) {
    switch (t) {
        case T_PL: case T_MI: case T_ST: case T_SL: case T_PC:
        case T_AM: case T_PI: case T_CA: case T_SHL: case T_SHR:
        case T_EQ: case T_NE:
        case T_LT: case T_GT: case T_LE: case T_GE:
        case T_AND: case T_OR:
            return 1;
        default: return 0;
    }
}

/* ─── Lexer ─── */

const char *src; int sl;
static int pc(void) { return (unsigned char)*src; }
static int ac(void) { int c = (unsigned char)*src++; if (c == '\n') sl++; return c; }

void lex(const char *s) {
    src = s; sl = 1; tc = 0; tp = 0;
    int m = 8192; ts = malloc(m * sizeof(Tok));
    for (;;) {
        while (pc() == ' ' || pc() == '\t') ac();
        int c = pc(); Tok tk = { .l = sl };
        if (c == '\n') { ac(); tk.t = T_NL; goto em; }
        if (c == 0) { tk.t = T_EOF; goto em; }
        if (c == '/' && src[1] == '/') { while (pc() && pc() != '\n') ac(); continue; }
        if (isdigit(c) || (c == '.' && isdigit(src[1]))) {
            char b[64]; int i = 0;
            if (c == '0' && (src[1] == 'x' || src[1] == 'X')) {
                ac(); ac();
                while (isxdigit(pc())) { if (i >= 62) die("hex literal too long"); b[i++] = ac(); }
                if (i == 0) die("invalid hex literal");
                b[i] = 0; tk.t = T_NUM; tk.n = (double)strtoull(b, NULL, 16); goto em;
            }
            if (c == '.') { b[i++] = c; ac(); c = pc(); }
            while (isdigit(c)) { b[i++] = c; ac(); c = pc(); }
            if (c == '.') { b[i++] = c; ac(); c = pc(); while (isdigit(c)) { b[i++] = c; ac(); c = pc(); } }
            b[i] = 0; tk.t = T_NUM; tk.n = atof(b); goto em;
        }
        if (isalpha(c) || c == '_') {
            char b[64]; int i = 0;
            while (isalnum(c) || c == '_') { b[i++] = c; ac(); c = pc(); }
            b[i] = 0; tk.t = T_ID; tk.s = strdup(b);
            if (!strcmp(b,"nil")){tk.t=T_NIL;free(tk.s);}
            else if (!strcmp(b,"if")){tk.t=T_IF;free(tk.s);}
            else if (!strcmp(b,"elif")){tk.t=T_ELIF;free(tk.s);}
            else if (!strcmp(b,"else")){tk.t=T_ELSE;free(tk.s);}
            else if (!strcmp(b,"for")){tk.t=T_WH;free(tk.s);}
            else if (!strcmp(b,"fun")){tk.t=T_FN;free(tk.s);}
            else if (!strcmp(b,"ret")){tk.t=T_RT;free(tk.s);}
            else if (!strcmp(b,"include")){tk.t=T_INCLUDE;free(tk.s);}
            goto em;
        }
        if (c == '"') {
            ac(); char b[8192]; int i = 0;
            for (;;) {
                c = ac(); if (c == '"') break;
                if (c == '\\') {
                    c = ac();
                    switch(c) {
                        case 'n': b[i++]=10; break; case 't': b[i++]=9; break;
                        case 'r': b[i++]=13; break; case '0': b[i++]=0; break;
                        case '\\': b[i++]='\\'; break; case '"': b[i++]='"'; break;
                        case 'x': { char h[3]={ac(),ac(),0}; b[i++]=(char)strtol(h,0,16); break; }
                        default: b[i++]=c;
                    }
                } else { if (c==0||c=='\n') die("unterminated string"); b[i++]=c; }
            }
            Arr *a = aalloc(i); a->len = i; a->is_string = 1;
            for (int j = 0; j < i; j++) a->val[j] = vnum((double)(unsigned char)b[j]);
            tk.t = T_STR; tk.s = (char*)a; goto em;
        }
        ac();
        switch (c) {
            case '(': tk.t=T_LP; break; case ')': tk.t=T_RP; break;
            case '[': tk.t=T_LB; break; case ']': tk.t=T_RB; break;
            case '{': tk.t=T_LC; break; case '}': tk.t=T_RC; break;
            case ',': tk.t=T_CM; break; case ';': tk.t=T_SEMI; break;
            case '+': if(pc()=='='){ac();tk.t=T_PL_ASSIGN;}else tk.t=T_PL; break;
            case '-': if(pc()=='='){ac();tk.t=T_MI_ASSIGN;}else tk.t=T_MI; break;
            case '*': if(pc()=='='){ac();tk.t=T_ST_ASSIGN;}else tk.t=T_ST; break;
            case '/': if(pc()=='='){ac();tk.t=T_SL_ASSIGN;}else tk.t=T_SL; break;
            case '%': tk.t=T_PC; break;
            case '&': if (pc()=='&'){ac();tk.t=T_AND;}else tk.t=T_AM; break;
            case '|': if (pc()=='|'){ac();tk.t=T_OR;}else tk.t=T_PI; break;
            case '^': tk.t=T_CA; break;
            case '~': tk.t=T_TILDE; break;
            case '#': tk.t=T_HASH; break; case ':': tk.t=T_COLON; break;
            case '!': if (pc()=='='){ac();tk.t=T_NE;}else tk.t=T_BN; break;
            case '=': if (pc()=='='){ac();tk.t=T_EQ;}else tk.t=T_ASSIGN; break;
            case '<': if(pc()=='<'){ac();tk.t=T_SHL;}else if(pc()=='='){ac();tk.t=T_LE;}else tk.t=T_LT; break;
            case '>': if(pc()=='>'){ac();tk.t=T_SHR;}else if(pc()=='='){ac();tk.t=T_GE;}else tk.t=T_GT; break;
            default: { char m[2]={c,0}; die("unexpected '%s'",m); }
        }
        em: if (tc>=m){m*=2;ts=realloc(ts,m*sizeof(Tok));} ts[tc++]=tk; if (tk.t==T_EOF) break;
    }
}

void comp_prim(Code *c) {
    while (ts[tp].t == T_NL) tp++;
    Tok t = ts[tp]; comp_line = t.l; err_line = t.l; err_file = comp_file;
    switch (t.t) {
        case T_NUM: tp++; emit(c, (Instr){OC_NUM, 0, 0, .num = t.n}); comp_last_type = T_NUM_TYPE; break;
        case T_NIL: tp++; emit(c, (Instr){OC_NIL, 0, 0, .num = 0}); comp_last_type = T_ARR_TYPE; break;
        case T_STR: { tp++; Arr *o = (Arr*)t.s; emit(c, (Instr){OC_STR, 0, 0, .arr = o}); comp_last_type = T_STR_TYPE; break; }
        case T_ID: {
            char *nm = strdup(t.s); tp++;
            if (ts[tp].t == T_LP) {
                free(nm); tp++;
                int ac = 0;
                if (ts[tp].t != T_RP) {
                    /* Check for move-semantics optimization on first arg */
                    int move_first = 0;
                    if (move_slot_hint >= 0 && ts[tp].t == T_ID &&
                        var_find(ts[tp].s) == move_slot_hint &&
                        (ts[tp+1].t == T_CM || ts[tp+1].t == T_RP)) {
                        move_first = 1;
                    }
                    do {
                        if (ac == 0 && move_first) {
                            int slot = move_slot_hint;
                            comp_expr(c);
                            emit(c, (Instr){OC_CLEAR_SLOT, slot, 0, .num = 0});
                            move_slot_hint = -1;
                        } else {
                            comp_expr(c);
                        }
                        ac++;
                    } while (ts[tp].t == T_CM && (tp++, 1));
                }
                tp++;
                if (!strcmp(t.s, "print")) { if (ac < 1) die("print needs 1 arg"); emit(c, (Instr){OC_PRINT, 0, 0, .num = 0}); }
                else if (!strcmp(t.s, "input")) emit(c, (Instr){OC_INPUT, 0, 0, .num = 0});
                else if (!strcmp(t.s, "thisfile")) {
                    if (ac != 0) die("thisfile expects 0 arguments");
                    int n = comp_file ? strlen(comp_file) : 0;
                    Arr *a = cstr_to_arr(comp_file, n);
                    emit(c, (Instr){OC_STR, 0, 0, .arr = a});
                } else {
                    int fi = ffind(t.s); if (fi < 0) die("undefined function '%s'", t.s);
                    Fn *fn = &fs[fi];
                    comp_last_type = fn->ret_type;
                    if (fn->is_inlinable && ac == fn->a) {
                        /* Inline the function body instead of calling it */
                        switch (fn->inline_pat) {
                            case 0: /* CONST_NUM */
                                emit(c, (Instr){OC_NUM, 0, 0, .num = fn->inline_num});
                                break;
                            case 1: /* CONST_NIL */
                                emit(c, (Instr){OC_NIL, 0, 0, .num = 0});
                                break;
                            case 2: /* CONST_STR */
                                emit(c, (Instr){OC_STR, 0, 0, .arr = fn->inline_arr});
                                break;
                            case 3: /* ACCESSOR_CONST */
                                emit(c, (Instr){OC_NUM, 0, 0, .num = fn->inline_index});
                                emit(c, (Instr){OC_INDEX, 0, 0, .num = 0});
                                break;
                            case 4: /* ACCESSOR_PARAM */
                                emit(c, (Instr){OC_INDEX, 0, 0, .num = 0});
                                break;
                            case 5: /* ACCESSOR_CONST_FLOOR */
                                emit(c, (Instr){OC_NUM, 0, 0, .num = fn->inline_index});
                                emit(c, (Instr){OC_INDEX, 0, 0, .num = 0});
                                { int ffi = ffind("floor");
                                  if (ffi >= 0) {
                                      if (profile_flag) emit(c, (Instr){OC_PROFILE, 0, 0, .num = 1.0});
                                      emit(c, (Instr){OC_CALL, ffi, 1, .num = 0});
                                  } }
                                break;
                            case 6: /* ACCESSOR_PARAM_FLOOR */
                                emit(c, (Instr){OC_INDEX, 0, 0, .num = 0});
                                { int ffi = ffind("floor");
                                  if (ffi >= 0) {
                                      if (profile_flag) emit(c, (Instr){OC_PROFILE, 0, 0, .num = 1.0});
                                      emit(c, (Instr){OC_CALL, ffi, 1, .num = 0});
                                  } }
                                break;
                        }
                    } else {
                        if (profile_flag) {
                            emit(c, (Instr){OC_PROFILE, 0, 0, .num = fs[fi].is_builtin ? 1.0 : 0.0});
                        }
                        emit(c, (Instr){OC_CALL, fi, ac, .num = 0});
                    }
                }
            } else {
                /* Variable read — use slot index in function bodies, name at top-level */
                int slot = var_find(nm);
                if (slot >= 0) {
                    comp_last_type = comp_types[slot];
                    emit(c, (Instr){OC_VAR_SLOT, slot, 0, .num = 0});
                } else if (cur_fi >= 0) {
                    die("undefined variable '%s'", nm);
                } else {
                    comp_last_type = T_UNKNOWN;
                    emit(c, (Instr){OC_VAR, 0, 0, .name = nm});
                    nm = NULL;
                }
                free(nm);
            }
            break;
        }
        case T_LB: {
            tp++;
            if (ts[tp].t == T_RB) { tp++; emit(c, (Instr){OC_NIL, 0, 0, .num = 0}); comp_last_type = T_ARR_TYPE; break; }
            int n = 0;
            do { comp_expr(c); n++; } while (ts[tp].t == T_CM && (tp++, 1));
            if (ts[tp].t != T_RB) die("expected ]"); tp++;
            emit(c, (Instr){OC_MAKE_ARR, n, 0, .num = 0}); comp_last_type = T_ARR_TYPE; break;
        }
        case T_LP: { tp++; comp_expr(c); if (ts[tp].t != T_RP) die("expected )"); tp++; break; }
        case T_BN: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_BN, 0, .num = 0}); comp_last_type = T_UNKNOWN; break;
        case T_MI: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_MI, 0, .num = 0}); comp_last_type = T_NUM_TYPE; break;
        case T_HASH: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_HASH, 0, .num = 0}); comp_last_type = T_NUM_TYPE; break;
        case T_TILDE: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_TILDE, 0, .num = 0}); comp_last_type = T_NUM_TYPE; break;
        default: die("unexpected token at line %d", t.l);
    }
    int had_index = 0;
    while (ts[tp].t == T_LB) {
        had_index = 1;
        tp++;
        int is_slice = 0;
        if (ts[tp].t == T_COLON) is_slice = 1;
        else if (ts[tp].t != T_RB) {
            int depth = 0;
            for (int p = tp; p < tc; p++) {
                if (depth == 0 && (ts[p].t == T_RB || ts[p].t == T_CM)) break;
                if (depth == 0 && ts[p].t == T_COLON) { is_slice = 1; break; }
                if (ts[p].t == T_LB || ts[p].t == T_LP) depth++;
                if (ts[p].t == T_RB || ts[p].t == T_RP) depth--;
            }
        }
        if (is_slice) {
            if (ts[tp].t != T_COLON && ts[tp].t != T_RB) comp_expr(c);
            else emit(c, (Instr){OC_NIL, 0, 0, .num = 0});
            if (ts[tp].t != T_COLON) die("expected ':' in slice"); tp++;
            if (ts[tp].t != T_COLON && ts[tp].t != T_RB) comp_expr(c);
            else emit(c, (Instr){OC_NIL, 0, 0, .num = 0});
            if (ts[tp].t == T_COLON) {
                tp++;
                if (ts[tp].t != T_RB) comp_expr(c);
                else emit(c, (Instr){OC_NUM, 0, 0, .num = 1.0});
            } else emit(c, (Instr){OC_NUM, 0, 0, .num = 1.0});
            if (ts[tp].t != T_RB) die("expected ]"); tp++;
            emit(c, (Instr){OC_SLICE, 0, 0, .num = 0});
        } else {
            do { comp_expr(c); emit(c, (Instr){OC_INDEX, 0, 0, .num = 0}); } while (ts[tp].t == T_CM && (tp++, 1));
            if (ts[tp].t != T_RB) die("expected ]"); tp++;
        }
    }
    if (had_index) comp_last_type = T_UNKNOWN;
}

static void comp_expr_prec(Code *c, int min_prec) {
    comp_prim(c);
    while (is_binary_op(ts[tp].t) && op_prec(ts[tp].t) >= min_prec) {
        int op = ts[tp].t; tp++;
        if (op == T_AND || op == T_OR) {
            emit(c, (Instr){ OC_DUP, 0, 0, .num = 0 });
            int jmp = c->len;
            emit(c, (Instr){ op == T_AND ? OC_JZ : OC_JNZ, 0, 0, .num = 0 });
            emit(c, (Instr){ OC_POP, 0, 0, .num = 0 });
            comp_expr_prec(c, op_prec(op) + 1);
            c->code[jmp].a = c->len;
        } else {
            ExprType left_type = comp_last_type;
            comp_expr_prec(c, op_prec(op) + 1);
            ExprType right_type = comp_last_type;
            /* Emit dedicated numeric opcodes when both operands are known numbers */
            if (left_type == T_NUM_TYPE && right_type == T_NUM_TYPE) {
                switch (op) {
                    case T_PL: emit(c, (Instr){OC_ADD_NUM, 0, 0, .num = 0}); comp_last_type = T_NUM_TYPE; break;
                    case T_MI: emit(c, (Instr){OC_SUB_NUM, 0, 0, .num = 0}); comp_last_type = T_NUM_TYPE; break;
                    case T_ST: emit(c, (Instr){OC_MUL_NUM, 0, 0, .num = 0}); comp_last_type = T_NUM_TYPE; break;
                    case T_SL: emit(c, (Instr){OC_DIV_NUM, 0, 0, .num = 0}); comp_last_type = T_NUM_TYPE; break;
                    default:
                        emit(c, (Instr){OC_OP, op, 0, .num = 0});
                        comp_last_type = T_NUM_TYPE;
                        break;
                }
            } else {
                emit(c, (Instr){OC_OP, op, 0, .num = 0});
                comp_last_type = T_UNKNOWN;
            }
        }
    }
}
void comp_expr(Code *c) { comp_expr_prec(c, 0); }

void comp_block(Code *c) {
    while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
    if (ts[tp].t != T_LC) return;
    tp++;
    while (ts[tp].t != T_RC && ts[tp].t != T_EOF) {
        while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
        if (ts[tp].t == T_RC || ts[tp].t == T_EOF) break;
        comp_stmt(c);
    }
    if (ts[tp].t == T_RC) tp++;
}

void comp_if(Code *c) {
    int jz[64], np = 0, jmp[64], nj = 0;
    tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    comp_expr(c); jz[np++] = c->len; emit(c, (Instr){OC_JZ, 0, 0, .num = 0});
    comp_block(c); jmp[nj++] = c->len; emit(c, (Instr){OC_JMP, 0, 0, .num = 0});
    while (ts[tp].t == T_ELIF) {
        c->code[jz[np-1]].a = c->len; tp++;
        comp_expr(c); jz[np++] = c->len; emit(c, (Instr){OC_JZ, 0, 0, .num = 0});
        comp_block(c); jmp[nj++] = c->len; emit(c, (Instr){OC_JMP, 0, 0, .num = 0});
    }
    if (ts[tp].t == T_ELSE) { c->code[jz[np-1]].a = c->len; tp++; comp_block(c); }
    else c->code[jz[np-1]].a = c->len;
    for (int i = 0; i < nj; i++) c->code[jmp[i]].a = c->len;
}

void comp_while(Code *c) {
    int loop = c->len; tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    comp_expr(c); int jz = c->len; emit(c, (Instr){OC_JZ, 0, 0, .num = 0});
    comp_block(c); emit(c, (Instr){OC_JMP, loop, 0, .num = 0}); c->code[jz].a = c->len;
}

void comp_return(Code *c) {
    tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    int pn = tp;
    ExprType rt = peek_expr_type(&pn);
    /* Compile first expression */
    comp_expr(c);
    int count = 1;
    /* Check for comma-separated expressions (implicit array return) */
    while (ts[tp].t == T_CM) {
        tp++;
        comp_expr(c);
        count++;
    }
    if (count > 1) {
        emit(c, (Instr){OC_MAKE_ARR, count, 0, .num = 0});
        rt = T_ARR_TYPE;  /* implicit array */
    }
    if (fn_ret_seen && rt != T_UNKNOWN && fn_ret_type != T_UNKNOWN && rt != fn_ret_type)
        die("inconsistent return type");
    if (rt != T_UNKNOWN) { fn_ret_type = rt; fn_ret_seen = 1; }
    emit(c, (Instr){OC_RET, 0, 0, .num = 0});
}

/* Evaluate a compile-time constant expression from the token stream.
 * Returns the Value. Supports: numbers, strings, nil, array literals with
 * constant elements, and unary minus on numbers. Designed for default
 * parameter values. */
Value eval_constant_expr(void) {
    while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
    Tok t = ts[tp];
    /* Unary minus */
    if (t.t == T_MI) {
        tp++;
        Value v = eval_constant_expr();
        if (v.type != VAL_NUM) die("default value: expected number after '-'");
        return vnum(-val_num(v));
    }
    switch (t.t) {
        case T_NUM: tp++; return vnum(t.n);
        case T_NIL: tp++; return nilv();
        case T_STR: {
            tp++;
            Arr *orig = (Arr*)t.s;
            Arr *copy = adeep_copy(orig);
            return (Value){ .type = VAL_ARR, .arr = copy };
        }
        case T_LB: {
            tp++;
            /* Empty array = nil */
            if (ts[tp].t == T_RB) { tp++; return nilv(); }
            /* Evaluate elements */
            Value elems[64];
            int n = 0;
            do {
                if (n >= 64) die("too many elements in default array literal");
                elems[n++] = eval_constant_expr();
            } while (ts[tp].t == T_CM && (tp++, 1));
            if (ts[tp].t != T_RB) die("expected ] in default array literal");
            tp++;
            Arr *a = aalloc(n); a->len = n;
            for (int i = 0; i < n; i++) {
                a->val[i] = elems[i];
                if (elems[i].type == VAL_ARR) aretain(elems[i].arr);
            }
            return (Value){ .type = VAL_ARR, .arr = a };
        }
        default:
            die("default value must be a constant (number, string, nil, or array literal)");
    }
    return nilv();
}

/* Scan a compiled function body for inlinable patterns.
 * Patterns:
 *   0 (CONST_NUM):  zero-arg, returns a constant number
 *   1 (CONST_NIL):  zero-arg, returns nil
 *   2 (CONST_STR):  zero-arg, returns a string/array constant
 *   3 (ACCESSOR_CONST):  one-arg, body is VAR_SLOT(p0) + NUM(idx) + INDEX + RET + END
 *   4 (ACCESSOR_PARAM):  two-arg, body is VAR_SLOT(p0) + VAR_SLOT(p1) + INDEX + RET + END
 * Sets f->is_inlinable and the corresponding inline fields. */
static void detect_inline(Fn *f) {
    Code *body = f->code;
    if (!body || body->len < 3) return;
    int last = body->len - 1;
    if (last < 0 || body->code[last].op != OC_END) return;
    last--;
    if (last < 0 || body->code[last].op != OC_RET) return;
    last--;
    if (last < 0) return;

    /* Pattern 0-2: CONST — zero args, single constant before RET */
    if (f->a == 0 && last == 0) {
        Instr *val = &body->code[last];
        if (val->op == OC_NUM) {
            f->is_inlinable = 1; f->inline_pat = 0;
            f->inline_num = val->num; f->inline_arr = NULL;
            return;
        }
        if (val->op == OC_NIL) {
            f->is_inlinable = 1; f->inline_pat = 1;
            f->inline_num = 0; f->inline_arr = NULL;
            return;
        }
        if (val->op == OC_STR) {
            f->is_inlinable = 1; f->inline_pat = 2;
            f->inline_num = 0; f->inline_arr = val->arr;
            aretain(val->arr);
            return;
        }
        return;
    }

    /* After consuming END and RET, `last` points to the instruction before RET.
     * For a body [VAR_SLOT, NUM, INDEX, RET, END], last = 2 (INDEX).
     * We check instructions at [last-2 .. last] for the 3-instr pattern. */

    /* Pattern 3: ACCESSOR_CONST — one arg, VAR_SLOT + NUM(idx) + INDEX + RET */
    if (f->a == 1 && last >= 2) {
        if (body->code[last-2].op != OC_VAR_SLOT) goto check_const_floor;
        if (body->code[last-1].op != OC_NUM) goto check_const_floor;
        if (body->code[last].op != OC_INDEX) goto check_const_floor;
        int obj_slot = body->code[last-2].a;
        if (obj_slot != f->p_slots[0]) return;
        f->is_inlinable = 1; f->inline_pat = 3;
        f->inline_obj_slot = obj_slot;
        f->inline_index = body->code[last-1].num;
        return;
    }
    check_const_floor:
    /* Pattern 5: ACCESSOR_CONST_FLOOR — like 3 but wraps with floor():
     *   VAR_SLOT + NUM(idx) + INDEX + CALL floor(1) + RET + END */
    if (f->a == 1 && last >= 3) {
        if (body->code[last-3].op != OC_VAR_SLOT) goto check_param;
        if (body->code[last-2].op != OC_NUM) goto check_param;
        if (body->code[last-1].op != OC_INDEX) goto check_param;
        if (body->code[last].op != OC_CALL || body->code[last].b != 1) goto check_param;
        int fi = body->code[last].a;
        if (fi < 0 || !fs[fi].is_builtin || strcmp(fs[fi].n, "floor")) goto check_param;
        int obj_slot = body->code[last-3].a;
        if (obj_slot != f->p_slots[0]) return;
        f->is_inlinable = 1; f->inline_pat = 5;
        f->inline_obj_slot = obj_slot;
        f->inline_index = body->code[last-2].num;
        return;
    }
    check_param:
    /* Pattern 4: ACCESSOR_PARAM — two args, VAR_SLOT(p0) + VAR_SLOT(p1) + INDEX + RET */
    if (f->a == 2 && last >= 2) {
        if (body->code[last-2].op != OC_VAR_SLOT) goto check_param_floor;
        if (body->code[last-1].op != OC_VAR_SLOT) goto check_param_floor;
        if (body->code[last].op != OC_INDEX) goto check_param_floor;
        int obj_slot = body->code[last-2].a;
        int field_slot = body->code[last-1].a;
        if (obj_slot != f->p_slots[0] || field_slot != f->p_slots[1]) return;
        f->is_inlinable = 1; f->inline_pat = 4;
        f->inline_obj_slot = obj_slot;
        f->inline_field_slot = field_slot;
        return;
    }
    check_param_floor:
    /* Pattern 6: ACCESSOR_PARAM_FLOOR — like 4 but wraps with floor():
     *   VAR_SLOT(p0) + VAR_SLOT(p1) + INDEX + CALL floor(1) + RET + END */
    if (f->a == 2 && last >= 3) {
        if (body->code[last-3].op != OC_VAR_SLOT) return;
        if (body->code[last-2].op != OC_VAR_SLOT) return;
        if (body->code[last-1].op != OC_INDEX) return;
        if (body->code[last].op != OC_CALL || body->code[last].b != 1) return;
        int fi = body->code[last].a;
        if (fi < 0 || !fs[fi].is_builtin || strcmp(fs[fi].n, "floor")) return;
        int obj_slot = body->code[last-3].a;
        int field_slot = body->code[last-2].a;
        if (obj_slot != f->p_slots[0] || field_slot != f->p_slots[1]) return;
        f->is_inlinable = 1; f->inline_pat = 6;
        f->inline_obj_slot = obj_slot;
        f->inline_field_slot = field_slot;
        return;
    }
}

void comp_fn(Code *c) {
    (void)c;
    tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    if (ts[tp].t != T_ID) die("expected function name");
    char *name = strdup(ts[tp].s); tp++;
    if (ts[tp].t != T_LP) die("expected ("); tp++;
    if (fc >= fm) { fm = fm ? fm*2 : 8; fs = realloc(fs, fm*sizeof(Fn)); }
    int fi = fc;
    Fn *f = &fs[fc++]; memset(f, 0, sizeof(Fn));
    f->n = name; f->code = NULL; f->p_slots = NULL;

    /* Parse parameters with required defaults */
    char *params[64]; int pa = 0;
    Value def_vals[64];
    if (ts[tp].t != T_RP) {
        do {
            if (ts[tp].t != T_ID) die("expected param name");
            params[pa] = strdup(ts[tp].s);
            tp++;
            if (ts[tp].t != T_ASSIGN) die("parameter '%s' must have a default value", params[pa]);
            tp++;
            def_vals[pa] = eval_constant_expr();
            pa++;
        } while (ts[tp].t == T_CM && (tp++, 1));
    }
    if (ts[tp].t != T_RP) die("expected )"); tp++;
    if (ffind(name) >= 0 && ffind(name) != fi) die("'%s' already defined", name);

    /* Save top-level var state, restore after function body */
    char **saved_vars = comp_vars;
    ExprType *saved_types = comp_types;
    int saved_vc = comp_vc, saved_vm = comp_vm;

    /* Reset var table for this function body */
    comp_vars = NULL; comp_types = NULL; comp_vc = 0; comp_vm = 0;
    for (int i = 0; i < pa; i++) var_add(params[i]);

    /* Set types from defaults */
    for (int i = 0; i < pa; i++) {
        int slot = var_find(params[i]);
        set_var_type(slot, def_vals[i].type == VAL_NUM ? T_NUM_TYPE :
            (def_vals[i].arr && def_vals[i].arr->is_string ? T_STR_TYPE : T_ARR_TYPE));
    }

    /* Register param slots for fast binding */
    f->p_slots = malloc(pa * sizeof(int));
    for (int i = 0; i < pa; i++) f->p_slots[i] = var_find(params[i]);
    f->a = pa;

    /* Save default values in Fn struct */
    if (pa > 0) {
        f->def_vals = malloc(pa * sizeof(Value));
        for (int i = 0; i < pa; i++) {
            f->def_vals[i] = def_vals[i];
            if (def_vals[i].type == VAL_ARR) aretain(def_vals[i].arr);
        }
    } else {
        f->def_vals = NULL;
    }

    /* Compile body */
    fn_ret_type = T_UNKNOWN; fn_ret_seen = 0;
    Code *body = new_code(); comp_block(body); emit(body, (Instr){OC_END, 0, 0, .num = 0});
    f->code = body;
    f->nvars = comp_vc;
    f->ret_type = fn_ret_seen ? fn_ret_type : T_ARR_TYPE;
    detect_inline(f);

    /* Free function's var table */
    for (int i = 0; i < comp_vc; i++) free(comp_vars[i]);
    free(comp_vars); free(comp_types);

    /* Restore top-level var state */
    comp_vars = saved_vars;
    comp_types = saved_types;
    comp_vc = saved_vc;
    comp_vm = saved_vm;

    /* TCO detection */
    int last = body->len - 1;
    if (last >= 0 && body->code[last].op == OC_END) last--;
    if (last >= 0 && body->code[last].op == OC_RET) last--;
    if (last >= 0 && body->code[last].op == OC_CALL &&
        body->code[last].a < fc && !strcmp(fs[body->code[last].a].n, name))
        { body->code[last].op = OC_TCO; body->code[last].a = body->code[last].b; body->code[last].b = 0; }
}

/* Evaluate an include path expression at compile time.
 * Supports string literals and + concatenation.
 * Returns a malloc'd string the caller must free. */
char *eval_include_path(void) {
    while (ts[tp].t == T_NL) tp++;

    char *result = NULL;

    /* Parse primary */
    if (ts[tp].t == T_STR) {
        Arr *a = (Arr*)ts[tp].s; tp++;
        int plen = a ? a->len : 0;
        result = malloc(plen + 1);
        for (int i = 0; i < plen; i++) result[i] = (char)val_num(a->val[i]);
        result[plen] = '\0';
    } else {
        die("include requires a string literal");
    }

    /* Handle + concatenation */
    while (ts[tp].t == T_PL) {
        tp++;
        char *right = NULL;
        if (ts[tp].t == T_STR) {
            Arr *a = (Arr*)ts[tp].s; tp++;
            int plen = a ? a->len : 0;
            right = malloc(plen + 1);
            for (int i = 0; i < plen; i++) right[i] = (char)val_num(a->val[i]);
            right[plen] = '\0';
        } else {
            die("include concatenation requires string literal");
        }
        char *tmp = malloc(strlen(result) + strlen(right) + 1);
        strcpy(tmp, result);
        strcat(tmp, right);
        free(result);
        free(right);
        result = tmp;
    }

    return result;
}

void comp_include(Code *c) {
    tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    int is_literal = (ts[tp].t == T_STR);
    char *path;
    if (is_literal) {
        /* String literal — extract the path */
        Arr *a = (Arr*)ts[tp].s; tp++;
        int plen = a ? a->len : 0;
        if (plen >= 1024) die("include path too long");
        path = malloc(plen + 1);
        for (int i = 0; i < plen; i++) path[i] = (char)val_num(a->val[i]);
        path[plen] = '\0';
    } else {
        /* Expression — evaluate at compile time */
        path = eval_include_path();
    }
    int plen = strlen(path);
    if (plen >= 1024) { free(path); die("include path too long"); }
    char full[1024];
    if (is_literal && include_dir && include_dir[0]) {
        /* String literal relative path — resolve against including file's dir */
        snprintf(full, sizeof(full), "%s/%s", include_dir, path);
    } else {
        /* Absolute path or expression — use directly */
        if ((size_t)plen >= sizeof(full)) plen = sizeof(full) - 1;
        memcpy(full, path, plen);
        full[plen] = '\0';
    }
    free(path);
    char *content = readf(full);
    if (!content) die("cannot include '%s'", full);
    Tok *saved_ts = ts; int saved_tc = tc, saved_tp = tp; char *saved_dir = include_dir;
    char *saved_file = comp_file;
    char inc_dir[1024] = {0};
    const char *sl = strrchr(full, '/');
    if (sl) { memcpy(inc_dir, full, sl - full); inc_dir[sl - full] = '\0'; }
    include_dir = inc_dir;
    comp_file = strdup(full);
    lex(content);
    while (ts[tp].t != T_EOF) {
        while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
        if (ts[tp].t == T_EOF) break;
        comp_stmt(c);
    }
    free(ts); ts = saved_ts; tc = saved_tc; tp = saved_tp;
    include_dir = saved_dir; free(comp_file); comp_file = saved_file; free(content);
}

/* Check if tokens at pn form a bracket literal [...] */
static int is_bracket_literal(int pn) {
    if (ts[pn].t != T_LB) return 0;
    pn++; int depth = 1;
    while (depth > 0 && ts[pn].t != T_EOF) {
        if (ts[pn].t == T_LP || ts[pn].t == T_LB) depth++;
        if (ts[pn].t == T_RP || ts[pn].t == T_RB) depth--;
        pn++;
    }
    return 1;
}

/* Peek at token stream to determine expression type at compile time.
 * Advances pn past the expression. Returns T_UNKNOWN if indeterminate. */
static ExprType peek_expr_type(int *pn) {
    while (ts[*pn].t == T_NL || ts[*pn].t == T_SEMI) (*pn)++;
    ExprType t = T_UNKNOWN;
    switch (ts[*pn].t) {
        case T_NUM: (*pn)++; t = T_NUM_TYPE; break;
        case T_NIL: case T_STR: case T_LB: {
            /* nil, string, or bracket literal */
            int is_str = (ts[*pn].t == T_STR);
            if (ts[*pn].t == T_LB) {
                /* skip past the bracket literal */
                int d = 1; (*pn)++;
                while (d > 0 && ts[*pn].t != T_EOF) {
                    if (ts[*pn].t == T_LP || ts[*pn].t == T_LB) d++;
                    if (ts[*pn].t == T_RP || ts[*pn].t == T_RB) d--;
                    (*pn)++;
                }
            } else {
                (*pn)++;
            }
            t = is_str ? T_STR_TYPE : T_ARR_TYPE;
            break;
        }
        case T_LP: {
            (*pn)++; /* skip ( */
            t = peek_expr_type(pn);
            if (ts[*pn].t == T_RP) (*pn)++;
            break;
        }
        case T_MI: case T_BN: { (*pn)++; t = peek_expr_type(pn); break; }
        case T_HASH: { (*pn)++; peek_expr_type(pn); t = T_NUM_TYPE; break; }
        case T_ID: {
            char *nm = ts[*pn].s; (*pn)++;
            if (ts[*pn].t == T_LP) {
                /* function call */
                (*pn)++; /* skip ( */
                int d = 1;
                while (d > 0 && ts[*pn].t != T_EOF) {
                    if (ts[*pn].t == T_LP || ts[*pn].t == T_LB) d++;
                    if (ts[*pn].t == T_RP || ts[*pn].t == T_RB) d--;
                    (*pn)++;
                }
                if (!strcmp(nm, "thisfile")) t = T_STR_TYPE;
                else { int fi = ffind(nm); t = (fi >= 0) ? fs[fi].ret_type : T_UNKNOWN; }
            } else {
                int slot = var_find(nm);
                t = (slot >= 0) ? comp_types[slot] : T_UNKNOWN;
            }
            break;
        }
        default: break;
    }
    /* Index brackets [...] — result type is unknown */
    while (ts[*pn].t == T_LB) {
        int d = 1; (*pn)++;
        while (d > 0 && ts[*pn].t != T_EOF) {
            if (ts[*pn].t == T_LP || ts[*pn].t == T_LB) d++;
            if (ts[*pn].t == T_RP || ts[*pn].t == T_RB) d--;
            (*pn)++;
        }
        t = T_UNKNOWN;
    }
    while (t != T_UNKNOWN && is_binary_op(ts[*pn].t)) {
        int op = ts[*pn].t; (*pn)++;
        ExprType rt = peek_expr_type(pn);
        if (t == T_UNKNOWN || rt == T_UNKNOWN) {
            t = T_UNKNOWN;
        } else if (op == T_PL) {
            /* +: num+num=num; str+str=arr; str+num=arr; num+str=arr */
            if (t == T_STR_TYPE || rt == T_STR_TYPE)
                t = T_STR_TYPE;
            else
                t = (t == T_NUM_TYPE && rt == T_NUM_TYPE) ? T_NUM_TYPE : T_ARR_TYPE;
        } else if (op == T_ST) {
            /* *: num*num=num; arr*num=arr (repeat); str*num=str */
            if (t == T_STR_TYPE && rt == T_NUM_TYPE)
                t = T_STR_TYPE;
            else if (t == T_ARR_TYPE && rt == T_NUM_TYPE)
                t = T_ARR_TYPE;
            else
                t = T_NUM_TYPE;
        } else {
            /* All other binary ops produce numbers */
            t = T_NUM_TYPE;
        }
    }
    return t;
}

void comp_stmt(Code *c) {
    while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
    if (ts[tp].t == T_EOF) return;
    if (ts[tp].t == T_RC) die("unexpected '}'");
    comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    switch (ts[tp].t) {
        case T_IF: comp_if(c); break;
        case T_WH: comp_while(c); break;
        case T_FN: comp_fn(c); break;
        case T_RT: comp_return(c); break;
        case T_LC: comp_block(c); break;
        case T_INCLUDE: comp_include(c); break;
        case T_ID: {
            /* Check for destructure: id, id, ... = expr */
            {
                int scan = tp;
                int dvars[64], dvc = 0;
                if (ts[scan].t == T_ID) {
                    dvars[dvc++] = scan; scan++;
                    while (ts[scan].t == T_CM || ts[scan].t == T_NL) {
                        if (ts[scan].t == T_CM) { scan++; while (ts[scan].t == T_NL) scan++; }
                        else { scan++; continue; }
                        if (ts[scan].t != T_ID) break;
                        dvars[dvc++] = scan; scan++;
                    }
                }
                if (dvc >= 2 && ts[scan].t == T_ASSIGN) {
                    /* Collect LHS variable slots */
                    int var_slots[64];
                    for (int i = 0; i < dvc; i++) {
                        tp = dvars[i];
                        char *nm = strdup(ts[tp].s); tp++;
                        int slot = var_find(nm);
                        if (slot < 0) slot = var_add(nm);
                        var_slots[i] = slot;
                        free(nm);
                    }
                    if (ts[tp].t != T_ASSIGN) die("expected =");
                    tp++; /* skip = */
                    /* Parse RHS — first expression */
                    comp_expr(c);
                    int rhs_count = 1;
                    while (ts[tp].t == T_CM) {
                        tp++;
                        comp_expr(c);
                        rhs_count++;
                    }
                    if (rhs_count > 1)
                        emit(c, (Instr){OC_MAKE_ARR, rhs_count, 0, .num = 0});
                    /* Store destructure slots in Arr* */
                    Arr *slot_arr = aalloc(dvc);
                    slot_arr->len = dvc;
                    for (int i = 0; i < dvc; i++)
                        slot_arr->val[i] = vnum((double)var_slots[i]);
                    emit(c, (Instr){OC_DESTRUCTURE, dvc, 0, .arr = slot_arr});
                    break;
                }
            }
            int pt = tp + 1;
            while (ts[pt].t == T_LB) {
                pt++; int bd = 1;
                while (bd > 0 && ts[pt].t != T_EOF) {
                    if (ts[pt].t == T_LB) bd++;
                    if (ts[pt].t == T_RB) bd--; pt++;
                }
            }
            int at = ts[pt].t;
            int is_assign = (at == T_ASSIGN);
            int is_compound = (at == T_PL_ASSIGN || at == T_MI_ASSIGN ||
                               at == T_ST_ASSIGN || at == T_SL_ASSIGN);
            if (is_assign || is_compound) {
                char *nm = strdup(ts[tp].s); int name_tp = tp; tp++; int idx_count = 0;
                while (ts[tp].t == T_LB) {
                    tp++;
                    do { comp_expr(c); idx_count++; } while (ts[tp].t == T_CM && (tp++, 1));
                    if (ts[tp].t != T_RB) die("expected ]"); tp++;
                }
                int compound_op = 0;
                if (ts[tp].t == T_PL_ASSIGN) compound_op = T_PL;
                else if (ts[tp].t == T_MI_ASSIGN) compound_op = T_MI;
                else if (ts[tp].t == T_ST_ASSIGN) compound_op = T_ST;
                else if (ts[tp].t == T_SL_ASSIGN) compound_op = T_SL;
                /* Try OC_MUTATE_NUM for array[idx] op= delta (fused read-modify-write) */
                if (compound_op) {
                    int slot = var_find(nm);
                    if (slot >= 0 && comp_types[slot] == T_ARR_TYPE && idx_count >= 1 && comp_last_type == T_NUM_TYPE) {
                        tp++;  /* skip compound op token */
                        comp_expr(c);  /* compile RHS delta expression */
                        if (profile_flag) emit(c, (Instr){OC_PROFILE, slot, 0, .num = 3.0});
                        emit(c, (Instr){OC_MUTATE_NUM, slot, idx_count, .num = (double)compound_op});
                        free(nm);
                        break;
                    }
                    /* Fallback: rewrite compound into plain: x op= RHS → x = x op RHS (incl indices) */
                    int lhs = tp - name_tp;
                    memmove(&ts[tp + lhs + 2], &ts[tp + 1], (tc - tp - 1) * sizeof(Tok));
                    ts[tp].t = T_ASSIGN;
                    for (int i = 0; i < lhs; i++) {
                        ts[tp + 1 + i] = ts[name_tp + i];
                        if (ts[tp + 1 + i].t == T_ID && ts[tp + 1 + i].s)
                            ts[tp + 1 + i].s = strdup(ts[tp + 1 + i].s);
                        else if (ts[tp + 1 + i].t == T_STR && ts[tp + 1 + i].s)
                            aretain((Arr*)ts[tp + 1 + i].s);
                    }
                    ts[tp + 1 + lhs] = (Tok){ .t = (TK)compound_op, .s = NULL, .l = comp_line };
                    tc += lhs + 1;
                    compound_op = 0;
                }
                /* Plain assignment (including rewritten compound) */
                {
                    /* Plain assignment (including rewritten compound) */
                    if (ts[tp].t != T_ASSIGN) die("expected ="); tp++;
                    /* Check for x = x[slice] — in-place slice optimization */
                    int is_slice_assign = 0;
                    if (idx_count == 0) {
                        int pn = tp;
                        while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                        is_slice_assign = ts[pn].t == T_ID && !strcmp(ts[pn].s, nm);
                        if (is_slice_assign) {
                            pn++; while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                            is_slice_assign = ts[pn].t == T_LB;
                            if (is_slice_assign) {
                                pn++; /* skip [ */
                                int depth = 0, found_colon = 0;
                                for (int pp = pn; pp < tc; pp++) {
                                    if (depth == 0 && ts[pp].t == T_RB) break;
                                    if (depth == 0 && ts[pp].t == T_COLON) { found_colon = 1; break; }
                                    if (ts[pp].t == T_LB || ts[pp].t == T_LP) depth++;
                                    if (ts[pp].t == T_RB || ts[pp].t == T_RP) depth--;
                                }
                                is_slice_assign = found_colon;
                            }
                        }
                    }
                    if (is_slice_assign) {
                        /* Compile x = x[start:stop:step] as OC_SLICE_INPLACE */
                        int slot = var_find(nm);
                        if (slot < 0) slot = var_add(nm);
                        set_var_type(slot, T_ARR_TYPE);
                        tp++; tp++; /* skip x, [ */
                        /* Compile start */
                        if (ts[tp].t != T_COLON && ts[tp].t != T_RB) comp_expr(c);
                        else emit(c, (Instr){OC_NIL, 0, 0, .num = 0});
                        if (ts[tp].t != T_COLON) die("expected ':' in slice"); tp++;
                        /* Compile stop */
                        if (ts[tp].t != T_COLON && ts[tp].t != T_RB) comp_expr(c);
                        else emit(c, (Instr){OC_NIL, 0, 0, .num = 0});
                        /* Compile step (default 1) */
                        if (ts[tp].t == T_COLON) {
                            tp++;
                            if (ts[tp].t != T_RB) comp_expr(c);
                            else emit(c, (Instr){OC_NUM, 0, 0, .num = 1.0});
                        } else emit(c, (Instr){OC_NUM, 0, 0, .num = 1.0});
                        if (ts[tp].t != T_RB) die("expected ]"); tp++;
                        if (profile_flag) emit(c, (Instr){OC_PROFILE, slot, 0, .num = 3.0});
                        emit(c, (Instr){OC_SLICE_INPLACE, slot, 0, .num = 0});
                    } else {
                        int is_push = 0;
                        if (idx_count == 0) {
                            /* Push detection for simple variable: var + [...] */
                            int pn = tp;
                            while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                            is_push = ts[pn].t == T_ID && !strcmp(ts[pn].s, nm);
                            if (is_push) {
                                pn++; while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                                is_push = ts[pn].t == T_PL;
                                if (is_push) {
                                    pn++; while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                                    is_push = is_bracket_literal(pn);
                                    if (is_push) {
                                        int scan = pn + 1, depth = 1;
                                        while (depth > 0 && ts[scan].t != T_EOF) {
                                            if (ts[scan].t == T_LP || ts[scan].t == T_LB) depth++;
                                            if (ts[scan].t == T_RP || ts[scan].t == T_RB) depth--;
                                            scan++;
                                        }
                                        while (ts[scan].t == T_NL || ts[scan].t == T_SEMI) scan++;
                                        is_push = !is_binary_op(ts[scan].t);
                                    }
                                }
                            }
                        } else {
                            /* Push detection for indexed LHS: var [...] + [...] */
                            int pn = tp;
                            while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                            is_push = ts[pn].t == T_ID && !strcmp(ts[pn].s, nm);
                            if (is_push) {
                                pn++;
                                /* Skip past the index brackets (same count as compiled LHS) */
                                for (int k = 0; k < idx_count && is_push; k++) {
                                    while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                                    if (ts[pn].t != T_LB) { is_push = 0; break; }
                                    pn++;
                                    int bd = 1;
                                    while (bd > 0 && ts[pn].t != T_EOF) {
                                        if (ts[pn].t == T_LB || ts[pn].t == T_LP) bd++;
                                        if (ts[pn].t == T_RB || ts[pn].t == T_RP) bd--;
                                        pn++;
                                    }
                                }
                                if (is_push) {
                                    while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                                    is_push = ts[pn].t == T_PL;
                                    if (is_push) {
                                        pn++; while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                                        is_push = is_bracket_literal(pn);
                                        if (is_push) {
                                            int scan = pn + 1, bd = 1;
                                            while (bd > 0 && ts[scan].t != T_EOF) {
                                                if (ts[scan].t == T_LP || ts[scan].t == T_LB) bd++;
                                                if (ts[scan].t == T_RP || ts[scan].t == T_RB) bd--;
                                                scan++;
                                            }
                                            while (ts[scan].t == T_NL || ts[scan].t == T_SEMI) scan++;
                                            is_push = !is_binary_op(ts[scan].t);
                                        }
                                    }
                                }
                            }
                        }
                        if (is_push) {
                        int slot = var_find(nm);
                        if (slot < 0) slot = var_add(nm);
                        set_var_type(slot, T_ARR_TYPE);
                        tp++;  /* skip arr */
                        /* Skip past index brackets in token stream (already compiled) */
                        for (int k = 0; k < idx_count; k++) {
                            tp++;  /* skip [ */
                            int bd = 1;
                            while (bd > 0 && ts[tp].t != T_EOF) {
                                if (ts[tp].t == T_LB || ts[tp].t == T_LP) bd++;
                                if (ts[tp].t == T_RB || ts[tp].t == T_RP) bd--;
                                tp++;
                            }
                        }
                        tp++; tp++;  /* skip +, [ */
                        do {
                            comp_expr(c);
                            if (idx_count > 0) {
                                if (profile_flag) emit(c, (Instr){OC_PROFILE, slot, 0, .num = 3.0});
                                emit(c, (Instr){OC_LVALS_PUSH, slot, idx_count, .num = 0});
                            } else {
                                if (profile_flag) emit(c, (Instr){OC_PROFILE, slot, 0, .num = 3.0});
                                emit(c, (Instr){OC_PUSH, slot, 0, .num = 0});
                            }
                        } while (ts[tp].t == T_CM && (tp++, 1));
                        if (ts[tp].t != T_RB) die("expected ]"); tp++;
                    } else {
                        int use_push_all = (idx_count == 0);
                        if (use_push_all) {
                            int pn = tp;
                            while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                            use_push_all = ts[pn].t == T_ID && !strcmp(ts[pn].s, nm);
                            if (use_push_all) {
                                pn++; while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                                use_push_all = ts[pn].t == T_PL;
                            }
                        }
                        if (use_push_all) {
                            int slot = var_find(nm);
                            if (slot < 0) slot = var_add(nm);
                            /* Only use push_all when slot is known to be an array */
                            if (comp_types[slot] != T_ARR_TYPE) use_push_all = 0;
                        }
                        if (use_push_all) {
                            int slot = var_find(nm);
                            set_var_type(slot, T_ARR_TYPE);
                            tp++; tp++;  /* skip var, + */
                            comp_expr(c);
                            emit(c, (Instr){OC_PUSH_ALL, slot, 0, .num = 0});
                        } else if (idx_count > 0) {
                            comp_expr(c);
                            int slot = var_find(nm);
                            if (slot < 0) slot = var_add(nm);
                            set_var_type(slot, T_ARR_TYPE);
                            if (profile_flag) emit(c, (Instr){OC_PROFILE, slot, 0, .num = 3.0});
                            emit(c, (Instr){OC_LVALS, slot, idx_count, .num = 0});
                        } else {
                            int pn = tp;
                            ExprType store_type = peek_expr_type(&pn);
                            /* Detect x = f(x, ...) pattern for move semantics */
                            if (idx_count == 0) {
                                int scan = tp;
                                while (ts[scan].t == T_NL || ts[scan].t == T_SEMI) scan++;
                                if (ts[scan].t == T_ID && ts[scan+1].t == T_LP) {
                                    int scan2 = scan + 2;
                                    while (ts[scan2].t == T_NL) scan2++;
                                    if (ts[scan2].t == T_ID && !strcmp(ts[scan2].s, nm) &&
                                        (ts[scan2+1].t == T_CM || ts[scan2+1].t == T_RP)) {
                                        int s = var_find(nm);
                                        if (s >= 0) move_slot_hint = s;
                                    }
                                }
                            }
                            comp_expr(c);
                            move_slot_hint = -1;
                            int slot = var_find(nm);
                            if (slot < 0) slot = var_add(nm);
                            set_var_type(slot, store_type);
                            emit(c, (Instr){OC_STORE_SLOT, slot, 0, .num = 0});
                        }
                        }
                    }
                }
                free(nm);
            } else { comp_expr(c); emit(c, (Instr){ (comp_file ? OC_POP : OC_PRINT), 0, 0, .num = 0 }); }
            break;
        }
        default: comp_expr(c); emit(c, (Instr){ (comp_file ? OC_POP : OC_PRINT), 0, 0, .num = 0 }); break;
    }
}

void comp_program(Code *c) {
    while (ts[tp].t != T_EOF) {
        while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
        if (ts[tp].t == T_EOF) break;
        comp_stmt(c);
    }
    emit(c, (Instr){OC_END, 0, 0, .num = 0});
}

/* ─── VM Executor (C99 goto-to-switch dispatch) ─── */

void exec(Code *c) {
    int ip = 0;
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    goto dispatch;

    /* ── Opcode handlers ── */
op_num:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    istk[++isp] = vnum(c->code[ip].num); ip++; goto dispatch;

op_nil:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    istk[++isp] = nilv(); ip++; goto dispatch;

op_str: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    istk[++isp] = (Value){ .type = VAL_ARR, .arr = ins->arr };
    aretain(ins->arr); ip++; goto dispatch;
}

op_make_arr: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int n = ins->a; Value tmp[64];
    for (int i = n-1; i >= 0; i--) tmp[i] = istk[isp--];
    Arr *a = aalloc(n); a->len = n;
    for (int i = 0; i < n; i++) {
        a->val[i] = tmp[i];
        if (tmp[i].type == VAL_ARR) { aretain(tmp[i].arr); arelease(tmp[i].arr); }
    }
    istk[++isp] = (Value){ .type = VAL_ARR, .arr = a };
    ip++; goto dispatch;
}

/* Name-based variable access (top-level / REPL) */
op_var: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value v = sget(cs, ins->name);
    istk[++isp] = v; if (v.type == VAL_ARR) aretain(v.arr);
    ip++; goto dispatch;
}
/* Slot-indexed variable access (function bodies) — O(1), no strcmp */
op_var_slot: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    int slot = c->code[ip].a;
    Value v = cs->v[slot];
    istk[++isp] = v; if (v.type == VAL_ARR) aretain(v.arr);
    ip++; goto dispatch;
}
op_store_slot: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    int slot = c->code[ip].a;
    Value v = istk[isp--];
    vassign(&cs->v[slot], v);
    if (v.type == VAL_ARR) arelease(v.arr);
    ip++; goto dispatch;
}

op_op: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value r = istk[isp--], l = istk[isp--];
    /* Fast path: both operands are numbers — skip apply() entirely */
    if (l.type == VAL_NUM && r.type == VAL_NUM) {
        double ld = l.num, rd = r.num;
        switch (ins->a) {
            case T_PL: istk[++isp] = vnum(ld + rd); ip++; goto dispatch;
            case T_MI: istk[++isp] = vnum(ld - rd); ip++; goto dispatch;
            case T_ST: istk[++isp] = vnum(ld * rd); ip++; goto dispatch;
            case T_SL: if (rd == 0) die("division by zero"); istk[++isp] = vnum(ld / rd); ip++; goto dispatch;
            case T_PC: if (rd == 0) die("modulo by zero"); istk[++isp] = vnum(fmod(ld, rd)); ip++; goto dispatch;
            case T_LT: istk[++isp] = ld < rd ? vnum(1) : nilv(); ip++; goto dispatch;
            case T_GT: istk[++isp] = ld > rd ? vnum(1) : nilv(); ip++; goto dispatch;
            case T_LE: istk[++isp] = ld <= rd ? vnum(1) : nilv(); ip++; goto dispatch;
            case T_GE: istk[++isp] = ld >= rd ? vnum(1) : nilv(); ip++; goto dispatch;
            case T_EQ: istk[++isp] = ld == rd ? vnum(1) : nilv(); ip++; goto dispatch;
            case T_NE: istk[++isp] = ld != rd ? vnum(1) : nilv(); ip++; goto dispatch;
            case T_SHL: istk[++isp] = vnum((double)((int64_t)ld << (int32_t)rd)); ip++; goto dispatch;
            case T_SHR: istk[++isp] = vnum((double)((int64_t)ld >> (int32_t)rd)); ip++; goto dispatch;
            case T_AM: istk[++isp] = vnum((double)((int64_t)ld & (int64_t)rd)); ip++; goto dispatch;
            case T_PI: istk[++isp] = vnum((double)((int64_t)ld | (int64_t)rd)); ip++; goto dispatch;
            case T_CA: istk[++isp] = vnum((double)((int64_t)ld ^ (int64_t)rd)); ip++; goto dispatch;
            default: break;
        }
    }
    /* Fallback: call apply() for non-numeric cases */
    {
        Value res = apply(ins->a, l, r);
        if (l.type == VAL_ARR) arelease(l.arr);
        if (r.type == VAL_ARR) arelease(r.arr);
        istk[++isp] = res; ip++; goto dispatch;
    }
}

op_add_num: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    double r = istk[isp--].num, l = istk[isp--].num;
    istk[++isp] = vnum(l + r); ip++; goto dispatch;
}
op_sub_num: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    double r = istk[isp--].num, l = istk[isp--].num;
    istk[++isp] = vnum(l - r); ip++; goto dispatch;
}
op_mul_num: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    double r = istk[isp--].num, l = istk[isp--].num;
    istk[++isp] = vnum(l * r); ip++; goto dispatch;
}
op_div_num: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    double r = istk[isp--].num, l = istk[isp--].num;
    if (r == 0) die("division by zero");
    istk[++isp] = vnum(l / r); ip++; goto dispatch;
}

op_unary: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value v = istk[isp--];
    if (ins->a == T_BN) { istk[++isp] = truthy(v) ? nilv() : vnum(1); if (v.type==VAL_ARR) arelease(v.arr); }
    else if (ins->a == T_MI) { if (v.type!=VAL_NUM) die("minus on non-number"); istk[++isp] = vnum(-val_num(v)); }
    else if (ins->a == T_HASH) { if (v.type!=VAL_ARR) die("# requires array"); istk[++isp] = vnum((double)(v.arr ? v.arr->len : 0)); if (v.type==VAL_ARR) arelease(v.arr); }
    else if (ins->a == T_TILDE) { if (v.type!=VAL_NUM) die("bitwise ~ requires number"); istk[++isp] = vnum((double)(~((int64_t)val_num(v)))); }
    ip++; goto dispatch;
}

op_index: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Value idx = istk[isp--], arr = istk[isp--];
    if (arr.type != VAL_ARR) die("cannot index into non-array");
    if (idx.type == VAL_NUM) {
        int ii = (int)val_num(idx);
        if (!arr.arr || ii < 0 || ii >= arr.arr->len) die("index out of bounds");
        Value res = arr.arr->val[ii];
        if (res.type == VAL_ARR) aretain(res.arr);
        arelease(arr.arr); istk[++isp] = res;
    } else if (idx.type == VAL_ARR) {
        if (idx.arr && idx.arr->len > 0 && is_string_arr(idx.arr)) {
            /* String key → hash-based indexing */
            if (!arr.arr || arr.arr->len == 0) die("cannot index into empty array");
            unsigned int h = get_arr_hash(idx.arr);
            int ii = (int)(h % (unsigned int)arr.arr->len);
            Value res = arr.arr->val[ii];
            if (res.type == VAL_ARR) aretain(res.arr);
            arelease(arr.arr);
            arelease(idx.arr);
            istk[++isp] = res;
        } else {
            /* Generic array → chain of numeric indices */
            Value cur = arr;
            if (idx.arr) for (int j = 0; j < idx.arr->len; j++) {
                int ii = (int)val_num(idx.arr->val[j]);
                if (cur.type != VAL_ARR || !cur.arr || ii < 0 || ii >= cur.arr->len) die("index out of bounds");
                Value next = cur.arr->val[ii];
                if (next.type == VAL_ARR) aretain(next.arr);
                if (cur.type == VAL_ARR) arelease(cur.arr);
                cur = next;
            }
            if (idx.type == VAL_ARR) arelease(idx.arr);
            istk[++isp] = cur;
        }
    } else die("index must be number or array");
    ip++; goto dispatch;
}

op_lvals: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int slot = ins->a, depth = ins->b;
    Value val = istk[isp--];
    Value indices[16];
    for (int j = depth-1; j >= 0; j--) indices[j] = istk[isp--];
    Value *root = &cs->v[slot];
    Value *sp = root;
    for (int j = 0; j < depth; j++) {
        amake_uniq(sp);
        if (indices[j].type == VAL_NUM) {
            int ii = (int)val_num(indices[j]);
            if (sp->type != VAL_ARR || !sp->arr || ii < 0 || ii >= sp->arr->len) die("index out of bounds");
            sp = &sp->arr->val[ii];
        } else if (indices[j].type == VAL_ARR) {
            if (indices[j].arr && indices[j].arr->len > 0 && is_string_arr(indices[j].arr)) {
                /* String key → hash-based indexing */
                if (sp->type != VAL_ARR || !sp->arr || sp->arr->len == 0) die("cannot index into empty array");
                unsigned int h = get_arr_hash(indices[j].arr);
                int ii = (int)(h % (unsigned int)sp->arr->len);
                amake_uniq(sp); sp = &sp->arr->val[ii];
            } else {
                for (int k = 0; k < indices[j].arr->len; k++) {
                    int ii = (int)val_num(indices[j].arr->val[k]);
                    if (sp->type != VAL_ARR || !sp->arr || ii < 0 || ii >= sp->arr->len) die("index out of bounds");
                    amake_uniq(sp); sp = &sp->arr->val[ii];
                }
            }
        } else die("index must be number or array");
    }
    vassign(sp, val);
    if (val.type == VAL_ARR) arelease(val.arr);
    for (int j = 0; j < depth; j++)
        if (indices[j].type == VAL_ARR) arelease(indices[j].arr);
    ip++; goto dispatch;
}

op_push: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int slot = ins->a;
    Value *slot_val = &cs->v[slot];
    Value elem = istk[isp--];
    if (!slot_val->arr) { slot_val->arr = aalloc(4); slot_val->arr->len = 0; }
    else amake_uniq(slot_val);
    int len = slot_val->arr->len;
    if (len >= slot_val->arr->cap) {
        int old_cap = slot_val->arr->cap;
        slot_val->arr->cap = slot_val->arr->cap ? slot_val->arr->cap * 2 : 4;
        slot_val->arr->val = realloc(slot_val->arr->val, slot_val->arr->cap * sizeof(Value));
        memset(slot_val->arr->val + old_cap, 0, (slot_val->arr->cap - old_cap) * sizeof(Value));
    }
    vassign(&slot_val->arr->val[len], elem);
    slot_val->arr->len = len + 1;
    if (elem.type == VAL_ARR) arelease(elem.arr);
    ip++; goto dispatch;
}

op_lvals_push: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int slot = ins->a, depth = ins->b;
    Value elem = istk[isp--];
    Value indices[16];
    for (int j = depth-1; j >= 0; j--) indices[j] = istk[isp--];
    Value *sp = &cs->v[slot];
    /* Navigate to the target bucket (same logic as op_lvals) */
    for (int j = 0; j < depth; j++) {
        amake_uniq(sp);
        if (indices[j].type == VAL_NUM) {
            int ii = (int)val_num(indices[j]);
            if (sp->type != VAL_ARR || !sp->arr || ii < 0 || ii >= sp->arr->len) die("index out of bounds");
            sp = &sp->arr->val[ii];
        } else if (indices[j].type == VAL_ARR) {
            if (indices[j].arr && indices[j].arr->len > 0 && is_string_arr(indices[j].arr)) {
                if (sp->type != VAL_ARR || !sp->arr || sp->arr->len == 0) die("cannot index into empty array");
                unsigned int h = get_arr_hash(indices[j].arr);
                int ii = (int)(h % (unsigned int)sp->arr->len);
                amake_uniq(sp); sp = &sp->arr->val[ii];
            } else {
                for (int k = 0; k < indices[j].arr->len; k++) {
                    int ii = (int)val_num(indices[j].arr->val[k]);
                    if (sp->type != VAL_ARR || !sp->arr || ii < 0 || ii >= sp->arr->len) die("index out of bounds");
                    amake_uniq(sp); sp = &sp->arr->val[ii];
                }
            }
        } else die("index must be number or array");
    }
    /* Now sp points to the bucket — push elem into it */
    if (sp->type != VAL_ARR) die("cannot push into non-array");
    if (!sp->arr) { sp->arr = aalloc(4); sp->arr->len = 0; }
    else amake_uniq(sp);
    int len = sp->arr->len;
    if (len >= sp->arr->cap) {
        int old_cap = sp->arr->cap;
        sp->arr->cap = sp->arr->cap ? sp->arr->cap * 2 : 4;
        sp->arr->val = realloc(sp->arr->val, sp->arr->cap * sizeof(Value));
        memset(sp->arr->val + old_cap, 0, (sp->arr->cap - old_cap) * sizeof(Value));
    }
    vassign(&sp->arr->val[len], elem);
    sp->arr->len = len + 1;
    if (elem.type == VAL_ARR) arelease(elem.arr);
    for (int j = 0; j < depth; j++)
        if (indices[j].type == VAL_ARR) arelease(indices[j].arr);
    ip++; goto dispatch;
}

op_push_all: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int slot = ins->a;
    Value rhs = istk[isp--];
    Value *slot_val = &cs->v[slot];
    if (rhs.type == VAL_ARR) {
        /* RHS is an array: push all elements into slot in-place */
        if (!slot_val->arr) { slot_val->arr = aalloc(4); slot_val->arr->len = 0; }
        else amake_uniq(slot_val);
        Arr *ra = rhs.arr;
        int rn = ra ? ra->len : 0;
        for (int i = 0; i < rn; i++) {
            int len = slot_val->arr->len;
            if (len >= slot_val->arr->cap) {
                int old_cap = slot_val->arr->cap;
                slot_val->arr->cap = slot_val->arr->cap ? slot_val->arr->cap * 2 : 4;
                slot_val->arr->val = realloc(slot_val->arr->val, slot_val->arr->cap * sizeof(Value));
                memset(slot_val->arr->val + old_cap, 0, (slot_val->arr->cap - old_cap) * sizeof(Value));
            }
            vassign(&slot_val->arr->val[len], ra->val[i]);
            slot_val->arr->len = len + 1;
        }
        arelease(rhs.arr);
    } else {
        /* Fallback: use normal + operator */
        Value lhs = *slot_val;
        if (lhs.type == VAL_ARR) aretain(lhs.arr);
        Value res = apply(T_PL, lhs, rhs);
        vassign(slot_val, res);
        if (lhs.type == VAL_ARR) arelease(lhs.arr);
        if (rhs.type == VAL_ARR) arelease(rhs.arr);
        if (res.type == VAL_ARR) arelease(res.arr);
    }
    ip++; goto dispatch;
}

op_slice: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Value step_v = istk[isp--], stop_v = istk[isp--], start_v = istk[isp--], arr_v = istk[isp--];
    if (arr_v.type != VAL_ARR) die("slice requires array");
    Arr *src = arr_v.arr; int len = src ? src->len : 0;
    int step = (int)val_num(step_v);
    if (step == 0) die("slice step cannot be 0");
    int start, stop, count;
    slice_bounds(len, step, start_v, stop_v, &start, &stop, &count);
    if (count == 0) { arelease(arr_v.arr); istk[++isp] = nilv(); ip++; goto dispatch; }
    if (step == 1) {
        /* Zero-copy view: share parent's backing store */
        Arr *view = malloc(sizeof(Arr));
        view->refcount = 1;
        view->len = count;
        view->cap = count;
        view->val = src->val + start;
        view->is_slice = 1;
        view->parent = src;
        view->hash_cache = 0;
        view->is_string = src->is_string;
        aretain(src);
        arelease(arr_v.arr);
        istk[++isp] = (Value){ .type = VAL_ARR, .arr = view };
    } else {
        /* Strided slice: must copy */
        Arr *result = aalloc(count); result->len = count;
        result->is_string = src->is_string;
        int idx = 0;
        for (int i = start; step > 0 ? i < stop : i > stop; i += step) {
            result->val[idx] = src->val[i];
            if (result->val[idx].type == VAL_ARR) aretain(result->val[idx].arr);
            idx++;
        }
        arelease(arr_v.arr);
        istk[++isp] = (Value){ .type = VAL_ARR, .arr = result };
    }
    ip++; goto dispatch;
}

op_slice_inplace: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int slot = ins->a;
    Value step_v = istk[isp--], stop_v = istk[isp--], start_v = istk[isp--];
    Value *slot_val = &cs->v[slot];
    if (slot_val->type != VAL_ARR || !slot_val->arr) die("slice requires array");
    Arr *src = slot_val->arr;
    int len = src->len;
    int step = (int)val_num(step_v);
    if (step == 0) die("slice step cannot be 0");
    int start, stop, count;
    slice_bounds(len, step, start_v, stop_v, &start, &stop, &count);
    if (count == 0) {
        arelease(slot_val->arr);
        *slot_val = nilv();
        ip++; goto dispatch;
    }
    if (src->refcount == 1 && !src->is_slice && step == 1) {
        /* In-place: exclusively owned, contiguous slice — memmove + trim.
         * Safe for all-number arrays (common case). Falls back to view
         * if any element in the kept range is a sub-array. */
        int has_arr = 0;
        for (int i = start; i < start + count && !has_arr; i++)
            if (src->val[i].type == VAL_ARR) has_arr = 1;
        if (!has_arr) {
            /* All numbers in kept range: memmove and trim.
             * Elements at [count, old_len) are tail — release any sub-arrays there. */
            for (int i = count; i < src->len; i++)
                if (src->val[i].type == VAL_ARR) arelease(src->val[i].arr);
            memmove(src->val, src->val + start, count * sizeof(Value));
            src->len = count;
            ip++; goto dispatch;
        }
    }
    /* Fall back to zero-copy view (or copy for strided) */
    if (step == 1) {
        Arr *view = malloc(sizeof(Arr));
        view->refcount = 1;
        view->len = count;
        view->cap = count;
        view->val = src->val + start;
        view->is_slice = 1;
        view->parent = src;
        view->hash_cache = 0;
        view->is_string = src->is_string;
        aretain(src);
        arelease(slot_val->arr);
        slot_val->arr = view;
    } else {
        Arr *result = aalloc(count); result->len = count;
        result->is_string = src->is_string;
        int idx = 0;
        for (int i = start; step > 0 ? i < stop : i > stop; i += step) {
            result->val[idx] = src->val[i];
            if (result->val[idx].type == VAL_ARR) aretain(result->val[idx].arr);
            idx++;
        }
        arelease(slot_val->arr);
        slot_val->arr = result;
    }
    if (start_v.type == VAL_ARR) arelease(start_v.arr);
    if (stop_v.type == VAL_ARR) arelease(stop_v.arr);
    ip++; goto dispatch;
}

op_call: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int fi = ins->a, ac = ins->b; Fn *f = &fs[fi];
    Value args[64];
    for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
    /* Builtin fast path — no scope/call frame overhead */
    if (f->is_builtin) {
        Value result = f->native_fn(ac, args);
        istk[++isp] = result;
        ip++; goto dispatch;
    }
    int saved_isp = isp;
    Value saved_stack[64];
    for (int j = 0; j <= saved_isp; j++) saved_stack[j] = istk[j];
    Scp *saved_cs = cs; cs = snew_sized(f->nvars);
    int saved_cur_fi = cur_fi; cur_fi = fi;
    call_stack[++call_depth].fi = saved_cur_fi;
    call_stack[call_depth].line = err_line;
    call_stack[call_depth].file = err_file;
    for (int j = 0; j < f->a; j++) {
        if (j < ac) {
            cs->v[f->p_slots[j]] = args[j];
            if (args[j].type == VAL_ARR && args[j].arr) {
                /* Move semantics: if the arr is exclusively owned (refcount==1),
                 * skip the retain — the function takes ownership. This avoids
                 * unnecessary COW deep copies when the caller clears its slot. */
                if (args[j].arr->refcount > 1) aretain(args[j].arr);
            }
        } else if (f->def_vals) {
            Value d = f->def_vals[j];
            cs->v[f->p_slots[j]] = d;
            if (d.type == VAL_ARR) aretain(d.arr);
        } else {
            cs->v[f->p_slots[j]] = nilv();
        }
    }
    int saved_rf = rf; rf = 0; Value saved_rv = rv; isp = -1;
    exec(f->code);
    Value result = rf ? rv : nilv();
    sfree(cs); cs = saved_cs; cur_fi = saved_cur_fi;
    call_depth--;
    rf = saved_rf; rv = saved_rv; isp = saved_isp;
    for (int j = 0; j <= saved_isp; j++) istk[j] = saved_stack[j];
    istk[++isp] = result;
    ip++; goto dispatch;
}

op_tco: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int ac = ins->a; Fn *f = &fs[cur_fi];
    Value args[64];
    for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
    isp = -1; rf = 0;
    for (int j = 0; j < f->a; j++) {
        if (j < ac) {
            cs->v[f->p_slots[j]] = args[j];
            if (args[j].type == VAL_ARR && args[j].arr) aretain(args[j].arr);
        } else if (f->def_vals) {
            Value d = f->def_vals[j];
            cs->v[f->p_slots[j]] = d;
            if (d.type == VAL_ARR) aretain(d.arr);
        } else {
            cs->v[f->p_slots[j]] = nilv();
        }
    }
    ip = 0; goto dispatch;
}

op_ret:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    rv = istk[isp--]; rf = 1; return;

op_pop:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    if (isp >= 0) { Value v = istk[isp--]; if (v.type == VAL_ARR) arelease(v.arr); }
    ip++; goto dispatch;

op_dup: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    if (isp < 0) die("stack underflow");
    Value v = istk[isp]; if (v.type == VAL_ARR) aretain(v.arr);
    istk[++isp] = v; ip++; goto dispatch;
}

op_jz: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value v = istk[isp--]; int t = truthy(v);
    if (v.type == VAL_ARR) arelease(v.arr);
    ip = t ? ip + 1 : ins->a;
    goto dispatch;
}

op_jnz: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value v = istk[isp--]; int t = truthy(v);
    if (v.type == VAL_ARR) arelease(v.arr);
    ip = t ? ins->a : ip + 1;
    goto dispatch;
}

op_jmp:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    ip = c->code[ip].a;
    goto dispatch;

op_print:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    if (isp >= 0) { print_val(istk[isp--]); }
    ip++; goto dispatch;

op_input: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    char buf[1024]; int n;
    if (!fgets(buf, 1024, stdin)) { n = 0; } else { n = strlen(buf); if (n && buf[n-1]=='\n') n--; }
    Arr *a = aalloc(n); a->len = n; a->is_string = 1;
    for (int i = 0; i < n; i++) a->val[i] = vnum((double)(unsigned char)buf[i]);
    istk[++isp] = (Value){ .type = VAL_ARR, .arr = a };
    ip++; goto dispatch;
}

op_destructure: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int count = ins->a;
    Arr *slots = ins->arr;
    Value arr_val = istk[isp--];
    if (arr_val.type != VAL_ARR) die("destructure requires array");
    Arr *arr = arr_val.arr;
    int len = arr ? arr->len : 0;
    for (int i = 0; i < count; i++) {
        int slot = (int)val_num(slots->val[i]);
        if (i < len) {
            Value elem = arr->val[i];
            if (elem.type == VAL_ARR) aretain(elem.arr);
            vassign(&cs->v[slot], elem);
            if (elem.type == VAL_ARR) arelease(elem.arr);
        } else {
            vassign(&cs->v[slot], nilv());
        }
    }
    arelease(arr_val.arr);
    ip++; goto dispatch;
}

op_clear_slot: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    int slot = c->code[ip].a;
    Value *v = &cs->v[slot];
    if (v->type == VAL_ARR) arelease(v->arr);
    *v = nilv();
    ip++; goto dispatch;
}

op_mutate_num: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int slot = ins->a, depth = ins->b;
    int op = (int)ins->num;
    Value delta = istk[isp--];
    Value indices[16];
    for (int j = depth - 1; j >= 0; j--) indices[j] = istk[isp--];
    Value *sp = &cs->v[slot];
    for (int j = 0; j < depth; j++) {
        amake_uniq(sp);
        int ii = (int)val_num(indices[j]);
        if (sp->type != VAL_ARR || !sp->arr || ii < 0 || ii >= sp->arr->len)
            die("index out of bounds");
        sp = &sp->arr->val[ii];
    }
    if (sp->type == VAL_ARR) die("mutate on non-number");
    double old = sp->num;
    double d = delta.num;
    double result = 0.0;
    switch (op) {
        case T_PL: result = old + d; break;
        case T_MI: result = old - d; break;
        case T_ST: result = old * d; break;
        case T_SL: if (d == 0) die("division by zero"); result = old / d; break;
        default: die("invalid mutate op");
    }
    *sp = vnum(result);
    for (int j = 0; j < depth; j++)
        if (indices[j].type == VAL_ARR) arelease(indices[j].arr);
    ip++; goto dispatch;
}

    /* ── Central dispatch ── */
dispatch:
    switch (c->code[ip].op) {
    case OC_NUM: goto op_num;
    case OC_NIL: goto op_nil;
    case OC_STR: goto op_str;
    case OC_MAKE_ARR: goto op_make_arr;
    case OC_VAR: goto op_var;
    case OC_VAR_SLOT: goto op_var_slot;
    case OC_STORE_SLOT: goto op_store_slot;
    case OC_OP: goto op_op;
    case OC_ADD_NUM: goto op_add_num;
    case OC_SUB_NUM: goto op_sub_num;
    case OC_MUL_NUM: goto op_mul_num;
    case OC_DIV_NUM: goto op_div_num;
    case OC_UNARY: goto op_unary;
    case OC_INDEX: goto op_index;
    case OC_LVALS: goto op_lvals;
    case OC_PUSH: goto op_push;
    case OC_LVALS_PUSH: goto op_lvals_push;
    case OC_SLICE: goto op_slice;
    case OC_CALL: goto op_call;
    case OC_TCO: goto op_tco;
    case OC_RET: goto op_ret;
    case OC_POP: goto op_pop;
    case OC_DUP: goto op_dup;
    case OC_JZ: goto op_jz;
    case OC_JNZ: goto op_jnz;
    case OC_JMP: goto op_jmp;
    case OC_PRINT: goto op_print;
    case OC_INPUT: goto op_input;
    case OC_PUSH_ALL: goto op_push_all;
    case OC_SLICE_INPLACE: goto op_slice_inplace;
    case OC_DESTRUCTURE: goto op_destructure;
    case OC_MUTATE_NUM: goto op_mutate_num;
    case OC_CLEAR_SLOT: goto op_clear_slot;
    case OC_PROFILE: goto op_profile;
    case OC_END: goto op_end;
    }

op_end:
    return;

op_profile: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    int kind = (int)c->code[ip].num;
    switch (kind) {
        case 0: profile_user_calls++; break;
        case 1: profile_builtin_calls++; break;
        case 2: profile_tco_calls++; break;
        case 3: {
            /* COW: check slot refcount — if > 1, a deep copy is imminent */
            int slot = c->code[ip].a;
            if (cs->v[slot].type == VAL_ARR && cs->v[slot].arr && cs->v[slot].arr->refcount > 1) {
                profile_cow_copies++;
                profile_cow_bytes += cs->v[slot].arr->len * (int)sizeof(Value);
            }
            break;
        }
    }
    ip++; goto dispatch;
}
}

/* ─── Native builtin function implementations ─── */

static void reg_builtin(const char *name, int arity, ExprType ret_type, NativeFn fn) {
    if (fc >= fm) { fm = fm ? fm*2 : 16; fs = realloc(fs, fm*sizeof(Fn)); }
    Fn *f = &fs[fc++]; memset(f, 0, sizeof(Fn));
    f->n = strdup(name);
    f->a = arity;
    f->ret_type = ret_type;
    f->is_builtin = 1;
    f->native_fn = fn;
}

static Value native_split(int ac, Value *args) {
    (void)ac;
    Value str_v = args[0], sep_v = args[1];
    if (str_v.type != VAL_ARR) die("split requires string");
    Arr *str = str_v.arr;
    Arr *sep = (sep_v.type == VAL_ARR) ? sep_v.arr : NULL;
    int slen = str ? str->len : 0;
    int splen = sep ? sep->len : 0;
    /* Empty separator: split into individual characters as slices */
    if (splen == 0) {
        Arr *result = aalloc(slen > 0 ? slen : 1);
        result->len = slen > 0 ? slen : 1;
        result->is_string = 0;
        if (slen == 0) {
            Arr *empty = malloc(sizeof(Arr));
            empty->refcount = 1; empty->len = 0; empty->cap = 0; empty->val = NULL;
            empty->is_slice = 0; empty->parent = NULL; empty->hash_cache = 0; empty->is_string = 1;
            result->val[0] = (Value){ .type = VAL_ARR, .arr = empty };
        } else {
            for (int i = 0; i < slen; i++) {
                Arr *view = malloc(sizeof(Arr));
                view->refcount = 1; view->len = 1; view->cap = 1;
                view->val = str->val + i;
                view->is_slice = 1; view->parent = str;
                view->hash_cache = 0; view->is_string = 1;
                aretain(str);
                result->val[i] = (Value){ .type = VAL_ARR, .arr = view };
            }
        }
        arelease(str); if (sep) arelease(sep);
        return (Value){ .type = VAL_ARR, .arr = result };
    }
    int nsegs = 1;
    for (int i = 0; i <= slen - splen; i++) {
        int match = 1;
        for (int j = 0; j < splen; j++) {
            if ((int)val_num(str->val[i+j]) != (int)val_num(sep->val[j])) { match = 0; break; }
        }
        if (match) { nsegs++; i += splen - 1; }
    }
    Arr *result = aalloc(nsegs); result->len = nsegs;
    result->is_string = 0;
    int seg_idx = 0, start = 0;
    for (int i = 0; i <= slen - splen && seg_idx < nsegs - 1; i++) {
        int match = 1;
        for (int j = 0; j < splen; j++) {
            if ((int)val_num(str->val[i+j]) != (int)val_num(sep->val[j])) { match = 0; break; }
        }
        if (match) {
            int seg_len = i - start;
            Arr *view = malloc(sizeof(Arr));
            view->refcount = 1; view->len = seg_len; view->cap = seg_len;
            view->val = seg_len > 0 ? (str->val + start) : NULL;
            view->is_slice = seg_len > 0;
            view->parent = seg_len > 0 ? str : NULL;
            view->hash_cache = 0; view->is_string = 1;
            if (seg_len > 0) aretain(str);
            result->val[seg_idx++] = (Value){ .type = VAL_ARR, .arr = view };
            start = i + splen;
            i += splen - 1;
        }
    }
    int seg_len = slen - start;
    Arr *view = malloc(sizeof(Arr));
    view->refcount = 1; view->len = seg_len; view->cap = seg_len;
    view->val = seg_len > 0 ? (str->val + start) : NULL;
    view->is_slice = seg_len > 0;
    view->parent = seg_len > 0 ? str : NULL;
    view->hash_cache = 0; view->is_string = 1;
    if (seg_len > 0) aretain(str);
    result->val[seg_idx] = (Value){ .type = VAL_ARR, .arr = view };
    arelease(str); if (sep) arelease(sep);
    return (Value){ .type = VAL_ARR, .arr = result };
}

static Value native_env(int ac, Value *args) {
    (void)ac;
    Value name_v = args[0];
    if (name_v.type != VAL_ARR || !is_string_arr(name_v.arr)) die("env requires string");
    Arr *name_arr = name_v.arr;
    int nlen = name_arr ? name_arr->len : 0;
    char name[1024];
    for (int i = 0; i < nlen && i < 1023; i++) name[i] = (char)val_num(name_arr->val[i]);
    name[nlen < 1024 ? nlen : 1023] = '\0';
    const char *val = getenv(name);
    arelease(name_v.arr);
    Arr *a;
    if (val) {
        a = cstr_to_arr(val, strlen(val));
    } else {
        a = aalloc(0); a->len = 0; a->is_string = 1;
    }
    return (Value){ .type = VAL_ARR, .arr = a };
}

static Value native_args(int ac, Value *args) {
    (void)ac; (void)args;
    Arr *result = aalloc(saved_argc > 1 ? saved_argc - 1 : 0);
    result->len = saved_argc > 1 ? saved_argc - 1 : 0;
    result->is_string = 0;
    for (int i = 1; i < saved_argc; i++) {
        int vlen = strlen(saved_argv[i]);
        Arr *a = cstr_to_arr(saved_argv[i], vlen);
        result->val[i-1] = (Value){ .type = VAL_ARR, .arr = a };
    }
    return (Value){ .type = VAL_ARR, .arr = result };
}

static Value native_time(int ac, Value *args) {
    (void)ac; (void)args;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return vnum((double)ts.tv_sec + (double)ts.tv_nsec / 1.0e9);
}

static Value native_date(int ac, Value *args) {
    (void)ac; (void)args;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    Arr *a = aalloc(6); a->len = 6; a->is_string = 0;
    a->val[0] = vnum((double)(tm->tm_year + 1900));
    a->val[1] = vnum((double)(tm->tm_mon + 1));
    a->val[2] = vnum((double)tm->tm_mday);
    a->val[3] = vnum((double)tm->tm_hour);
    a->val[4] = vnum((double)tm->tm_min);
    a->val[5] = vnum((double)tm->tm_sec);
    return (Value){ .type = VAL_ARR, .arr = a };
}

static Value native_hash(int ac, Value *args) {
    (void)ac;
    Value v = args[0];
    if (v.type != VAL_ARR) die("hash requires string");
    unsigned int h = get_arr_hash(v.arr);
    arelease(v.arr);
    return vnum((double)h);
}

static Value native_sleep(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("sleep requires a number");
    double secs = args[0].num;
    if (secs < 0) die("sleep duration cannot be negative");
    struct timespec ts;
    ts.tv_sec = (time_t)secs;
    ts.tv_nsec = (long)((secs - (double)ts.tv_sec) * 1.0e9);
    nanosleep(&ts, NULL);
    return nilv();
}

static Value native_read(int ac, Value *args) {
    (void)ac;
    Value path_v = args[0], mode_v = args[1];
    if (path_v.type != VAL_ARR || !is_string_arr(path_v.arr)) die("read: filepath must be a string");
    if (mode_v.type != VAL_ARR || !is_string_arr(mode_v.arr)) die("read: mode must be a string");
    int plen = path_v.arr ? path_v.arr->len : 0;
    char path[1024];
    for (int i = 0; i < plen && i < 1023; i++) path[i] = (char)val_num(path_v.arr->val[i]);
    path[plen < 1024 ? plen : 1023] = '\0';
    int mlen = mode_v.arr ? mode_v.arr->len : 0;
    char mode[4] = {0};
    for (int i = 0; i < mlen && i < 3; i++) mode[i] = (char)val_num(mode_v.arr->val[i]);
    FILE *f = fopen(path, mode);
    if (!f) die("read: cannot open '%s'", path);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz > 0 ? (int)sz : 1);
    fread(buf, 1, sz, f);
    fclose(f);
    Arr *a = cstr_to_arr(buf, (int)sz);
    free(buf);
    arelease(path_v.arr);
    arelease(mode_v.arr);
    return (Value){ .type = VAL_ARR, .arr = a };
}

static Value native_write(int ac, Value *args) {
    (void)ac;
    Value path_v = args[0], data_v = args[1], mode_v = args[2];
    if (path_v.type != VAL_ARR || !is_string_arr(path_v.arr)) die("write: filepath must be a string");
    if (data_v.type != VAL_ARR || !is_string_arr(data_v.arr)) die("write: data must be a string");
    if (mode_v.type != VAL_ARR || !is_string_arr(mode_v.arr)) die("write: mode must be a string");
    int plen = path_v.arr ? path_v.arr->len : 0;
    char path[1024];
    for (int i = 0; i < plen && i < 1023; i++) path[i] = (char)val_num(path_v.arr->val[i]);
    path[plen < 1024 ? plen : 1023] = '\0';
    int dlen = data_v.arr ? data_v.arr->len : 0;
    int mlen = mode_v.arr ? mode_v.arr->len : 0;
    char mode[4] = {0};
    for (int i = 0; i < mlen && i < 3; i++) mode[i] = (char)val_num(mode_v.arr->val[i]);
    FILE *f = fopen(path, mode);
    if (!f) die("write: cannot open '%s'", path);
    for (int i = 0; i < dlen; i++) {
        putc((int)val_num(data_v.arr->val[i]), f);
    }
    fclose(f);
    arelease(path_v.arr);
    arelease(data_v.arr);
    arelease(mode_v.arr);
    return nilv();
}

static Value native_exec(int ac, Value *args) {
    (void)ac;
    Value cmd_v = args[0];
    if (cmd_v.type != VAL_ARR || !is_string_arr(cmd_v.arr)) die("exec: command must be a string");
    int clen = cmd_v.arr ? cmd_v.arr->len : 0;
    char cmd[4096];
    for (int i = 0; i < clen && i < 4095; i++) cmd[i] = (char)val_num(cmd_v.arr->val[i]);
    cmd[clen < 4096 ? clen : 4095] = '\0';
    FILE *p = popen(cmd, "r");
    if (!p) die("exec: cannot run command");
    /* Read output in chunks */
    int cap = 1024, len = 0;
    char *buf = malloc(cap);
    while (1) {
        int n = fread(buf + len, 1, cap - len, p);
        len += n;
        if (len >= cap) { cap *= 2; buf = realloc(buf, cap); }
        if (feof(p)) break;
    }
    int status = pclose(p);
    (void)status;
    Arr *a = cstr_to_arr(buf, len);
    free(buf);
    arelease(cmd_v.arr);
    return (Value){ .type = VAL_ARR, .arr = a };
}

/* ─── Maths helpers for sort ─── */

static int vlt(Value a, Value b) {
    if (a.type == VAL_NUM && b.type == VAL_NUM) return a.num < b.num;
    if (a.type == VAL_NUM) return 1;
    if (b.type == VAL_NUM) return 0;
    if (!a.arr && !b.arr) return 0;
    if (!a.arr) return 1;
    if (!b.arr) return 0;
    if (a.arr->is_string && b.arr->is_string) {
        int m = a.arr->len < b.arr->len ? a.arr->len : b.arr->len;
        for (int i = 0; i < m; i++) {
            double ca = val_num(a.arr->val[i]), cb = val_num(b.arr->val[i]);
            if (ca != cb) return ca < cb;
        }
        return a.arr->len < b.arr->len;
    }
    if (a.arr->len != b.arr->len) return a.arr->len < b.arr->len;
    for (int i = 0; i < a.arr->len; i++) {
        if (!veq(a.arr->val[i], b.arr->val[i])) return vlt(a.arr->val[i], b.arr->val[i]);
    }
    return 0;
}

static int value_cmp(const void *a, const void *b) {
    const Value *va = (const Value *)a, *vb = (const Value *)b;
    if (vlt(*va, *vb)) return -1;
    if (vlt(*vb, *va)) return 1;
    return 0;
}

static void flatten_recursive(Arr *arr, Arr *result) {
    if (!arr) return;
    for (int i = 0; i < arr->len; i++) {
        Value v = arr->val[i];
        if (v.type == VAL_ARR && v.arr && !v.arr->is_string) {
            flatten_recursive(v.arr, result);
        } else {
            int idx = result->len;
            if (idx >= result->cap) {
                result->cap = result->cap ? result->cap * 2 : 16;
                result->val = realloc(result->val, result->cap * sizeof(Value));
            }
            result->val[idx] = v;
            if (v.type == VAL_ARR) aretain(v.arr);
            result->len++;
        }
    }
}

/* ─── Math builtins ─── */

static Value native_sin(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("sin: argument must be a number");
    return vnum(sin(args[0].num));
}

static Value native_cos(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("cos: argument must be a number");
    return vnum(cos(args[0].num));
}

static Value native_sqrt(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("sqrt: argument must be a number");
    if (args[0].num < 0) die("sqrt: negative argument");
    return vnum(sqrt(args[0].num));
}

static Value native_exp(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("exp: argument must be a number");
    return vnum(exp(args[0].num));
}

static Value native_log(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("log: argument must be a number");
    if (args[0].num <= 0) die("log: argument must be positive");
    return vnum(log(args[0].num));
}

static Value native_floor(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("floor: argument must be a number");
    return vnum(floor(args[0].num));
}

static Value native_ceil(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("ceil: argument must be a number");
    return vnum(ceil(args[0].num));
}

static Value native_round(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM) die("round: argument must be a number");
    return vnum(round(args[0].num));
}

/* ─── Random number builtin ─── */

static int rand_seeded = 0;

static Value native_rand(int ac, Value *args) {
    (void)ac;
    if (args[0].type != VAL_NUM || args[1].type != VAL_NUM) die("rand: min and max must be numbers");
    if (!rand_seeded) { srand((unsigned int)(time(NULL) ^ ((long)getpid() << 16))); rand_seeded = 1; }
    double min = args[0].num, max = args[1].num;
    if (min >= max) die("rand: min must be less than max");
    double r = (double)rand() / (double)RAND_MAX;
    return vnum(min + r * (max - min));
}

/* ─── Data structure builtins ─── */

static Value native_sort(int ac, Value *args) {
    (void)ac;
    Value arr_v = args[0];
    if (arr_v.type != VAL_ARR) die("sort: argument must be an array");
    if (!arr_v.arr) { /* empty/nil array */
        Arr *a = aalloc(0); a->len = 0; a->is_string = 0;
        return (Value){ .type = VAL_ARR, .arr = a };
    }
    Arr *result = adeep_copy(arr_v.arr);
    qsort(result->val, result->len, sizeof(Value), value_cmp);
    arelease(arr_v.arr);
    return (Value){ .type = VAL_ARR, .arr = result };
}

static Value native_set(int ac, Value *args) {
    (void)ac;
    Value arr_v = args[0];
    if (arr_v.type != VAL_ARR) die("set: argument must be an array");
    if (!arr_v.arr) {
        Arr *a = aalloc(0); a->len = 0; a->is_string = 0;
        return (Value){ .type = VAL_ARR, .arr = a };
    }
    Arr *src = arr_v.arr;
    int n = src->len;
    Arr *result = aalloc(n); result->len = 0; result->is_string = 0;
    for (int i = 0; i < n; i++) {
        Value v = src->val[i];
        int found = 0;
        for (int j = 0; j < result->len; j++) {
            if (veq(v, result->val[j])) { found = 1; break; }
        }
        if (!found) {
            int idx = result->len;
            result->val[idx] = v;
            if (v.type == VAL_ARR) aretain(v.arr);
            result->len++;
        }
    }
    arelease(arr_v.arr);
    return (Value){ .type = VAL_ARR, .arr = result };
}

static Value native_flat(int ac, Value *args) {
    (void)ac;
    Value arr_v = args[0];
    if (arr_v.type != VAL_ARR) die("flat: argument must be an array");
    if (!arr_v.arr) {
        Arr *a = aalloc(0); a->len = 0; a->is_string = 0;
        return (Value){ .type = VAL_ARR, .arr = a };
    }
    Arr *result = aalloc(16); result->len = 0; result->is_string = 0;
    flatten_recursive(arr_v.arr, result);
    arelease(arr_v.arr);
    return (Value){ .type = VAL_ARR, .arr = result };
}

/* ─── Terminal / keyboard builtins ─── */

static int orig_termios_valid = 0;
static struct termios orig_termios;

static void restore_terminal(void) {
    if (orig_termios_valid) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        orig_termios_valid = 0;
    }
}

static Value native_key(int ac, Value *args) {
    (void)ac; (void)args;
    int fd = STDIN_FILENO;
    struct termios raw, old;
    int n = 0;
    char buf[8];

    if (tcgetattr(fd, &old) != 0) {
        /* Not a terminal — read one byte anyway */
        char c;
        if (read(fd, &c, 1) <= 0) {
            Arr *empty = aalloc(0); empty->len = 0; empty->is_string = 1;
            return (Value){ .type = VAL_ARR, .arr = empty };
        }
        n = 1; buf[0] = c;
        goto done;
    }

    /* Save terminal state for atexit restore */
    if (!orig_termios_valid) {
        orig_termios = old;
        orig_termios_valid = 1;
        atexit(restore_terminal);
    }

    /* Set raw mode */
    raw = old;
    cfmakeraw(&raw);
    /* VMIN=1, VTIME=0: blocking read of 1 byte */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSAFLUSH, &raw);

    /* Read first byte */
    if (read(fd, buf, 1) <= 0) {
        tcsetattr(fd, TCSAFLUSH, &old);
        Arr *empty = aalloc(0); empty->len = 0; empty->is_string = 1;
        return (Value){ .type = VAL_ARR, .arr = empty };
    }
    n = 1;

    /* If ESC, try to read escape sequence with 100ms timeout */
    if (buf[0] == 0x1b) {
        struct termios esc_attr;
        tcgetattr(fd, &esc_attr);
        esc_attr.c_cc[VMIN] = 0;
        esc_attr.c_cc[VTIME] = 1;   /* 100ms timeout */
        tcsetattr(fd, TCSANOW, &esc_attr);

        while (n < 8) {
            char c;
            if (read(fd, &c, 1) <= 0) break;
            buf[n++] = c;
        }
    }

    /* Restore terminal */
    tcsetattr(fd, TCSAFLUSH, &old);

done:;
    Arr *a = cstr_to_arr(buf, n);
    return (Value){ .type = VAL_ARR, .arr = a };
}

/* ─── System builtins ─── */

static Value native_die(int ac, Value *args) {
    int code = 1;
    if (ac >= 1) {
        if (args[0].type != VAL_NUM) die("die: code must be a number");
        code = (int)args[0].num;
    }
    exit(code);
    return nilv();
}

static Value native_glob(int ac, Value *args) {
    (void)ac;
    Value pat_v = args[0];
    if (pat_v.type != VAL_ARR || !is_string_arr(pat_v.arr)) die("glob: pattern must be a string");
    int plen = pat_v.arr ? pat_v.arr->len : 0;
    char pattern[1024];
    for (int i = 0; i < plen && i < 1023; i++) pattern[i] = (char)val_num(pat_v.arr->val[i]);
    pattern[plen < 1024 ? plen : 1023] = '\0';
    glob_t g;
    int ret = glob(pattern, 0, NULL, &g);
    if (ret != 0) {
        arelease(pat_v.arr);
        Arr *a = aalloc(0); a->len = 0; a->is_string = 0;
        return (Value){ .type = VAL_ARR, .arr = a };
    }
    Arr *result = aalloc((int)g.gl_pathc);
    result->len = (int)g.gl_pathc;
    result->is_string = 0;
    for (size_t i = 0; i < g.gl_pathc; i++) {
        int vlen = strlen(g.gl_pathv[i]);
        Arr *s = cstr_to_arr(g.gl_pathv[i], vlen);
        result->val[i] = (Value){ .type = VAL_ARR, .arr = s };
    }
    globfree(&g);
    arelease(pat_v.arr);
    return (Value){ .type = VAL_ARR, .arr = result };
}

static void wr_le32(int32_t v) {
    unsigned char buf[4] = { v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff };
    fwrite(buf, 1, 4, stdout);
}
static void wr_le64(int64_t v) {
    unsigned char buf[8] = { v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff,
        (v >> 32) & 0xff, (v >> 40) & 0xff, (v >> 48) & 0xff, (v >> 56) & 0xff };
    fwrite(buf, 1, 8, stdout);
}
static void wr_str(const char *s) {
    int32_t len = s ? (int32_t)strlen(s) : -1;
    wr_le32(len);
    if (s) fwrite(s, 1, len, stdout);
}
static void wr_arr(const Arr *a) {
    int32_t len = a ? a->len : -1;
    wr_le32(len);
    if (a) for (int j = 0; j < a->len; j++) wr_le64((int64_t)val_num(a->val[j]));
}
static const char *op_name(OC op) {
    switch (op) {
        case OC_NUM: return "NUM"; case OC_NIL: return "NIL"; case OC_STR: return "STR";
        case OC_MAKE_ARR: return "MAKE_ARR"; case OC_VAR: return "VAR"; case OC_VAR_SLOT: return "VAR_SLOT";
        case OC_STORE_SLOT: return "STORE_SLOT"; case OC_OP: return "OP"; case OC_ADD_NUM: return "ADD_NUM";
        case OC_SUB_NUM: return "SUB_NUM"; case OC_MUL_NUM: return "MUL_NUM"; case OC_DIV_NUM: return "DIV_NUM";
        case OC_UNARY: return "UNARY"; case OC_INDEX: return "INDEX"; case OC_CALL: return "CALL";
        case OC_TCO: return "TCO"; case OC_JZ: return "JZ"; case OC_JMP: return "JMP";
        case OC_RET: return "RET"; case OC_POP: return "POP"; case OC_LVALS: return "LVALS";
        case OC_PUSH: return "PUSH"; case OC_LVALS_PUSH: return "LVALS_PUSH"; case OC_SLICE: return "SLICE";
        case OC_PRINT: return "PRINT"; case OC_INPUT: return "INPUT"; case OC_DUP: return "DUP";
        case OC_JNZ: return "JNZ"; case OC_PUSH_ALL: return "PUSH_ALL"; case OC_SLICE_INPLACE: return "SLICE_IP";
        case OC_DESTRUCTURE: return "DEST"; case OC_MUTATE_NUM: return "MUTATE"; case OC_CLEAR_SLOT: return "CLEAR"; case OC_PROFILE: return "PROF";
        case OC_END: return "END"; default: return "???";
    }
}

static void bytecode_stats(void) {
    int total_instrs = 0;
    int total_calls = 0;
    int total_ret = 0;
    int fn_count = 0;
    for (int i = 0; i < fc; i++) {
        Fn *f = &fs[i];
        if (!f->code) continue;
        fn_count++;
        int count = f->code->len;
        total_instrs += count;
        printf("  %s: %d instrs", f->n, count);
        if (f->is_inlinable) {
            printf(" [inlinable]");
        }
        printf("\n");
        for (int j = 0; j < count; j++) {
            Instr *ins = &f->code->code[j];
            printf("    %4d: %s", j, op_name(ins->op));
            if (ins->op == OC_CALL || ins->op == OC_TCO) {
                printf(" %s(%d)", fs[ins->a].n, ins->b);
                total_calls++;
            } else if (ins->op == OC_NUM) {
                printf(" %.0f", ins->num);
            } else if (ins->op == OC_VAR_SLOT || ins->op == OC_STORE_SLOT ||
                       ins->op == OC_CLEAR_SLOT || ins->op == OC_MUTATE_NUM) {
                printf(" slot=%d", ins->a);
            } else if (ins->op == OC_LVALS || ins->op == OC_PUSH || ins->op == OC_LVALS_PUSH ||
                       ins->op == OC_PUSH_ALL || ins->op == OC_SLICE_INPLACE) {
                printf(" slot=%d", ins->a);
            } else if (ins->op == OC_RET) {
                total_ret++;
            } else if (ins->op == OC_OP) {
                printf(" op=%d", ins->a);
            } else if (ins->op == OC_JZ || ins->op == OC_JNZ || ins->op == OC_JMP) {
                printf(" ->%d", ins->a);
            } else if (ins->op == OC_MAKE_ARR || ins->op == OC_DESTRUCTURE) {
                printf(" n=%d", ins->a);
            } else if (ins->op == OC_UNARY) {
                printf(" op=%d", ins->a);
            }
            printf("\n");
        }
    }
    printf("\n");
    printf("  Functions: %d\n", fn_count);
    printf("  Total instructions: %d\n", total_instrs);
    printf("  Total OC_CALL instrs: %d\n", total_calls);
    printf("  Total OC_RET instrs: %d\n", total_ret);
}

static void dump_binary(Code *c, const char *label) {
    int32_t count = c ? c->len : 0;
    wr_str(label);
    wr_le32(count);
    if (!c) return;
    for (int i = 0; i < c->len; i++) {
        Instr *ins = &c->code[i];
        wr_le32((int32_t)ins->op);
        wr_le32(ins->a);
        wr_le32(ins->b);
        { int64_t tmp; memcpy(&tmp, &ins->num, 8); wr_le64(tmp); }
        wr_le32(ins->line);
        wr_str(ins->name);
        wr_str(ins->file);
        wr_arr(ins->arr);
    }
}
static void dump_all_binary(void) {
    for (int i = 0; i < fc; i++)
        dump_binary(fs[i].code, fs[i].n);
}

/* ─── Main ─── */

int main(int a, char **v) {
    saved_argc = a; saved_argv = v;
    cs = snew(); cur_fi = -1;
    /* Register builtin functions */
    reg_builtin("split", 2, T_ARR_TYPE, native_split);
    reg_builtin("env", 1, T_STR_TYPE, native_env);
    reg_builtin("args", 0, T_ARR_TYPE, native_args);
    reg_builtin("time", 0, T_NUM_TYPE, native_time);
    reg_builtin("date", 0, T_ARR_TYPE, native_date);
    reg_builtin("hash", 1, T_NUM_TYPE, native_hash);
    reg_builtin("sleep", 1, T_ARR_TYPE, native_sleep);
    reg_builtin("read", 2, T_STR_TYPE, native_read);
    reg_builtin("write", 3, T_ARR_TYPE, native_write);
    reg_builtin("exec", 1, T_STR_TYPE, native_exec);
    reg_builtin("sin", 1, T_NUM_TYPE, native_sin);
    reg_builtin("cos", 1, T_NUM_TYPE, native_cos);
    reg_builtin("sqrt", 1, T_NUM_TYPE, native_sqrt);
    reg_builtin("exp", 1, T_NUM_TYPE, native_exp);
    reg_builtin("log", 1, T_NUM_TYPE, native_log);
    reg_builtin("floor", 1, T_NUM_TYPE, native_floor);
    reg_builtin("ceil", 1, T_NUM_TYPE, native_ceil);
    reg_builtin("round", 1, T_NUM_TYPE, native_round);
    reg_builtin("rand", 2, T_NUM_TYPE, native_rand);
    reg_builtin("sort", 1, T_ARR_TYPE, native_sort);
    reg_builtin("set", 1, T_ARR_TYPE, native_set);
    reg_builtin("flat", 1, T_ARR_TYPE, native_flat);
    reg_builtin("die", 1, T_ARR_TYPE, native_die);
    reg_builtin("glob", 1, T_ARR_TYPE, native_glob);
    reg_builtin("key", 0, T_STR_TYPE, native_key);
    if (a >= 2) {
        /* Parse flags */
        char *file = NULL;
        for (int i = 1; i < a; i++) {
            if (!strcmp(v[i], "--bytecode")) show_bytecode = 1;
            else if (!strcmp(v[i], "--profile")) profile_flag = 1;
            else file = v[i];
        }
        if (!file) { fprintf(stderr, "usage: tiny [--bytecode] [--profile] <file>\n"); return 1; }
        char *src = readf(file); if (!src) { fprintf(stderr, "cannot read '%s'\n", file); return 1; }
        char dir[1024] = {0}; const char *slash = strrchr(file, '/');
        if (slash) { memcpy(dir, file, slash - file); } include_dir = dir;
        comp_file = strdup(file);
        lex(src); Code *code = new_code();
        comp_vars = NULL; comp_types = NULL; comp_vc = 0; comp_vm = 0;
        comp_program(code); free(src); free(comp_file); comp_file = NULL;
        /* Grow scope if needed for top-level variable slots */
        if (comp_vc > cs->c) {
            Scp *new_cs = snew_sized(comp_vc);
            for (int i = 0; i < cs->c; i++) {
                new_cs->n[i] = cs->n[i];
                new_cs->v[i] = cs->v[i];
            }
            free(cs->n); free(cs->v); free(cs);
            cs = new_cs;
        }
        for (int i = 0; i < comp_vc; i++) {
            if (!cs->n[i]) cs->n[i] = strdup(comp_vars[i]);
            if ((comp_types[i] == T_ARR_TYPE || comp_types[i] == T_STR_TYPE) && cs->v[i].type == VAL_NUM)
                cs->v[i] = (Value){ .type = VAL_ARR };
        }
        if (show_bytecode) { bytecode_stats(); return 0; }
        exec(code);
        if (profile_user_calls || profile_builtin_calls || profile_tco_calls || profile_cow_copies) {
            fprintf(stderr, "\n--- Profile ---\n");
            fprintf(stderr, "  User function calls:  %ld\n", profile_user_calls);
            fprintf(stderr, "  Builtin calls:        %ld\n", profile_builtin_calls);
            fprintf(stderr, "  Tail calls (TCO):     %ld\n", profile_tco_calls);
            fprintf(stderr, "  Total calls:          %ld\n",
                    profile_user_calls + profile_builtin_calls + profile_tco_calls);
            fprintf(stderr, "  COW deep copies:      %ld\n", profile_cow_copies);
            fprintf(stderr, "  COW bytes copied:     %ld\n", profile_cow_bytes);
            fprintf(stderr, "\n");
        }
        code_free(code); free(ts);
    } else {
        char buf[65536];
#ifdef READLINE
        rl_initialize();
#endif
        while (1) {
            buf[0] = 0;
#ifdef READLINE
            {
                char *rline = readline("> ");
                if (!rline) { printf("\n"); break; }
                strcat(buf, rline);
                strcat(buf, "\n");
                free(rline);
                /* Empty line — skip */
                if (!buf[0] || (buf[0] == '\n' && buf[1] == 0)) continue;
                /* Continue reading until braces balance (multi-line input) */
                int op = 0, cl = 0;
                for (char *p = buf; *p; p++) { if (*p == '{') op++; if (*p == '}') cl++; }
                while (op != cl) {
                    rline = readline("  ");
                    if (!rline) break;
                    strcat(buf, rline);
                    strcat(buf, "\n");
                    free(rline);
                    op = 0; cl = 0;
                    for (char *p = buf; *p; p++) { if (*p == '{') op++; if (*p == '}') cl++; }
                }
                add_history(buf);
            }
#else
            printf("> "); fflush(stdout);
            while (1) {
                char line[4096];
                if (!fgets(line, sizeof(line), stdin)) { printf("\n"); goto done; }
                strcat(buf, line);
                int op = 0, cl = 0;
                for (char *p = buf; *p; p++) { if (*p == '{') op++; if (*p == '}') cl++; }
                if (op == cl) break;
                printf("  "); fflush(stdout);
            }
#endif
            comp_file = NULL;
            lex(buf); Code *code = new_code();
            comp_program(code);
            if (comp_vc > cs->c) {
                Scp *new_cs = snew_sized(comp_vc);
                for (int i = 0; i < cs->c; i++) {
                    new_cs->n[i] = cs->n[i];
                    new_cs->v[i] = cs->v[i];
                }
                free(cs->n); free(cs->v); free(cs);
                cs = new_cs;
            }
            for (int i = 0; i < comp_vc; i++) {
                if (!cs->n[i]) cs->n[i] = strdup(comp_vars[i]);
                if ((comp_types[i] == T_ARR_TYPE || comp_types[i] == T_STR_TYPE) && cs->v[i].type == VAL_NUM)
                    cs->v[i] = (Value){ .type = VAL_ARR };
            }
            { int _sr = repl_catching; repl_catching = 1;
              if (setjmp(repl_jmp)) { isp = -1; rf = 0; call_depth = -1; }
              else { isp = -1; exec(code); printf("\n"); }
              repl_catching = _sr; }
            code_free(code); free(ts);
        }
#ifndef READLINE
        done:;
#endif
    }
    if (comp_vars) { for (int i = 0; i < comp_vc; i++) free(comp_vars[i]); free(comp_vars); free(comp_types); }
    sfree(cs); return 0;
}
