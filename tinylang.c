#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>

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
typedef struct Arr {
    int refcount, len, cap; ArrKind kind;
    union { struct Value *val; uint8_t *u8; uint16_t *u16; uint32_t *u32; uint64_t *u64;
            int8_t *i8; int16_t *i16; int32_t *i32; int64_t *i64;
            float *f32; double *f64; } as;
} Arr;
typedef struct Value {
    Type type; NumKind nkind;
    union {
        uint8_t u8; uint16_t u16; uint32_t u32; uint64_t u64;
        int8_t i8; int16_t i16; int32_t i32; int64_t i64;
        float f32; double f64;
        Arr *arr; void *ptr;
    } as;
} Value;

typedef enum {
    T_EOF, T_NUM, T_ID, T_STR, T_NIL,
    T_LP, T_RP, T_LB, T_RB, T_LC, T_RC, T_CM, T_SEMI,
    T_PL, T_MI, T_ST, T_SL, T_PC,
    T_AM, T_PI, T_CA, T_AT, T_BN,
    T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_HASH,
    T_NL, T_IF, T_ELIF, T_ELSE, T_WH, T_FN, T_RT, T_INCLUDE,
} TK;

typedef struct { TK t; double n; char *s; int l; } Tok;
typedef struct { char **n; Value *v; int c, m; } Scp;

typedef enum {
    OC_NUM, OC_NIL, OC_STR, OC_MAKE_ARR,
    OC_VAR, OC_STORE,
    OC_OP, OC_UNARY,
    OC_INDEX,
    OC_CALL, OC_TCO,
    OC_JZ, OC_JMP, OC_RET, OC_POP,
    OC_LVALS,
    OC_PRINT, OC_INPUT, OC_ASSERT, OC_CFUNC, OC_PUSH, OC_TYPE,
    OC_END,
} OC;

typedef struct Code { struct Instr *code; int len, cap; } Code;
typedef struct Instr {
    OC op; int a, b; double num; Arr *arr; char *name; Code *sub; int line; char *file;
} Instr;

Tok *ts; int tc, tp;
Scp *cs;
int rf; Value rv;
int cur_fi;
static char *include_dir;
jmp_buf assert_jmp; int assert_catching;
char assert_msg[512];
int comp_line; char *comp_file;
int err_line; char *err_file;
typedef struct { int fi; int line; char *file; } CallFrame;
CallFrame call_stack[128]; int call_depth = -1;

typedef struct { char *n, **p; int a; Code *code; } Fn;
Fn *fs; int fc, fm;
Value istk[4096]; int isp;

typedef Value (*CFunc)(int, Value*);
typedef struct { char *name; CFunc func; } CReg;
static CReg *cregs; static int creg_count, creg_cap;

void tl_register(const char *name, CFunc func) {
    if (creg_count >= creg_cap) {
        creg_cap = creg_cap ? creg_cap * 2 : 8;
        cregs = realloc(cregs, creg_cap * sizeof(CReg));
    }
    cregs[creg_count].name = strdup(name);
    cregs[creg_count].func = func;
    creg_count++;
}

/* Detect the narrowest NumKind for a double value */
NumKind detect_num_kind(double d) {
    if (d != floor(d) || !isfinite(d)) {
        return ((double)(float)d == d) ? NK_F32 : NK_F64;
    }
    if (d >= 0) {
        if (d <= 255.0) return NK_U8;
        if (d <= 65535.0) return NK_U16;
        if (d <= 4294967295.0) return NK_U32;
        return NK_U64;
    } else {
        if (d >= -128.0 && d <= 127.0) return NK_I8;
        if (d >= -32768.0 && d <= 32767.0) return NK_I16;
        if (d >= -2147483648.0 && d <= 2147483647.0) return NK_I32;
        return NK_I64;
    }
}

/* Create a number Value with compact storage */
Value vnum(double n) {
    Value v = { .type = VAL_NUM, .nkind = NK_F64, .as.f64 = n };
    v.nkind = detect_num_kind(n);
    switch (v.nkind) {
        case NK_U8:  v.as.u8  = (uint8_t)n; break;
        case NK_U16: v.as.u16 = (uint16_t)n; break;
        case NK_U32: v.as.u32 = (uint32_t)n; break;
        case NK_U64: v.as.u64 = (uint64_t)n; break;
        case NK_I8:  v.as.i8  = (int8_t)n; break;
        case NK_I16: v.as.i16 = (int16_t)n; break;
        case NK_I32: v.as.i32 = (int32_t)n; break;
        case NK_I64: v.as.i64 = (int64_t)n; break;
        case NK_F32: v.as.f32 = (float)n; break;
        case NK_F64: v.as.f64 = n; break;
    }
    return v;
}

/* Widen a number Value to double */
double val_num(Value v) {
    switch (v.nkind) {
        case NK_U8:  return v.as.u8;
        case NK_U16: return v.as.u16;
        case NK_U32: return v.as.u32;
        case NK_U64: return (double)v.as.u64;
        case NK_I8:  return v.as.i8;
        case NK_I16: return v.as.i16;
        case NK_I32: return v.as.i32;
        case NK_I64: return (double)v.as.i64;
        case NK_F32: return v.as.f32;
        case NK_F64: return v.as.f64;
    }
    return v.as.f64;
}

Value vempty(void) { return (Value){ .type = VAL_ARR }; }
Value vptr(void *p) { return (Value){ .type = VAL_PTR, .as.ptr = p }; }

/* Read element from any array kind, wrapping as Value */
Value arr_item(Arr *a, int i) {
    switch (a->kind) {
        case ARR_U8:  return vnum((double)a->as.u8[i]);
        case ARR_U16: return vnum((double)a->as.u16[i]);
        case ARR_U32: return vnum((double)a->as.u32[i]);
        case ARR_U64: return vnum((double)a->as.u64[i]);
        case ARR_I8:  return vnum((double)a->as.i8[i]);
        case ARR_I16: return vnum((double)a->as.i16[i]);
        case ARR_I32: return vnum((double)a->as.i32[i]);
        case ARR_I64: return vnum((double)a->as.i64[i]);
        case ARR_F32: return vnum((double)a->as.f32[i]);
        case ARR_F64: return vnum(a->as.f64[i]);
        case ARR_VAL: return a->as.val[i];
    }
    return vnum(0);
}

