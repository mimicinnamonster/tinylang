/* tinylang.c — VM with computed goto dispatch + slot-indexed variables
 * Removed: compact types, FFI, assert, type(), 0b/0123 / OC_SLICE_ASSIGN
 * Kept: TCO, COW+refcounting, push optimization, short-circuit && ||,
 *        0x hex, slices, thispath, input, REPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#ifdef READLINE
#  include <readline/readline.h>
#  include <readline/history.h>
#endif

typedef enum { VAL_NUM, VAL_ARR } Type;
typedef struct Arr { int refcount, len, cap; struct Value *val; int is_slice; unsigned int hash_cache; struct Arr *parent; } Arr;
typedef struct Value {
    Type type;
    double num;
    Arr *arr;
} Value;

typedef enum {
    T_EOF, T_NUM, T_ID, T_STR, T_NIL,
    T_LP, T_RP, T_LB, T_RB, T_LC, T_RC, T_CM, T_SEMI,
    T_PL, T_MI, T_ST, T_SL, T_PC,
    T_AM, T_PI, T_CA, T_SHL, T_SHR, T_BN,
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
    OC_OP, OC_UNARY, OC_INDEX,
    OC_CALL, OC_TCO,
    OC_JZ, OC_JMP, OC_RET, OC_POP,
    OC_LVALS, OC_PUSH, OC_LVALS_PUSH, OC_SLICE,
    OC_PRINT, OC_INPUT,
    OC_DUP, OC_JNZ,
    OC_PUSH_ALL,
    OC_SLICE_INPLACE,
    OC_DESTRUCTURE,
    OC_END,
} OC;

typedef struct Code { struct Instr *code; int len, cap; } Code;
typedef struct Instr {
    OC op; int a, b; double num; Arr *arr; char *name; Code *sub; int line; char *file;
} Instr;

Tok *ts; int tc, tp;
Scp *cs;
int rf; Value rv;
Value istk[4096]; int isp;
int cur_fi;

static char *include_dir;
char *comp_file; int comp_line;
char *err_file; int err_line;
typedef struct { int fi; int line; char *file; } CallFrame;
CallFrame call_stack[128]; int call_depth = -1;

typedef enum { T_UNKNOWN = 0, T_NUM_TYPE = 1, T_ARR_TYPE = 2 } ExprType;
typedef struct { char *n; int a; int nvars, *p_slots; ExprType ret_type, *init_types; Code *code; Value *def_vals; char *has_def; } Fn;
Fn *fs; int fc, fm;

jmp_buf repl_jmp; int repl_catching;

/* Compile-time variable tracking: name → slot index + type */
static char **comp_vars;
static ExprType *comp_types;
static int comp_vc, comp_vm;

/* Compile-time return type tracking for current function */
static ExprType fn_ret_type;
static int fn_ret_seen;

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
    a->is_slice = 0; a->parent = NULL;
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
Value arr_item(Arr *a, int i) { return a->val[i]; }