/* Detect the narrowest compact kind that can hold all values */
ArrKind detect_kind(Value *vals, int n) {
    if (n == 0) return ARR_VAL;
    double dmin = 0, dmax = 0;
    int all_int = 1, all_num = 1, first = 1;
    for (int i = 0; i < n; i++) {
        if (vals[i].type != VAL_NUM) { all_num = 0; continue; }
        double d = val_num(vals[i]);
        if (first) { dmin = dmax = d; first = 0; }
        else { if (d < dmin) dmin = d; if (d > dmax) dmax = d; }
        if (d != floor(d) || !isfinite(d)) all_int = 0;
    }
    if (!all_num) return ARR_VAL;
    if (all_int) {
        if (dmin >= 0) {
            if (dmax <= 255.0) return ARR_U8;
            if (dmax <= 65535.0) return ARR_U16;
            if (dmax <= 4294967295.0) return ARR_U32;
            return ARR_U64;
        } else {
            if (dmin >= -128.0 && dmax <= 127.0) return ARR_I8;
            if (dmin >= -32768.0 && dmax <= 32767.0) return ARR_I16;
            if (dmin >= -2147483648.0 && dmax <= 2147483647.0) return ARR_I32;
            return ARR_I64;
        }
    }
    for (int i = 0; i < n; i++)
        if ((double)(float)val_num(vals[i]) != val_num(vals[i])) return ARR_F64;
    return ARR_F32;
}

ArrKind promote_kind(ArrKind a, ArrKind b);
void arr_append_val(Arr *a, int *k, Value v);
void vassign(Value *d, Value s);

/* ── Arr helpers ──────────────────────────────────────────────── */

int kind_exactly_covers(ArrKind container, ArrKind contained) {
    if (container == contained) return 1;
    switch (container) {
        case ARR_U16: return contained == ARR_U8;
        case ARR_U32: return contained == ARR_U8 || contained == ARR_U16;
        case ARR_U64: return contained >= ARR_U8 && contained <= ARR_U32;
        case ARR_I16: return contained == ARR_I8 || contained == ARR_U8;
        case ARR_I32: return (contained >= ARR_I8 && contained <= ARR_I16) ||
                             (contained >= ARR_U8 && contained <= ARR_U16);
        case ARR_I64: return (contained >= ARR_I8 && contained <= ARR_I32) ||
                             (contained >= ARR_U8 && contained <= ARR_U32);
        case ARR_F32: return contained == ARR_U8 || contained == ARR_I8 ||
                             contained == ARR_U16 || contained == ARR_I16 ||
                             contained == ARR_F32;
        case ARR_F64: return (contained >= ARR_U8 && contained <= ARR_U32) ||
                             (contained >= ARR_I8 && contained <= ARR_I32) ||
                             contained == ARR_F32 || contained == ARR_F64;
        case ARR_VAL: return 1;
        default: return 0;
    }
}

ArrKind promote_kind(ArrKind a, ArrKind b) {
    if (a == b) return a;
    ArrKind candidates[] = {
        ARR_U8, ARR_I8, ARR_U16, ARR_I16,
        ARR_U32, ARR_I32, ARR_F32,
        ARR_U64, ARR_I64, ARR_F64, ARR_VAL
    };
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        ArrKind k = candidates[i];
        if (kind_exactly_covers(k, a) && kind_exactly_covers(k, b)) return k;
    }
    return ARR_VAL;
}

void arr_append_val(Arr *a, int *k, Value v) {
    double d = val_num(v);
    switch (a->kind) {
        case ARR_U8:  a->as.u8[(*k)++]  = (uint8_t)d; break;
        case ARR_U16: a->as.u16[(*k)++] = (uint16_t)d; break;
        case ARR_U32: a->as.u32[(*k)++] = (uint32_t)d; break;
        case ARR_U64: a->as.u64[(*k)++] = (uint64_t)d; break;
        case ARR_I8:  a->as.i8[(*k)++]  = (int8_t)d; break;
        case ARR_I16: a->as.i16[(*k)++] = (int16_t)d; break;
        case ARR_I32: a->as.i32[(*k)++] = (int32_t)d; break;
        case ARR_I64: a->as.i64[(*k)++] = (int64_t)d; break;
        case ARR_F32: a->as.f32[(*k)++] = (float)d; break;
        case ARR_F64: a->as.f64[(*k)++] = d; break;
        case ARR_VAL: vassign(&a->as.val[*k], v); (*k)++; break;
    }
}

Arr *aalloc(int c, ArrKind k) {
    Arr *a = calloc(1, sizeof(Arr)); a->refcount = 1;
    a->cap = c; a->kind = k;
    switch (k) {
        case ARR_U8:  a->as.u8  = c ? calloc(c, 1) : NULL; break;
        case ARR_U16: a->as.u16 = c ? calloc(c, sizeof(uint16_t)) : NULL; break;
        case ARR_U32: a->as.u32 = c ? calloc(c, sizeof(uint32_t)) : NULL; break;
        case ARR_U64: a->as.u64 = c ? calloc(c, sizeof(uint64_t)) : NULL; break;
        case ARR_I8:  a->as.i8  = c ? calloc(c, 1) : NULL; break;
        case ARR_I16: a->as.i16 = c ? calloc(c, sizeof(int16_t)) : NULL; break;
        case ARR_I32: a->as.i32 = c ? calloc(c, sizeof(int32_t)) : NULL; break;
        case ARR_I64: a->as.i64 = c ? calloc(c, sizeof(int64_t)) : NULL; break;
        case ARR_F32: a->as.f32 = c ? calloc(c, sizeof(float)) : NULL; break;
        case ARR_F64: a->as.f64 = c ? calloc(c, sizeof(double)) : NULL; break;
        case ARR_VAL: a->as.val = c ? calloc(c, sizeof(Value)) : NULL; break;
    }
    return a;
}

void arelease(Arr *a) {
    if (!a) return;
    if (--a->refcount > 0) return;
    if (a->kind == ARR_VAL) {
        for (int i = 0; i < a->len; i++)
            if (a->as.val[i].type == VAL_ARR) arelease(a->as.val[i].as.arr);
        free(a->as.val);
    } else {
        free(a->as.i8);
    }
    free(a);
}

void aretain(Arr *a) { if (a) a->refcount++; }

Arr *adeep_copy(Arr *s) {
    if (!s) return NULL;
    Arr *d = aalloc(s->cap, s->kind); d->len = s->len;
    if (s->kind == ARR_VAL) {
        for (int i = 0; i < s->len; i++) {
            d->as.val[i] = s->as.val[i];
            if (d->as.val[i].type == VAL_ARR) aretain(d->as.val[i].as.arr);
        }
        return d;
    }
    int esize = 0;
    switch (s->kind) {
        case ARR_U8:  case ARR_I8:  esize = 1; break;
        case ARR_U16: case ARR_I16: esize = 2; break;
        case ARR_U32: case ARR_I32: case ARR_F32: esize = 4; break;
        case ARR_U64: case ARR_I64: case ARR_F64: esize = 8; break;
        default: break;
    }
    if (esize) memcpy(d->as.i8, s->as.i8, s->len * esize);
    return d;
}

void apromote_to_val(Arr *a) {
    if (a->kind == ARR_VAL) return;
    int n = a->len, c = a->cap;
    Value *nv = calloc(c, sizeof(Value));
    for (int i = 0; i < n; i++) nv[i] = arr_item(a, i);
    free(a->as.i8);
    a->as.val = nv;
    a->kind = ARR_VAL;
}

void amake_uniq(Value *v) {
    if (v->type != VAL_ARR || !v->as.arr) return;
    if (v->as.arr->refcount > 1) {
        Arr *old = v->as.arr;
        v->as.arr = adeep_copy(old);
        apromote_to_val(v->as.arr);
        arelease(old);
    } else {
        apromote_to_val(v->as.arr);
    }
}

void vassign(Value *d, Value s) {
    if (d->type == VAL_ARR) arelease(d->as.arr);
    *d = s;
    if (s.type == VAL_ARR) aretain(s.as.arr);
}

void print_val(Value v) {
    if (v.type == VAL_NUM) {
        double d = val_num(v);
        if (d == (double)(int64_t)d) printf("%lld", (int64_t)d);
        else printf("%g", d);
    } else if (v.type == VAL_ARR) {
        Arr *a = v.as.arr;
        int is_str = a ? 1 : 0;
        if (a && (a->kind == ARR_U8 || a->kind == ARR_I8)) {
            int8_t *p = (a->kind == ARR_U8) ? (int8_t*)a->as.u8 : a->as.i8;
            for (int i = 0; i < a->len && is_str; i++) {
                int c = (unsigned char)p[i];
                if (c != 10 && c != 13 && c != 9 && (c < 32 || c > 126)) is_str = 0;
            }
            if (is_str) { fwrite(p, 1, a->len, stdout); return; }
        } else if (a && a->kind == ARR_VAL) {
            for (int i = 0; i < a->len && is_str; i++) {
                if (a->as.val[i].type != VAL_NUM) { is_str = 0; break; }
                int c = (int)val_num(a->as.val[i]);
                if (c != 10 && c != 13 && c != 9 && (c < 32 || c > 126)) is_str = 0;
            }
            if (is_str) { for (int i = 0; i < a->len; i++) putchar((int)val_num(a->as.val[i])); return; }
        } else is_str = 0;
        putchar('[');
        if (a) for (int i = 0; i < a->len; i++) {
            if (i) printf(", "); print_val(arr_item(a, i));
        }
        putchar(']');
    } else printf("<ptr: %p>", v.as.ptr);
}

int truthy(Value v) {
    if (v.type == VAL_PTR) return v.as.ptr != NULL;
    return v.type != VAL_ARR || (v.as.arr && v.as.arr->len > 0);
}

int veq(Value a, Value b) {
    if (a.type != b.type) return 0;
    if (a.type == VAL_NUM) return val_num(a) == val_num(b);
    if (a.type == VAL_PTR) return a.as.ptr == b.as.ptr;
    if (!a.as.arr && !b.as.arr) return 1;
    if (!a.as.arr || !b.as.arr || a.as.arr->len != b.as.arr->len) return 0;

    Arr *aa = a.as.arr, *bb = b.as.arr;
    for (int i = 0; i < aa->len; i++)
        if (!veq(arr_item(aa, i), arr_item(bb, i))) return 0;
    return 1;
}

void die(const char *f, ...) {
    va_list ap, aq; va_start(ap, f); va_copy(aq, ap);
    if (assert_catching) { vsnprintf(assert_msg, sizeof(assert_msg), f, aq); longjmp(assert_jmp, 1); }
    if (err_file) fprintf(stderr, "%s:%d: ", err_file, err_line);
    vfprintf(stderr, f, ap); fputc('\n', stderr);
    fprintf(stderr, "stack trace:\n");
    for (int i = 0; i <= call_depth; i++) {
        if (call_stack[i].file) fprintf(stderr, "  %s:%d: ", call_stack[i].file, call_stack[i].line);
        if (call_stack[i].fi >= 0) fprintf(stderr, "%s()\n", fs[call_stack[i].fi].n);
        else fprintf(stderr, "<top-level>\n");
    }
    if (err_file) fprintf(stderr, "  %s:%d: ", err_file, err_line);
    if (cur_fi >= 0) fprintf(stderr, "%s()\n", fs[cur_fi].n);
    else fprintf(stderr, "<top-level>\n");
    va_end(aq); va_end(ap); exit(1);
}

#define nilv() ((Value){ .type = VAL_ARR })