int is_string_arr(Arr *a) {
    if (!a) return 0;
    for (int i = 0; i < a->len; i++) {
        if (a->val[i].type != VAL_NUM) return 0;
        int c = (int)val_num(a->val[i]);
        if (c != 10 && c != 13 && c != 9 && (c < 32 || c > 126)) return 0;
    }
    return 1;
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
 * avoiding the intermediate Arr allocation that num_to_string_arr creates. */
Arr *strcat_num(Arr *la, double d) {
    char buf[64];
    int rn = num_to_buf(buf, d);
    int ln = la ? la->len : 0;
    Arr *a = aalloc(ln + rn); a->len = ln + rn;
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
        int is_str = a ? 1 : 0;
        if (a) {
            for (int i = 0; i < a->len && is_str; i++) {
                if (a->val[i].type != VAL_NUM) { is_str = 0; break; }
                int c = (int)val_num(a->val[i]);
                if (c != 10 && c != 13 && c != 9 && (c < 32 || c > 126)) is_str = 0;
            }
            if (is_str) {
                for (int i = 0; i < a->len; i++) putchar((int)val_num(a->val[i]));
                return;
            }
        }
        putchar('[');
        if (a) for (int i = 0; i < a->len; i++) {
            if (i) printf(", "); print_val(a->val[i]);
        }
        putchar(']');
    }
}
int truthy(Value v) { return v.type != VAL_ARR || (v.arr && v.arr->len > 0); }
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
        code_free(c->code[i].sub);
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
            else if (!strcmp(b,"while")){tk.t=T_WH;free(tk.s);}
            else if (!strcmp(b,"function")){tk.t=T_FN;free(tk.s);}
            else if (!strcmp(b,"return")){tk.t=T_RT;free(tk.s);}
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
            Arr *a = aalloc(i); a->len = i;
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
        case T_NUM: tp++; emit(c, (Instr){OC_NUM, 0, 0, .num = t.n}); break;
        case T_NIL: tp++; emit(c, (Instr){OC_NIL, 0, 0, .num = 0}); break;
        case T_STR: { tp++; Arr *o = (Arr*)t.s; emit(c, (Instr){OC_STR, 0, 0, .arr = o}); break; }
        case T_ID: {
            char *nm = strdup(t.s); tp++;
            if (ts[tp].t == T_LP) {
                free(nm); tp++;
                int ac = 0;
                if (ts[tp].t != T_RP) { do { comp_expr(c); ac++; } while (ts[tp].t == T_CM && (tp++, 1)); }
                tp++;
                if (!strcmp(t.s, "print")) { if (ac < 1) die("print needs 1 arg"); emit(c, (Instr){OC_PRINT, 0, 0, .num = 0}); }
                else if (!strcmp(t.s, "input")) emit(c, (Instr){OC_INPUT, 0, 0, .num = 0});
                else if (!strcmp(t.s, "thispath")) {
                    int n = comp_file ? strlen(comp_file) : 0;
                    Arr *a = aalloc(n); a->len = n;
                    for (int j = 0; j < n; j++) a->val[j] = vnum((double)(unsigned char)comp_file[j]);
                    emit(c, (Instr){OC_STR, 0, 0, .arr = a});
                } else {
                    int fi = ffind(t.s); if (fi < 0) die("undefined function '%s'", t.s);
                    emit(c, (Instr){OC_CALL, fi, ac, .num = 0});
                }
            } else {
                /* Variable read — use slot index in function bodies, name at top-level */
                int slot = var_find(nm);
                if (slot >= 0) {
                    emit(c, (Instr){OC_VAR_SLOT, slot, 0, .num = 0});
                } else if (cur_fi >= 0) {
                    die("undefined variable '%s'", nm);
                } else {
                    emit(c, (Instr){OC_VAR, 0, 0, .name = nm});
                    nm = NULL;
                }
                free(nm);
            }
            break;
        }
        case T_LB: {
            tp++;
            if (ts[tp].t == T_RB) { tp++; emit(c, (Instr){OC_NIL, 0, 0, .num = 0}); break; }
            int n = 0;
            do { comp_expr(c); n++; } while (ts[tp].t == T_CM && (tp++, 1));
            if (ts[tp].t != T_RB) die("expected ]"); tp++;
            emit(c, (Instr){OC_MAKE_ARR, n, 0, .num = 0}); break;
        }
        case T_LP: { tp++; comp_expr(c); if (ts[tp].t != T_RP) die("expected )"); tp++; break; }
        case T_BN: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_BN, 0, .num = 0}); break;
        case T_MI: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_MI, 0, .num = 0}); break;
        case T_HASH: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_HASH, 0, .num = 0}); break;
        default: die("unexpected token at line %d", t.l);
    }
    while (ts[tp].t == T_LB) {
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
            comp_expr_prec(c, op_prec(op) + 1);
            emit(c, (Instr){OC_OP, op, 0, .num = 0});
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
        set_var_type(slot, def_vals[i].type == VAL_NUM ? T_NUM_TYPE : T_ARR_TYPE);
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
    f->has_def = NULL;

    /* Compile body */
    fn_ret_type = T_UNKNOWN; fn_ret_seen = 0;
    Code *body = new_code(); comp_block(body); emit(body, (Instr){OC_END, 0, 0, .num = 0});
    f->code = body;
    f->nvars = comp_vc;
    f->ret_type = fn_ret_seen ? fn_ret_type : T_ARR_TYPE;

    /* Save per-variable init types before freeing */
    f->init_types = malloc(comp_vc * sizeof(ExprType));
    memcpy(f->init_types, comp_types, comp_vc * sizeof(ExprType));

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
 * Supports: string literals, thispath(), and + concatenation.
 * thispath() inside include returns the directory of the current file
 * (with trailing /) so concatenation with a relative path "just works".
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
    } else if (ts[tp].t == T_ID && !strcmp(ts[tp].s, "thispath")) {
        tp++;
        if (ts[tp].t != T_LP) die("expected ( after thispath");
        tp++;
        if (ts[tp].t != T_RP) die("expected ) after thispath");
        tp++;
        /* Return the directory of the current file (with trailing /) */
        if (comp_file) {
            const char *sl = strrchr(comp_file, '/');
            if (sl) {
                int dlen = sl - comp_file + 1;  /* include the slash */
                result = malloc(dlen + 1);
                memcpy(result, comp_file, dlen);
                result[dlen] = '\0';
            } else {
                result = strdup("");
            }
        } else {
            result = strdup("");
        }
    } else {
        die("include requires a string literal or thispath() expression");
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
        } else if (ts[tp].t == T_ID && !strcmp(ts[tp].s, "thispath")) {
            tp++;
            if (ts[tp].t != T_LP) die("expected ( after thispath");
            tp++;
            if (ts[tp].t != T_RP) die("expected ) after thispath");
            tp++;
            if (comp_file) {
                const char *sl = strrchr(comp_file, '/');
                if (sl) {
                    int dlen = sl - comp_file + 1;
                    right = malloc(dlen + 1);
                    memcpy(right, comp_file, dlen);
                    right[dlen] = '\0';
                } else {
                    right = strdup("");
                }
            } else {
                right = strdup("");
            }
        } else {
            die("include concatenation requires string literal or thispath()");
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
            /* nil, string, or bracket literal — all produce arrays */
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
            t = T_ARR_TYPE;
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
                int fi = ffind(nm);
                t = (fi >= 0) ? fs[fi].ret_type : T_UNKNOWN;
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
            /* +: num+num=num; arr+arr=arr; arr+num=arr; num+arr=arr */
            t = (t == T_NUM_TYPE && rt == T_NUM_TYPE) ? T_NUM_TYPE : T_ARR_TYPE;
        } else if (op == T_ST) {
            /* *: num*num=num; arr*num=arr (repeat) */
            t = (t == T_ARR_TYPE && rt == T_NUM_TYPE) ? T_ARR_TYPE : T_NUM_TYPE;
        } else {
            /* All other binary ops produce numbers */
            t = T_NUM_TYPE;
        }
    }
    return t;
}