Value apply(int op, Value l, Value r) {
    double ld = val_num(l), rd = val_num(r);
    switch (op) {
        case T_PL:
            if (l.type == VAL_NUM && r.type == VAL_NUM) return vnum(ld + rd);
            if (l.type == VAL_ARR && r.type == VAL_ARR) {
                Arr *la = l.as.arr, *ra = r.as.arr;
                int ln = la ? la->len : 0, rn = ra ? ra->len : 0;
                ArrKind lk = la ? la->kind : ARR_VAL, rk = ra ? ra->kind : ARR_VAL;
                ArrKind rkf = promote_kind(lk, rk);
                Arr *a = aalloc(ln + rn, rkf); a->len = ln + rn; int k = 0;
                for (int i = 0; i < ln; i++) arr_append_val(a, &k, arr_item(la, i));
                for (int i = 0; i < rn; i++) arr_append_val(a, &k, arr_item(ra, i));
                return (Value){ .type = VAL_ARR, .as.arr = a };
            } die("'+' type mismatch");

        case T_MI:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'-' requires numbers");
            return vnum(ld - rd);

        case T_ST:
            if (l.type == VAL_NUM && r.type == VAL_NUM) return vnum(ld * rd);
            if (l.type == VAL_ARR && r.type == VAL_NUM) {
                Arr *la = l.as.arr; int bl = la ? la->len : 0, n = (int)rd;
                if (n <= 0) return nilv();
                ArrKind lk = la ? la->kind : ARR_VAL;
                Arr *a = aalloc(bl * n, lk); a->len = bl * n;
                if (lk == ARR_VAL) {
                    for (int i = 0; i < bl; i++) vassign(&a->as.val[i], arr_item(la, i));
                    for (int i = 1; i < n; i++)
                        for (int j = 0; j < bl; j++) vassign(&a->as.val[i * bl + j], arr_item(la, j));
                } else {
                    int esize = 0;
                    switch (lk) {
                        case ARR_U8:  case ARR_I8:  esize = 1; break;
                        case ARR_U16: case ARR_I16: esize = 2; break;
                        case ARR_U32: case ARR_I32: case ARR_F32: esize = 4; break;
                        case ARR_U64: case ARR_I64: case ARR_F64: esize = 8; break;
                        default: break;
                    }
                    memcpy(a->as.i8, la->as.i8, bl * esize);
                    for (int i = 1; i < n; i++)
                        memcpy(a->as.i8 + i * bl * esize, la->as.i8, bl * esize);
                }
                return (Value){ .type = VAL_ARR, .as.arr = a };
            } die("'*' type mismatch");

        case T_SL:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'/' requires numbers");
            if (rd == 0) die("division by zero");
            return vnum(ld / rd);

        case T_PC:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'%%' requires numbers");
            if (rd == 0) die("modulo by zero");
            return vnum(fmod(ld, rd));

        case T_AM: case T_PI: case T_CA: case T_AT: {
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("bitwise requires numbers");
            int64_t a = (int64_t)ld, b = (int64_t)rd;
            if (op == T_AM) return vnum((double)(a & b));
            if (op == T_PI) return vnum((double)(a | b));
            if (op == T_CA) return vnum((double)(a ^ b));
            int64_t s = (int64_t)rd;
            if (s < 0) { if (s < -63) s = -63; return vnum((double)((uint64_t)a >> -s)); }
            if (s > 63) s = 63;
            return vnum((double)((uint64_t)a << s));
        }
        case T_EQ: return veq(l, r) ? vnum(1) : nilv();
        case T_NE: return veq(l, r) ? nilv() : vnum(1);
        case T_LT:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'<' requires numbers");
            return ld < rd ? vnum(1) : nilv();
        case T_GT:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'>' requires numbers");
            return ld > rd ? vnum(1) : nilv();
        case T_LE:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'<=' requires numbers");
            return ld <= rd ? vnum(1) : nilv();
        case T_GE:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'>=' requires numbers");
            return ld >= rd ? vnum(1) : nilv();
        default: die("unknown op");
    } return nilv();
}

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
            if (c == '.') { b[i++] = c; ac(); c = pc(); }
            while (isdigit(c)) { b[i++] = c; ac(); c = pc(); }
            if (c == '.') { b[i++] = c; ac(); c = pc(); while (isdigit(c)) { b[i++] = c; ac(); c = pc(); } }
            b[i] = 0; tk.t = T_NUM; tk.n = atof(b); goto em;
        }

        if (isalpha(c) || c == '_') {
            char b[64]; int i = 0;
            while (isalnum(c) || c == '_') { b[i++] = c; ac(); c = pc(); }
            b[i] = 0; tk.t = T_ID; tk.s = strdup(b);
            if      (!strcmp(b,"nil")){tk.t=T_NIL;free(tk.s);}
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
            Arr *a = aalloc(i, ARR_I8); a->len = i;
            for (int j = 0; j < i; j++) a->as.i8[j] = (unsigned char)b[j];
            tk.t = T_STR; tk.s = (char*)a; goto em;
        }

        ac();
        switch (c) {
            case '(': tk.t=T_LP; break; case ')': tk.t=T_RP; break;
            case '[': tk.t=T_LB; break; case ']': tk.t=T_RB; break;
            case '{': tk.t=T_LC; break; case '}': tk.t=T_RC; break;
            case ',': tk.t=T_CM; break; case ';': tk.t=T_SEMI; break;
            case '+': tk.t=T_PL; break; case '-': tk.t=T_MI; break;
            case '*': tk.t=T_ST; break; case '/': tk.t=T_SL; break;
            case '%': tk.t=T_PC; break;
            case '&': tk.t=T_AM; break; case '|': tk.t=T_PI; break;
            case '^': tk.t=T_CA; break; case '@': tk.t=T_AT; break;
            case '#': tk.t=T_HASH; break;
            case '!': if (pc()=='='){ac();tk.t=T_NE;}else tk.t=T_BN; break;
            case '=': tk.t=T_EQ; break;
            case '<': if (pc()=='='){ac();tk.t=T_LE;}else tk.t=T_LT; break;
            case '>': if (pc()=='='){ac();tk.t=T_GE;}else tk.t=T_GT; break;
            default: { char m[2]={c,0}; die("unexpected '%s'",m); }
        }
        em: if (tc>=m){m*=2;ts=realloc(ts,m*sizeof(Tok));} ts[tc++]=tk; if (tk.t==T_EOF) break;
    }
}

Scp *snew(void) { return calloc(1, sizeof(Scp)); }

void sfree(Scp *s) {
    for (int i = 0; i < s->c; i++) { if (s->v[i].type == VAL_ARR) arelease(s->v[i].as.arr); free(s->n[i]); }
    free(s->n); free(s->v); free(s);
}

Value sget(Scp *s, const char *n) {
    for (int i = 0; i < s->c; i++) if (!strcmp(s->n[i], n)) return s->v[i];
    die("undefined '%s'", n); return nilv();
}

void sset(Scp *s, const char *n, Value v) {
    for (int i = 0; i < s->c; i++)
        if (!strcmp(s->n[i], n)) { vassign(&s->v[i], v); return; }
    if (s->c >= s->m) { s->m = s->m ? s->m*2 : 4;
        s->n = realloc(s->n, s->m*sizeof(char*)); s->v = realloc(s->v, s->m*sizeof(Value)); }
    s->n[s->c] = strdup(n); s->v[s->c] = v;
    if (v.type == VAL_ARR) aretain(v.as.arr); s->c++;
}

int ffind(const char *n) { for (int i = 0; i < fc; i++) if (!strcmp(fs[i].n, n)) return i; return -1; }

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

void comp_stmt(Code *c);
void comp_expr(Code *c);
void comp_prim(Code *c);
void comp_block(Code *c);

void comp_prim(Code *c) {
    while (ts[tp].t == T_NL) tp++;
    Tok t = ts[tp]; comp_line = t.l; err_line = t.l; err_file = comp_file;
    switch (t.t) {
        case T_NUM: tp++; emit(c, (Instr){OC_NUM, 0, 0, .num = t.n}); break;
        case T_NIL: tp++; emit(c, (Instr){OC_NIL, 0, 0}); break;
        case T_STR: { tp++; Arr *o = (Arr*)t.s; emit(c, (Instr){OC_STR, 0, 0, .arr = o}); break; }
        case T_ID: {
            char *nm = strdup(t.s); tp++;
            if (ts[tp].t == T_LP) {
                free(nm); tp++;
                int is_assert = !strcmp(t.s, "assert");
                if (is_assert) {
                    Code *arg = new_code(); int ac = 0;
                    if (ts[tp].t != T_RP) { do { comp_expr(arg); ac++; } while (ts[tp].t == T_CM && (tp++, 1)); }
                    tp++; emit(c, (Instr){OC_ASSERT, ac, 0, .sub = arg}); break;
                }
                int ac = 0;
                if (ts[tp].t != T_RP) { do { comp_expr(c); ac++; } while (ts[tp].t == T_CM && (tp++, 1)); }
                tp++;
                if (!strcmp(t.s, "print")) { if (ac < 1) die("print needs 1 arg"); emit(c, (Instr){OC_PRINT, 0, 0}); }
                else if (!strcmp(t.s, "input")) emit(c, (Instr){OC_INPUT, 0, 0});
                else if (!strcmp(t.s, "type")) emit(c, (Instr){OC_TYPE, 0, 0});
                else if (!strcmp(t.s, "thispath")) {
                    int n = comp_file ? strlen(comp_file) : 0;
                    Arr *a = aalloc(n, ARR_I8); a->len = n;
                    for (int j = 0; j < n; j++) a->as.i8[j] = comp_file[j];
                    emit(c, (Instr){OC_STR, 0, 0, .arr = a});
                } else {
                    int ci = -1;
                    for (int i = 0; i < creg_count; i++) if (!strcmp(t.s, cregs[i].name)) { ci = i; break; }
                    if (ci >= 0) { emit(c, (Instr){OC_CFUNC, ci, ac}); }
                    else { int fi = ffind(t.s); if (fi < 0) die("undefined function '%s'", t.s); emit(c, (Instr){OC_CALL, fi, ac}); }
                }
            } else {
                emit(c, (Instr){OC_VAR, 0, 0, .name = nm});
                while (ts[tp].t == T_LB) {
                    tp++;
                    do { comp_expr(c); emit(c, (Instr){OC_INDEX, 0, 0}); } while (ts[tp].t == T_CM && (tp++, 1));
                    if (ts[tp].t != T_RB) die("expected ]"); tp++;
                }
            }
            break;
        }
        case T_LB: {
            tp++;
            if (ts[tp].t == T_RB) { tp++; emit(c, (Instr){OC_NIL, 0, 0}); break; }
            int n = 0;
            do { comp_expr(c); n++; } while (ts[tp].t == T_CM && (tp++, 1));
            if (ts[tp].t != T_RB) die("expected ]"); tp++;
            emit(c, (Instr){OC_MAKE_ARR, n, 0}); break;
        }
        case T_LP: { tp++; comp_expr(c); if (ts[tp].t != T_RP) die("expected )"); tp++; break; }
        case T_BN: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_BN, 0}); break;
        case T_MI: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_MI, 0}); break;
        case T_HASH: tp++; comp_prim(c); emit(c, (Instr){OC_UNARY, T_HASH, 0}); break;
        default: die("unexpected token at line %d", t.l);
    }
}

void comp_expr(Code *c) {
    comp_prim(c);
    if (ts[tp].t >= T_PL && ts[tp].t <= T_GE) {
        int op = ts[tp].t; tp++;
        comp_prim(c);
        if (ts[tp].t >= T_PL && ts[tp].t <= T_GE) die("chaining ops needs ()");
        emit(c, (Instr){OC_OP, op, 0});
    }
}

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
    int jz_patches[64], np = 0, jmp_patches[64], nj = 0;
    tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file; comp_expr(c);
    jz_patches[np++] = c->len; emit(c, (Instr){OC_JZ, 0, 0});
    comp_block(c);
    jmp_patches[nj++] = c->len; emit(c, (Instr){OC_JMP, 0, 0});
    while (ts[tp].t == T_ELIF) {
        c->code[jz_patches[np-1]].a = c->len; tp++;
        comp_expr(c); jz_patches[np++] = c->len; emit(c, (Instr){OC_JZ, 0, 0});
        comp_block(c); jmp_patches[nj++] = c->len; emit(c, (Instr){OC_JMP, 0, 0});
    }
    if (ts[tp].t == T_ELSE) { c->code[jz_patches[np-1]].a = c->len; tp++; comp_block(c); }
    else c->code[jz_patches[np-1]].a = c->len;
    for (int i = 0; i < nj; i++) c->code[jmp_patches[i]].a = c->len;
}

void comp_while(Code *c) {
    int loop = c->len; tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    comp_expr(c); int jz = c->len; emit(c, (Instr){OC_JZ, 0, 0});
    comp_block(c);
    emit(c, (Instr){OC_JMP, loop, 0}); c->code[jz].a = c->len;
}

void comp_return(Code *c) {
    tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    int is_tc = (cur_fi >= 0 && ts[tp].t == T_ID &&
                 !strcmp(ts[tp].s, fs[cur_fi].n) && ts[tp+1].t == T_LP);
    comp_expr(c);
    if (is_tc && c->code[c->len-1].op == OC_CALL) {
        Instr *last = &c->code[c->len-1];
        last->op = OC_TCO; last->a = last->b; last->b = 0;
    } else if (!is_tc) emit(c, (Instr){OC_RET, 0, 0});
}

void comp_fn(Code *c) {
    tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    if (ts[tp].t != T_ID) die("expected function name");
    char *name = strdup(ts[tp].s); tp++;
    if (ts[tp].t != T_LP) die("expected ("); tp++;
    char *params[64]; int pa = 0;
    if (ts[tp].t != T_RP) {
        do { if (ts[tp].t != T_ID) die("expected param name"); params[pa++] = strdup(ts[tp].s); tp++; }
        while (ts[tp].t == T_CM && (tp++, 1));
    }
    if (ts[tp].t != T_RP) die("expected )"); tp++;
    if (ffind(name) >= 0) die("'%s' already defined", name);
    if (fc >= fm) { fm = fm ? fm*2 : 8; fs = realloc(fs, fm*sizeof(Fn)); }
    Fn *f = &fs[fc++]; f->n = name; f->p = malloc(pa*sizeof(char*));
    for (int i = 0; i < pa; i++) f->p[i] = params[i]; f->a = pa; f->code = NULL;
    Code *body = new_code(); comp_block(body); emit(body, (Instr){OC_END, 0, 0}); f->code = body;
    int last = body->len - 1;
    if (last >= 0 && body->code[last].op == OC_END) last--;
    if (last >= 0 && body->code[last].op == OC_RET) last--;
    if (last >= 0 && body->code[last].op == OC_CALL &&
        body->code[last].a < fc && !strcmp(fs[body->code[last].a].n, name))
        { body->code[last].op = OC_TCO; body->code[last].a = body->code[last].b; body->code[last].b = 0; }
}

char *tl_to_cstring(Value v) {
    if (v.type != VAL_ARR || !v.as.arr) return strdup("");
    Arr *a = v.as.arr;
    char *s = malloc(a->len + 1);
    if (a->kind == ARR_I8 || a->kind == ARR_U8) {
        memcpy(s, a->as.i8, a->len);
    } else {
        for (int i = 0; i < a->len; i++)
            s[i] = (char)val_num(arr_item(a, i));
    }
    s[a->len] = 0;
    return s;
}

#ifdef TL_FFI
#include <dlfcn.h>
#include <ffi.h>

Value tl_dlopen(int argc, Value *args) {
    if (argc < 1) return vptr(NULL);
    char *path = tl_to_cstring(args[0]);
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    free(path);
    return vptr(handle);
}