void comp_stmt(Code *c) {
    while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
    if (ts[tp].t == T_EOF || ts[tp].t == T_RC) return;
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
                /* Rewrite compound into plain: x op= RHS → x = x op RHS (incl indices) */
                if (compound_op) {
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
                            if (idx_count > 0)
                                emit(c, (Instr){OC_LVALS_PUSH, slot, idx_count, .num = 0});
                            else
                                emit(c, (Instr){OC_PUSH, slot, 0, .num = 0});
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
                            emit(c, (Instr){OC_LVALS, slot, idx_count, .num = 0});
                        } else {
                            int pn = tp;
                            ExprType store_type = peek_expr_type(&pn);
                            comp_expr(c);
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

/* ─── VM Executor (computed goto dispatch) ─── */

void exec(Code *c) {
    static void *dispatch[] = {
        [OC_NUM] = &&op_num, [OC_NIL] = &&op_nil, [OC_STR] = &&op_str,
        [OC_MAKE_ARR] = &&op_make_arr,
        [OC_VAR] = &&op_var,
        [OC_VAR_SLOT] = &&op_var_slot, [OC_STORE_SLOT] = &&op_store_slot,
        [OC_OP] = &&op_op, [OC_UNARY] = &&op_unary, [OC_INDEX] = &&op_index,
        [OC_LVALS] = &&op_lvals, [OC_PUSH] = &&op_push, [OC_LVALS_PUSH] = &&op_lvals_push, [OC_SLICE] = &&op_slice,
        [OC_CALL] = &&op_call, [OC_TCO] = &&op_tco,
        [OC_RET] = &&op_ret, [OC_POP] = &&op_pop,
        [OC_DUP] = &&op_dup, [OC_JZ] = &&op_jz, [OC_JNZ] = &&op_jnz, [OC_JMP] = &&op_jmp,
        [OC_PRINT] = &&op_print, [OC_INPUT] = &&op_input,
        [OC_PUSH_ALL] = &&op_push_all,
        [OC_SLICE_INPLACE] = &&op_slice_inplace,
        [OC_DESTRUCTURE] = &&op_destructure,
        [OC_END] = &&op_end,
    };
    int ip = 0;
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    goto *dispatch[c->code[ip].op];

    /* ── Opcode handlers ── */
op_num:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    istk[++isp] = vnum(c->code[ip].num); ip++; goto *dispatch[c->code[ip].op];

op_nil:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    istk[++isp] = nilv(); ip++; goto *dispatch[c->code[ip].op];

op_str: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    istk[++isp] = (Value){ .type = VAL_ARR, .arr = ins->arr };
    aretain(ins->arr); ip++; goto *dispatch[c->code[ip].op];
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
    ip++; goto *dispatch[c->code[ip].op];
}

/* Name-based variable access (top-level / REPL) */
op_var: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value v = sget(cs, ins->name);
    istk[++isp] = v; if (v.type == VAL_ARR) aretain(v.arr);
    ip++; goto *dispatch[c->code[ip].op];
}
/* Slot-indexed variable access (function bodies) — O(1), no strcmp */
op_var_slot: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    int slot = c->code[ip].a;
    Value v = cs->v[slot];
    istk[++isp] = v; if (v.type == VAL_ARR) aretain(v.arr);
    ip++; goto *dispatch[c->code[ip].op];
}
op_store_slot: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    int slot = c->code[ip].a;
    Value v = istk[isp--];
    vassign(&cs->v[slot], v);
    if (v.type == VAL_ARR) arelease(v.arr);
    ip++; goto *dispatch[c->code[ip].op];
}