Value tl_dlsym(int argc, Value *args) {
    if (argc < 2 || args[0].type != VAL_PTR || !args[0].as.ptr) return vptr(NULL);
    char *name = tl_to_cstring(args[1]);
    void *sym = dlsym(args[0].as.ptr, name);
    free(name);
    return vptr(sym);
}

Value tl_dlclose(int argc, Value *args) {
    if (argc < 1) die("dlclose needs handle");
    dlclose(args[0].as.ptr);
    return vempty();
}

Value tl_ffi_call(int argc, Value *args) {
    if (argc < 2) die("ffi_call needs fn_ptr, sig, ...");
    void  *fn   = args[0].as.ptr;
    char  *sig  = tl_to_cstring(args[1]);
    int    slen = strlen(sig);
    char   rets = sig[0];
    int    nargs = slen - 1;
    if (argc - 2 != nargs)
        die("ffi_call: sig says %d args, got %d", nargs, argc - 2);

    ffi_type *at[64]; void *av[64];
    int ib[64], ni=0;
    double db[64]; int nd=0;
    void *pb[64]; int np2=0;
    char *sb[64]; int ns2=0;

    for (int i = 1; i < slen; i++) {
        switch (sig[i]) {
            case 'i': at[i-1]=&ffi_type_sint32;  ib[ni]=(int)val_num(args[1+i]); av[i-1]=&ib[ni++]; break;
            case 'd': at[i-1]=&ffi_type_double;  db[nd]=val_num(args[1+i]);     av[i-1]=&db[nd++]; break;
            case 'p': at[i-1]=&ffi_type_pointer;  pb[np2]=args[1+i].as.ptr;     av[i-1]=&pb[np2++]; break;
            case 's': at[i-1]=&ffi_type_pointer;  sb[ns2]=tl_to_cstring(args[1+i]); av[i-1]=&sb[ns2++]; break;
            default: die("ffi_call: unknown type '%c'", sig[i]);
        }
    }

    ffi_type *rt = NULL;
    char rbuf[64];
    switch (rets) {
        case 'v': rt=&ffi_type_void;   break;
        case 'i': rt=&ffi_type_sint32;  break;
        case 'd': rt=&ffi_type_double;  break;
        case 'p': case 's': rt=&ffi_type_pointer; break;
        default: die("ffi_call: unknown return '%c'", rets);
    }

    ffi_cif cif;
    ffi_prep_cif(&cif, FFI_DEFAULT_ABI, nargs, rt, at);
    ffi_call(&cif, FFI_FN(fn), rbuf, av);

    for (int i = 0; i < (int)ns2; i++) free(sb[i]);
    free(sig);

    switch (rets) {
        case 'v': return vempty();
        case 'i': return vnum((double)*(int*)rbuf);
        case 'd': return vnum(*(double*)rbuf);
        case 'p': return vptr(*(void**)rbuf);
        case 's': {
            const char *str = *(const char**)rbuf;
            if (!str) return vempty();
            int n = strlen(str);
            Arr *a = aalloc(n, ARR_I8); a->len = n;
            memcpy(a->as.i8, str, n);
            return (Value){ .type = VAL_ARR, .as.arr = a };
        }
    }
    return vempty();
}
#endif

char *readf(const char *p);

void comp_include(Code *c) {
    tp++; comp_line = ts[tp].l; err_line = ts[tp].l; err_file = comp_file;
    if (ts[tp].t != T_STR) die("include requires a string path");
    Arr *a = (Arr*)ts[tp].s; tp++;
    int plen = a ? a->len : 0;
    if (plen >= 1024) die("include path too long");
    char path[1024];
    for (int i = 0; i < plen; i++) path[i] = (char)val_num(arr_item(a, i)); path[plen] = '\0';
    char full[1024];
    if (include_dir && include_dir[0])
        snprintf(full, sizeof(full), "%s/%s", include_dir, path);
    else { size_t nl = strlen(path); if (nl >= sizeof(full)) nl = sizeof(full)-1; memcpy(full, path, nl+1); }
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
            int pt = tp + 1;
            while (ts[pt].t == T_LB) {
                pt++; int bd = 1;
                while (bd > 0 && ts[pt].t != T_EOF) {
                    if (ts[pt].t == T_LB) bd++;
                    if (ts[pt].t == T_RB) bd--;
                    pt++;
                }
            }
            int is_assign = (ts[pt].t == T_EQ);
            if (is_assign) {
                char *nm = strdup(ts[tp].s); tp++; int idx_count = 0;
                while (ts[tp].t == T_LB) {
                    tp++;
                    do { comp_expr(c); idx_count++; } while (ts[tp].t == T_CM && (tp++, 1));
                    if (ts[tp].t != T_RB) die("expected ]"); tp++;
                }
                if (ts[tp].t != T_EQ) die("expected ="); tp++;
                int is_push = (idx_count == 0);
                if (is_push) {
                    int pn = tp;
                    while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                    is_push = ts[pn].t == T_ID && !strcmp(ts[pn].s, nm);
                    if (is_push) {
                        pn++; while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                        is_push = ts[pn].t == T_PL;
                        if (is_push) {
                            pn++; while (ts[pn].t == T_NL || ts[pn].t == T_SEMI) pn++;
                            is_push = ts[pn].t == T_LB;
                            if (is_push) {
                                pn++; int depth = 1;
                                while (depth > 0 && ts[pn].t != T_EOF) {
                                    if (ts[pn].t == T_LP || ts[pn].t == T_LB) depth++;
                                    if (ts[pn].t == T_RP || ts[pn].t == T_RB) depth--;
                                    if (depth == 1 && ts[pn].t == T_CM) { is_push = 0; break; }
                                    pn++;
                                }
                            }
                        }
                    }
                }
                if (is_push) {
                    tp++; tp++; tp++;
                    comp_expr(c);
                    while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
                    if (ts[tp].t != T_RB) die("expected ]");
                    tp++;
                    emit(c, (Instr){OC_PUSH, 0, 0, .name = nm});
                } else {
                    comp_expr(c);
                    if (idx_count > 0) emit(c, (Instr){OC_LVALS, idx_count, 0, .name = nm});
                    else emit(c, (Instr){OC_STORE, 0, 0, .name = nm});
                }
            } else { comp_expr(c); emit(c, (Instr){OC_POP, 0, 0}); }
            break;
        }
        default: comp_expr(c); emit(c, (Instr){OC_POP, 0, 0}); break;
    }
}

void comp_program(Code *c) {
    while (ts[tp].t != T_EOF) {
        while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
        if (ts[tp].t == T_EOF) break;
        comp_stmt(c);
    }
    emit(c, (Instr){OC_END, 0, 0});
}

void exec(Code *c) {
    int ip = 0;
    while (ip < c->len && !rf) {
        Instr *ins = &c->code[ip];
        err_line = ins->line; err_file = ins->file;
        switch (ins->op) {
            case OC_NUM: istk[++isp] = vnum(ins->num); break;
            case OC_NIL: istk[++isp] = nilv(); break;
            case OC_STR:
                istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = ins->arr };
                aretain(ins->arr); break;

            case OC_MAKE_ARR: {
                int n = ins->a;
                Value tmp[64];
                for (int i = n-1; i >= 0; i--) tmp[i] = istk[isp--];
                ArrKind k = n > 0 && n <= 64 ? detect_kind(tmp, n) : ARR_VAL;
                Arr *a = aalloc(n, k); a->len = n;
                if (k == ARR_VAL) {
                    for (int i = 0; i < n; i++) {
                        a->as.val[i] = tmp[i];
                        if (tmp[i].type == VAL_ARR) { aretain(tmp[i].as.arr); arelease(tmp[i].as.arr); }
                    }
                } else {
                    for (int i = 0; i < n; i++) {
                        double d = val_num(tmp[i]);
                        switch (k) {
                            case ARR_U8:  a->as.u8[i]  = (uint8_t)d; break;
                            case ARR_U16: a->as.u16[i] = (uint16_t)d; break;
                            case ARR_U32: a->as.u32[i] = (uint32_t)d; break;
                            case ARR_U64: a->as.u64[i] = (uint64_t)d; break;
                            case ARR_I8:  a->as.i8[i]  = (int8_t)d; break;
                            case ARR_I16: a->as.i16[i] = (int16_t)d; break;
                            case ARR_I32: a->as.i32[i] = (int32_t)d; break;
                            case ARR_I64: a->as.i64[i] = (int64_t)d; break;
                            case ARR_F32: a->as.f32[i] = (float)d; break;
                            case ARR_F64: a->as.f64[i] = d; break;
                            default: break;
                        }
                    }
                }
                istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = a }; break;
            }
            case OC_VAR: {
                Value v = sget(cs, ins->name);
                istk[++isp] = v; if (v.type == VAL_ARR) aretain(v.as.arr); break;
            }
            case OC_STORE: { Value v = istk[isp--]; sset(cs, ins->name, v); if (v.type == VAL_ARR) arelease(v.as.arr); break; }

            case OC_OP: {
                Value r = istk[isp--], l = istk[isp--], res = apply(ins->a, l, r);
                if (l.type==VAL_ARR) arelease(l.as.arr);
                if (r.type==VAL_ARR) arelease(r.as.arr);
                istk[++isp] = res; break;
            }
            case OC_UNARY: {
                Value v = istk[isp--];
                if (ins->a == T_BN) { istk[++isp] = truthy(v) ? nilv() : vnum(1); if (v.type==VAL_ARR) arelease(v.as.arr); }
                else if (ins->a == T_MI) { if (v.type!=VAL_NUM) die("minus on non-number"); istk[++isp] = vnum(-val_num(v)); }
                else if (ins->a == T_HASH) { if (v.type!=VAL_ARR) die("# requires array"); istk[++isp] = vnum((double)(v.as.arr ? v.as.arr->len : 0)); if (v.type==VAL_ARR) arelease(v.as.arr); }
                break;
            }

            case OC_INDEX: {
                Value idx = istk[isp--], arr = istk[isp--];
                if (arr.type != VAL_ARR) die("cannot index into non-array");
                if (idx.type == VAL_NUM) {
                    int ii = (int)val_num(idx);
                    if (!arr.as.arr || ii < 0 || ii >= arr.as.arr->len) die("index out of bounds");
                    Value res = arr_item(arr.as.arr, ii);
                    if (res.type == VAL_ARR) aretain(res.as.arr);
                    if (arr.type == VAL_ARR) arelease(arr.as.arr);
                    istk[++isp] = res;
                } else if (idx.type == VAL_ARR) {
                    Value cur = arr;
                    if (idx.as.arr) for (int j = 0; j < idx.as.arr->len; j++) {
                        int ii = (int)val_num(arr_item(idx.as.arr, j));
                        if (cur.type != VAL_ARR || !cur.as.arr || ii < 0 || ii >= cur.as.arr->len)
                            die("index out of bounds");
                        Value next = arr_item(cur.as.arr, ii);
                        if (next.type == VAL_ARR) aretain(next.as.arr);
                        if (cur.type == VAL_ARR) arelease(cur.as.arr);
                        cur = next;
                    }
                    if (idx.type == VAL_ARR) arelease(idx.as.arr);
                    istk[++isp] = cur;
                } else die("index must be number or array");
                break;
            }
            case OC_LVALS: {
                int depth = ins->a; char *name = ins->name;
                Value val = istk[isp--];
                Value indices[16];
                for (int j = depth-1; j >= 0; j--) indices[j] = istk[isp--];
                int si = -1;
                for (int i = 0; i < cs->c; i++) if (!strcmp(cs->n[i], name)) { si = i; break; }
                if (si < 0) die("undefined '%s'", name);
                Value *slot = &cs->v[si];
                for (int j = 0; j < depth; j++) {
                    amake_uniq(slot);
                    if (indices[j].type == VAL_NUM) {
                        int ii = (int)val_num(indices[j]);
                        if (slot->type != VAL_ARR || !slot->as.arr || ii < 0 || ii >= slot->as.arr->len)
                            die("index out of bounds");
                        slot = &slot->as.arr->as.val[ii];
                    } else if (indices[j].type == VAL_ARR) {
                        for (int k = 0; k < indices[j].as.arr->len; k++) {
                            int ii = (int)val_num(arr_item(indices[j].as.arr, k));
                            if (slot->type != VAL_ARR || !slot->as.arr || ii < 0 || ii >= slot->as.arr->len)
                                die("index out of bounds");
                            amake_uniq(slot); slot = &slot->as.arr->as.val[ii];
                        }
                    } else die("index must be number or array");
                }
                vassign(slot, val);
                if (val.type == VAL_ARR) arelease(val.as.arr);
                for (int j = 0; j < depth; j++)
                    if (indices[j].type == VAL_ARR) arelease(indices[j].as.arr);
                break;
            }

            case OC_CALL: {
                int fi = ins->a, ac = ins->b; Fn *f = &fs[fi];
                Value args[64];
                for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
                int saved_isp = isp;
                Value saved_stack[64];
                for (int j = 0; j <= saved_isp; j++) saved_stack[j] = istk[j];
                Scp *saved_cs = cs; cs = snew();
                int saved_cur_fi = cur_fi; cur_fi = fi;
                call_stack[++call_depth].fi = saved_cur_fi;
                call_stack[call_depth].line = err_line;
                call_stack[call_depth].file = err_file;
                for (int j = 0; j < f->a; j++)
                    sset(cs, f->p[j], (j < ac) ? args[j] : nilv());
                int saved_rf = rf; rf = 0; Value saved_rv = rv; isp = -1;
                exec(f->code);
                Value result = rf ? rv : nilv();
                sfree(cs); cs = saved_cs; cur_fi = saved_cur_fi;
                call_depth--;
                rf = saved_rf; rv = saved_rv; isp = saved_isp;
                for (int j = 0; j <= saved_isp; j++) istk[j] = saved_stack[j];
                istk[++isp] = result; break;
            }
            case OC_TCO: {
                int ac = ins->a; Fn *f = &fs[cur_fi];
                Value args[64];
                for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
                isp = -1; rf = 0;
                for (int j = 0; j < f->a; j++)
                    sset(cs, f->p[j], (j < ac) ? args[j] : nilv());
                ip = -1; break;
            }
            case OC_RET: { rv = istk[isp--]; rf = 1; break; }
            case OC_POP: { if (isp >= 0) { Value v = istk[isp--]; if (v.type == VAL_ARR) arelease(v.as.arr); } break; }

            case OC_JZ: {
                Value v = istk[isp--]; int t = truthy(v);
                if (v.type == VAL_ARR) arelease(v.as.arr);
                if (!t) { ip = ins->a; continue; } break;
            }
            case OC_JMP: { ip = ins->a; continue; }

            case OC_TYPE: {
                Value v = istk[isp--];
                if (v.type == VAL_NUM) istk[++isp] = vnum((double)v.nkind);
                else if (v.type == VAL_ARR) {
                    if (v.as.arr) istk[++isp] = vnum((double)(100 + v.as.arr->kind));
                    else istk[++isp] = vnum(-1);
                } else istk[++isp] = vnum(-2);
                break;
            }
            case OC_PRINT: { print_val(istk[isp--]); printf("\n"); break; }
            case OC_INPUT: {
                char buf[1024]; int n;
                if (!fgets(buf, 1024, stdin)) { n = 0; } else { n = strlen(buf); if (n && buf[n-1]=='\n') n--; }
                Arr *a = aalloc(n, ARR_I8); a->len = n;
                memcpy(a->as.i8, buf, n);
                istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = a }; break;
            }

            case OC_ASSERT: {
                Code *sub = ins->sub; int ac = ins->a;
                int saved_isp = isp, saved_rf = rf; Value saved_rv = rv;
                Scp *saved_cs = cs; int saved_ac = assert_catching;
                int saved_cd = call_depth;
                assert_catching = 1;
                if (setjmp(assert_jmp)) {
                    assert_catching = 0; isp = saved_isp; rf = saved_rf; rv = saved_rv; cs = saved_cs;
                    call_depth = saved_cd;
                    assert_catching = saved_ac;
                    int n = strlen(assert_msg); Arr *a = aalloc(n, ARR_I8); a->len = n;
                    memcpy(a->as.i8, assert_msg, n);
                    istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = a };
                } else {
                    isp = saved_isp; rf = 0; Value asv[64];
                    for (int j=0;j<=saved_isp;j++) asv[j]=istk[j];
                    exec(sub); assert_catching = 0;
                    for (int j=0;j<=saved_isp;j++) istk[j]=asv[j];
                    assert_catching = saved_ac;
                    Value result = (isp > saved_isp) ? istk[isp--] : nilv();
                    if (ac >= 1 && truthy(result)) istk[++isp] = nilv();
                    else { const char *msg = "assertion failed"; int n = strlen(msg);
                        Arr *a = aalloc(n, ARR_I8); a->len = n;
                        memcpy(a->as.i8, msg, n);
                        istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = a }; }
                }
                break;
            }

            case OC_CFUNC: {
                int ci = ins->a, ac = ins->b;
                Value args[64];
                for (int j = ac-1; j >= 0; j--) args[j] = istk[isp--];
                Value result = cregs[ci].func(ac, args);
                istk[++isp] = result;
                break;
            }

            case OC_PUSH: {
                char *name = ins->name;
                Value elem = istk[isp--];
                int si = -1;
                for (int i = 0; i < cs->c; i++) if (!strcmp(cs->n[i], name)) { si = i; break; }
                if (si < 0) die("undefined '%s'", name);
                Value *slot = &cs->v[si];
                if (slot->type != VAL_ARR) die("cannot append to non-array");
                if (!slot->as.arr) {
                    slot->as.arr = aalloc(4, ARR_VAL);
                    slot->as.arr->len = 0;
                } else {
                    amake_uniq(slot);
                }
                int len = slot->as.arr->len;
                if (len >= slot->as.arr->cap) {
                    slot->as.arr->cap = slot->as.arr->cap ? slot->as.arr->cap * 2 : 4;
                    slot->as.arr->as.val = realloc(slot->as.arr->as.val, slot->as.arr->cap * sizeof(Value));
                }
                vassign(&slot->as.arr->as.val[len], elem);
                slot->as.arr->len = len + 1;
                if (elem.type == VAL_ARR) arelease(elem.as.arr);
                break;
            }

            case OC_END: return;
        }
        ip++;
    }
}