op_op: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value r = istk[isp--], l = istk[isp--], res = apply(ins->a, l, r);
    if (l.type == VAL_ARR) arelease(l.arr);
    if (r.type == VAL_ARR) arelease(r.arr);
    istk[++isp] = res; ip++; goto *dispatch[c->code[ip].op];
}

op_unary: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value v = istk[isp--];
    if (ins->a == T_BN) { istk[++isp] = truthy(v) ? nilv() : vnum(1); if (v.type==VAL_ARR) arelease(v.arr); }
    else if (ins->a == T_MI) { if (v.type!=VAL_NUM) die("minus on non-number"); istk[++isp] = vnum(-val_num(v)); }
    else if (ins->a == T_HASH) { if (v.type!=VAL_ARR) die("# requires array"); istk[++isp] = vnum((double)(v.arr ? v.arr->len : 0)); if (v.type==VAL_ARR) arelease(v.arr); }
    ip++; goto *dispatch[c->code[ip].op];
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
    ip++; goto *dispatch[c->code[ip].op];
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
    ip++; goto *dispatch[c->code[ip].op];
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
        slot_val->arr->cap = slot_val->arr->cap ? slot_val->arr->cap * 2 : 4;
        slot_val->arr->val = realloc(slot_val->arr->val, slot_val->arr->cap * sizeof(Value));
    }
    vassign(&slot_val->arr->val[len], elem);
    slot_val->arr->len = len + 1;
    if (elem.type == VAL_ARR) arelease(elem.arr);
    ip++; goto *dispatch[c->code[ip].op];
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
        sp->arr->cap = sp->arr->cap ? sp->arr->cap * 2 : 4;
        sp->arr->val = realloc(sp->arr->val, sp->arr->cap * sizeof(Value));
    }
    vassign(&sp->arr->val[len], elem);
    sp->arr->len = len + 1;
    if (elem.type == VAL_ARR) arelease(elem.arr);
    for (int j = 0; j < depth; j++)
        if (indices[j].type == VAL_ARR) arelease(indices[j].arr);
    ip++; goto *dispatch[c->code[ip].op];
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
                slot_val->arr->cap = slot_val->arr->cap ? slot_val->arr->cap * 2 : 4;
                slot_val->arr->val = realloc(slot_val->arr->val, slot_val->arr->cap * sizeof(Value));
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
    ip++; goto *dispatch[c->code[ip].op];
}

op_slice: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Value step_v = istk[isp--], stop_v = istk[isp--], start_v = istk[isp--], arr_v = istk[isp--];
    if (arr_v.type != VAL_ARR) die("slice requires array");
    Arr *src = arr_v.arr; int len = src ? src->len : 0;
    int step = (int)val_num(step_v);
    if (step == 0) die("slice step cannot be 0");
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
    if (count == 0) { arelease(arr_v.arr); istk[++isp] = nilv(); ip++; goto *dispatch[c->code[ip].op]; }
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
        aretain(src);
        arelease(arr_v.arr);
        istk[++isp] = (Value){ .type = VAL_ARR, .arr = view };
    } else {
        /* Strided slice: must copy */
        Arr *result = aalloc(count); result->len = count;
        int idx = 0;
        for (int i = start; step > 0 ? i < stop : i > stop; i += step) {
            result->val[idx] = arr_item(src, i);
            if (result->val[idx].type == VAL_ARR) aretain(result->val[idx].arr);
            idx++;
        }
        arelease(arr_v.arr);
        istk[++isp] = (Value){ .type = VAL_ARR, .arr = result };
    }
    ip++; goto *dispatch[c->code[ip].op];
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
    if (count == 0) {
        arelease(slot_val->arr);
        *slot_val = nilv();
        ip++; goto *dispatch[c->code[ip].op];
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
            ip++; goto *dispatch[c->code[ip].op];
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
        aretain(src);
        arelease(slot_val->arr);
        slot_val->arr = view;
    } else {
        Arr *result = aalloc(count); result->len = count;
        int idx = 0;
        for (int i = start; step > 0 ? i < stop : i > stop; i += step) {
            result->val[idx] = arr_item(src, i);
            if (result->val[idx].type == VAL_ARR) aretain(result->val[idx].arr);
            idx++;
        }
        arelease(slot_val->arr);
        slot_val->arr = result;
    }
    if (start_v.type == VAL_ARR) arelease(start_v.arr);
    if (stop_v.type == VAL_ARR) arelease(stop_v.arr);
    ip++; goto *dispatch[c->code[ip].op];
}

op_call: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    int fi = ins->a, ac = ins->b; Fn *f = &fs[fi];
    Value args[64];
    for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
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
            if (args[j].type == VAL_ARR) aretain(args[j].arr);
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
    ip++; goto *dispatch[c->code[ip].op];
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
            if (args[j].type == VAL_ARR) aretain(args[j].arr);
        } else if (f->def_vals) {
            Value d = f->def_vals[j];
            cs->v[f->p_slots[j]] = d;
            if (d.type == VAL_ARR) aretain(d.arr);
        } else {
            cs->v[f->p_slots[j]] = nilv();
        }
    }
    ip = 0; goto *dispatch[c->code[ip].op];
}

op_ret:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    rv = istk[isp--]; rf = 1; return;

op_pop:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    if (isp >= 0) { Value v = istk[isp--]; if (v.type == VAL_ARR) arelease(v.arr); }
    ip++; goto *dispatch[c->code[ip].op];

op_dup: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    if (isp < 0) die("stack underflow");
    Value v = istk[isp]; if (v.type == VAL_ARR) aretain(v.arr);
    istk[++isp] = v; ip++; goto *dispatch[c->code[ip].op];
}

op_jz: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value v = istk[isp--]; int t = truthy(v);
    if (v.type == VAL_ARR) arelease(v.arr);
    ip = t ? ip + 1 : ins->a;
    goto *dispatch[c->code[ip].op];
}

op_jnz: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    Instr *ins = &c->code[ip];
    Value v = istk[isp--]; int t = truthy(v);
    if (v.type == VAL_ARR) arelease(v.arr);
    ip = t ? ins->a : ip + 1;
    goto *dispatch[c->code[ip].op];
}

op_jmp:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    ip = c->code[ip].a;
    goto *dispatch[c->code[ip].op];

op_print:
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    if (isp >= 0) { print_val(istk[isp--]); }
    ip++; goto *dispatch[c->code[ip].op];

op_input: {
    err_line = c->code[ip].line; err_file = c->code[ip].file;
    char buf[1024]; int n;
    if (!fgets(buf, 1024, stdin)) { n = 0; } else { n = strlen(buf); if (n && buf[n-1]=='\n') n--; }
    Arr *a = aalloc(n); a->len = n;
    for (int i = 0; i < n; i++) a->val[i] = vnum((double)(unsigned char)buf[i]);
    istk[++isp] = (Value){ .type = VAL_ARR, .arr = a };
    ip++; goto *dispatch[c->code[ip].op];
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
    ip++; goto *dispatch[c->code[ip].op];
}

op_end:
    return;
}

/* ─── Main ─── */

int main(int a, char **v) {
    cs = snew(); cur_fi = -1;
    if (a >= 2) {
        char *src = readf(v[1]); if (!src) { fprintf(stderr, "cannot read '%s'\n", v[1]); return 1; }
        char dir[1024] = {0}; const char *slash = strrchr(v[1], '/');
        if (slash) { memcpy(dir, v[1], slash - v[1]); } include_dir = dir;
        comp_file = strdup(v[1]);
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
            if (comp_types[i] == T_ARR_TYPE && cs->v[i].type == VAL_NUM)
                cs->v[i] = (Value){ .type = VAL_ARR };
        }
        exec(code); code_free(code); free(ts);
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
                if (comp_types[i] == T_ARR_TYPE && cs->v[i].type == VAL_NUM)
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