char *readf(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *b = malloc(sz + 1); fread(b, 1, sz, f); b[sz] = 0;
    fclose(f); return b;
}

int main(int a, char **v) {
    cs = snew(); cur_fi = -1;
#ifdef TL_FFI
    tl_register("dlopen",   tl_dlopen);
    tl_register("dlsym",    tl_dlsym);
    tl_register("dlclose",  tl_dlclose);
    tl_register("ffi_call", tl_ffi_call);
#endif
    if (a >= 2) {
        char *src = readf(v[1]); if (!src) { fprintf(stderr, "cannot read '%s'\n", v[1]); return 1; }
        char dir[1024] = {0}; const char *slash = strrchr(v[1], '/');
        if (slash) { memcpy(dir, v[1], slash - v[1]); } include_dir = dir;
        comp_file = strdup(v[1]);
        lex(src); Code *code = new_code(); comp_program(code); free(src); free(comp_file); comp_file = NULL;
        exec(code); code_free(code); free(ts);
    } else {
        char buf[65536];
        while (1) {
            printf("> "); fflush(stdout); buf[0] = 0;
            while (1) {
                char line[4096];
                if (!fgets(line, sizeof(line), stdin)) { printf("\n"); goto done; }
                strcat(buf, line);
                int op = 0, cl = 0;
                for (char *p = buf; *p; p++) { if (*p == '{') op++; if (*p == '}') cl++; }
                if (op == cl) break;
                printf("  "); fflush(stdout);
            }
            comp_file = NULL;
            lex(buf); Code *code = new_code(); comp_program(code);
            { int _sac = assert_catching; assert_catching = 1;
              if (setjmp(assert_jmp)) { isp = -1; rf = 0; call_depth = -1; }
              else { exec(code); }
              assert_catching = _sac; }
            code_free(code); free(ts);
        }
        done:;
    }
    sfree(cs); return 0;
}
