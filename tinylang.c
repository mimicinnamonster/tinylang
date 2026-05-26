#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <setjmp.h>

typedef enum { VAL_NUM, VAL_ARR } Type;
typedef struct Arr { int refcount, len, cap; struct Value *items; } Arr;
typedef struct Value { Type type; union { double num; Arr *arr; } as; } Value;

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
    OC_PRINT, OC_INPUT, OC_ASSERT,
    OC_END,
} OC;

typedef struct Code { struct Instr *code; int len, cap; } Code;
typedef struct Instr {
    OC op; int a, b; double num; Arr *arr; char *name; Code *sub;
} Instr;

Tok *ts; int tc, tp;
Scp *cs;
int rf; Value rv;
int cur_fi;
static char *include_dir;
jmp_buf assert_jmp; int assert_catching;
char assert_msg[512];

typedef struct { char *n, **p; int a; Code *code; } Fn;
Fn *fs; int fc, fm;
Value istk[4096]; int isp;

Value vnum(double n) { return (Value){ .type = VAL_NUM, .as.num = n }; }

Arr *aalloc(int c) {
    Arr *a = calloc(1, sizeof(Arr)); a->refcount = 1;
    a->cap = c; a->items = c ? calloc(c, sizeof(Value)) : NULL; return a;
}

void arelease(Arr *a) {
    if (!a) return;
    if (--a->refcount > 0) return;
    for (int i = 0; i < a->len; i++)
        if (a->items[i].type == VAL_ARR) arelease(a->items[i].as.arr);
    free(a->items); free(a);
}

void aretain(Arr *a) { if (a) a->refcount++; }

Arr *adeep_copy(Arr *s) {
    if (!s) return NULL;
    Arr *d = aalloc(s->cap); d->len = s->len;
    for (int i = 0; i < s->len; i++) {
        d->items[i] = s->items[i];
        if (d->items[i].type == VAL_ARR) aretain(d->items[i].as.arr);
    }
    return d;
}

void amake_uniq(Value *v) {
    if (v->type != VAL_ARR || !v->as.arr || v->as.arr->refcount <= 1) return;
    Arr *old = v->as.arr; v->as.arr = adeep_copy(old); arelease(old);
}

void vassign(Value *d, Value s) {
    if (d->type == VAL_ARR) arelease(d->as.arr);
    d->type = s.type;
    if (s.type == VAL_NUM) d->as.num = s.as.num;
    else { d->as.arr = s.as.arr; aretain(d->as.arr); }
}

void print_val(Value v) {
    if (v.type == VAL_NUM) {
        if (v.as.num == (double)(int64_t)v.as.num) printf("%lld", (int64_t)v.as.num);
        else printf("%g", v.as.num);
    } else if (v.type == VAL_ARR) {
        int is_str = 1;
        if (v.as.arr) for (int i = 0; i < v.as.arr->len && is_str; i++) {
            if (v.as.arr->items[i].type != VAL_NUM) { is_str = 0; break; }
            int c = (int)v.as.arr->items[i].as.num;
            if (c != 10 && c != 13 && c != 9 && (c < 32 || c > 126)) is_str = 0;
        }
        if (is_str && v.as.arr) for (int i = 0; i < v.as.arr->len; i++)
            putchar((int)v.as.arr->items[i].as.num);
        else {
            putchar('[');
            if (v.as.arr) for (int i = 0; i < v.as.arr->len; i++) {
                if (i) printf(", "); print_val(v.as.arr->items[i]);
            }
            putchar(']');
        }
    }
}

int truthy(Value v) { return !(v.type == VAL_ARR && (!v.as.arr || v.as.arr->len == 0)); }

int veq(Value a, Value b) {
    if (a.type != b.type) return 0;
    if (a.type == VAL_NUM) return a.as.num == b.as.num;
    if (!a.as.arr && !b.as.arr) return 1;
    if (!a.as.arr || !b.as.arr || a.as.arr->len != b.as.arr->len) return 0;
    for (int i = 0; i < a.as.arr->len; i++)
        if (!veq(a.as.arr->items[i], b.as.arr->items[i])) return 0;
    return 1;
}

void die(const char *f, ...) {
    va_list ap, aq; va_start(ap, f); va_copy(aq, ap);
    vfprintf(stderr, f, ap); fputc('\n', stderr);
    if (assert_catching) { vsnprintf(assert_msg, sizeof(assert_msg), f, aq); longjmp(assert_jmp, 1); }
    va_end(aq); va_end(ap); exit(1);
}

#define nilv() ((Value){ .type = VAL_ARR })

Value apply(int op, Value l, Value r) {
    switch (op) {
        case T_PL:
            if (l.type == VAL_NUM && r.type == VAL_NUM) return vnum(l.as.num + r.as.num);
            if (l.type == VAL_ARR && r.type == VAL_ARR) {
                int n = (l.as.arr ? l.as.arr->len : 0) + (r.as.arr ? r.as.arr->len : 0);
                Arr *a = aalloc(n); a->len = n; int k = 0;
                for (int i = 0; i < (l.as.arr ? l.as.arr->len : 0); i++)
                    vassign(&a->items[k++], l.as.arr->items[i]);
                for (int i = 0; i < (r.as.arr ? r.as.arr->len : 0); i++)
                    vassign(&a->items[k++], r.as.arr->items[i]);
                return (Value){ .type = VAL_ARR, .as.arr = a };
            } die("'+' type mismatch");
        case T_MI:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'-' requires numbers");
            return vnum(l.as.num - r.as.num);

        case T_ST:
            if (l.type == VAL_NUM && r.type == VAL_NUM) return vnum(l.as.num * r.as.num);
            if (l.type == VAL_ARR && r.type == VAL_NUM) {
                int bl = l.as.arr ? l.as.arr->len : 0, n = (int)r.as.num;
                if (n <= 0) return nilv();
                Arr *a = aalloc(bl * n); a->len = bl * n;
                for (int i = 0; i < n; i++)
                    for (int j = 0; j < bl; j++)
                        vassign(&a->items[i * bl + j], l.as.arr->items[j]);
                return (Value){ .type = VAL_ARR, .as.arr = a };
            } die("'*' type mismatch");
        case T_SL:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'/' requires numbers");
            if (r.as.num == 0) die("division by zero"); return vnum(l.as.num / r.as.num);
        case T_PC:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'%%' requires numbers");
            if (r.as.num == 0) die("modulo by zero"); return vnum(fmod(l.as.num, r.as.num));
        case T_AM: case T_PI: case T_CA: case T_AT: {
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("bitwise requires numbers");
            int64_t a = (int64_t)l.as.num, b = (int64_t)r.as.num;
            if (op == T_AM) return vnum((double)(a & b));
            if (op == T_PI) return vnum((double)(a | b));
            if (op == T_CA) return vnum((double)(a ^ b));
            int64_t s = (int64_t)r.as.num;
            if (s < 0) { s = -s; if (s > 63) s = 63; return vnum((double)((uint64_t)a >> s)); }
            else { if (s > 63) s = 63; return vnum((double)((uint64_t)a << s)); }
        }
        case T_EQ: return veq(l, r) ? vnum(1) : nilv();
        case T_NE: return veq(l, r) ? nilv() : vnum(1);
        case T_LT:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'<' requires numbers");
            return l.as.num < r.as.num ? vnum(1) : nilv();
        case T_GT:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'>' requires numbers");
            return l.as.num > r.as.num ? vnum(1) : nilv();
        case T_LE:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'<=' requires numbers");
            return l.as.num <= r.as.num ? vnum(1) : nilv();
        case T_GE:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'>=' requires numbers");
            return l.as.num >= r.as.num ? vnum(1) : nilv();
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
            char b[64]; int i = 0, dot = 0;
            if (c == '.') { dot = 1; b[i++] = '.'; ac(); c = pc(); }
            while (isdigit(c)) { b[i++] = c; ac(); c = pc(); }
            if (c == '.' && !dot) {
                dot = 1; b[i++] = '.'; ac(); c = pc();
                while (isdigit(c)) { b[i++] = c; ac(); c = pc(); }
            } b[i] = 0; tk.t = T_NUM; tk.n = atof(b); goto em;
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
            Arr *a = aalloc(i); a->len = i;
            for (int j = 0; j < i; j++) a->items[j] = vnum((unsigned char)b[j]);
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
    c->code[c->len++] = ins;
}

Code *new_code(void) { return calloc(1, sizeof(Code)); }

void code_free(Code *c) {
    if (!c) return;
    for (int i = 0; i < c->len; i++) {
        free(c->code[i].name);
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
    Tok t = ts[tp];
    switch (t.t) {
        case T_NUM: tp++; emit(c, (Instr){OC_NUM, 0, 0, .num = t.n}); break;
        case T_NIL: tp++; emit(c, (Instr){OC_NIL, 0, 0}); break;
        case T_STR: {
            tp++; Arr *o = (Arr*)t.s;
            emit(c, (Instr){OC_STR, 0, 0, .arr = o});
            break;
        }
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
                else {
                    int fi = ffind(t.s);
                    if (fi < 0) die("undefined function '%s'", t.s);
                    emit(c, (Instr){OC_CALL, fi, ac});
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
    tp++; comp_expr(c);
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
    int loop = c->len; tp++;
    comp_expr(c); int jz = c->len; emit(c, (Instr){OC_JZ, 0, 0});
    comp_block(c);
    emit(c, (Instr){OC_JMP, loop, 0}); c->code[jz].a = c->len;
}

void comp_return(Code *c) {
    tp++;
    int is_tc = (cur_fi >= 0 && ts[tp].t == T_ID &&
                 !strcmp(ts[tp].s, fs[cur_fi].n) && ts[tp+1].t == T_LP);
    comp_expr(c);
    if (is_tc && c->len > 0 && c->code[c->len-1].op == OC_CALL) {
        Instr *last = &c->code[c->len-1]; int ac = last->b;
        last->op = OC_TCO; last->a = ac; last->b = 0;
    } else if (!is_tc) {
        emit(c, (Instr){OC_RET, 0, 0});
    }
}

void comp_fn(Code *c) {
    tp++;
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
    int tco_pos = body->len - 1;
    while (tco_pos >= 0 && (body->code[tco_pos].op == OC_RET || body->code[tco_pos].op == OC_END)) tco_pos--;
    if (tco_pos >= 0 && body->code[tco_pos].op == OC_CALL &&
        body->code[tco_pos].a < fc && !strcmp(fs[body->code[tco_pos].a].n, name)) {
        int ac = body->code[tco_pos].b; body->code[tco_pos].op = OC_TCO; body->code[tco_pos].a = ac;
    }
}

char *readf(const char *p);

void comp_include(Code *c) {
    tp++;
    if (ts[tp].t != T_STR) die("include requires a string path");
    Arr *a = (Arr*)ts[tp].s; tp++;
    int plen = a ? a->len : 0;
    if (plen >= 1024) die("include path too long");
    char path[1024];
    for (int i = 0; i < plen; i++) path[i] = (char)a->items[i].as.num; path[plen] = '\0';
    char full[1024];
    if (include_dir && include_dir[0])
        snprintf(full, sizeof(full), "%s/%s", include_dir, path);
    else { size_t nl = strlen(path); if (nl >= sizeof(full)) nl = sizeof(full)-1; memcpy(full, path, nl+1); }
    char *content = readf(full);
    if (!content) die("cannot include '%s'", full);
    Tok *saved_ts = ts; int saved_tc = tc, saved_tp = tp; char *saved_dir = include_dir;
    char inc_dir[1024] = {0};
    const char *sl = strrchr(full, '/');
    if (sl) { memcpy(inc_dir, full, sl - full); inc_dir[sl - full] = '\0'; }
    include_dir = inc_dir;
    lex(content);
    while (ts[tp].t != T_EOF) {
        while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
        if (ts[tp].t == T_EOF) break;
        comp_stmt(c);
    }
    free(ts); ts = saved_ts; tc = saved_tc; tp = saved_tp;
    include_dir = saved_dir; free(content);
}

void comp_stmt(Code *c) {
    while (ts[tp].t == T_NL || ts[tp].t == T_SEMI) tp++;
    if (ts[tp].t == T_EOF || ts[tp].t == T_RC) return;
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
                comp_expr(c);
                if (idx_count > 0) emit(c, (Instr){OC_LVALS, idx_count, 0, .name = nm});
                else emit(c, (Instr){OC_STORE, 0, 0, .name = nm});
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
        switch (ins->op) {
            case OC_NUM: istk[++isp] = vnum(ins->num); break;
            case OC_NIL: istk[++isp] = nilv(); break;
            case OC_STR:
                istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = ins->arr };
                aretain(ins->arr); break;

            case OC_MAKE_ARR: {
                int n = ins->a; Arr *a = aalloc(n); a->len = n;
                for (int i = n-1; i >= 0; i--) {
                    a->items[i] = istk[isp--];
                    if (a->items[i].type == VAL_ARR) aretain(a->items[i].as.arr);
                }
                istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = a }; break;
            }
            case OC_VAR: {
                Value v = sget(cs, ins->name);
                istk[++isp] = v; if (v.type == VAL_ARR) aretain(v.as.arr); break;
            }
            case OC_STORE: sset(cs, ins->name, istk[isp--]); break;

            case OC_OP: {
                Value r = istk[isp--], l = istk[isp--], res = apply(ins->a, l, r);
                if (l.type==VAL_ARR) arelease(l.as.arr);
                if (r.type==VAL_ARR) arelease(r.as.arr);
                istk[++isp] = res; break;
            }
            case OC_UNARY: {
                Value v = istk[isp--];
                if (ins->a == T_BN) { istk[++isp] = truthy(v) ? nilv() : vnum(1); if (v.type==VAL_ARR) arelease(v.as.arr); }
                else if (ins->a == T_MI) { if (v.type!=VAL_NUM) die("minus on non-number"); istk[++isp] = vnum(-v.as.num); }
                else if (ins->a == T_HASH) { if (v.type!=VAL_ARR) die("# requires array"); istk[++isp] = vnum((double)(v.as.arr ? v.as.arr->len : 0)); if (v.type==VAL_ARR) arelease(v.as.arr); }
                break;
            }
            case OC_INDEX: {
                Value idx = istk[isp--], arr = istk[isp--];
                if (arr.type != VAL_ARR) die("cannot index into non-array");
                if (idx.type == VAL_NUM) {
                    int ii = (int)idx.as.num;
                    if (!arr.as.arr || ii < 0 || ii >= arr.as.arr->len) die("index out of bounds");
                    Value res = arr.as.arr->items[ii];
                    if (res.type == VAL_ARR) aretain(res.as.arr);
                    if (arr.type == VAL_ARR) arelease(arr.as.arr);
                    istk[++isp] = res;
                } else if (idx.type == VAL_ARR) {
                    Value cur = arr;
                    if (idx.as.arr) for (int j = 0; j < idx.as.arr->len; j++) {
                        int ii = (int)idx.as.arr->items[j].as.num;
                        if (cur.type != VAL_ARR || !cur.as.arr || ii < 0 || ii >= cur.as.arr->len)
                            die("index out of bounds");
                        Value next = cur.as.arr->items[ii];
                        if (next.type == VAL_ARR) aretain(next.as.arr);
                        if (cur.type == VAL_ARR) arelease(cur.as.arr);
                        cur = next;
                    }
                    if (arr.type == VAL_ARR) arelease(arr.as.arr);
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
                        int ii = (int)indices[j].as.num;
                        if (slot->type != VAL_ARR || !slot->as.arr || ii < 0 || ii >= slot->as.arr->len)
                            die("index out of bounds");
                        slot = &slot->as.arr->items[ii];
                    } else if (indices[j].type == VAL_ARR) {
                        for (int k = 0; k < indices[j].as.arr->len; k++) {
                            int ii = (int)indices[j].as.arr->items[k].as.num;
                            if (slot->type != VAL_ARR || !slot->as.arr || ii < 0 || ii >= slot->as.arr->len)
                                die("index out of bounds");
                            amake_uniq(slot); slot = &slot->as.arr->items[ii];
                        }
                    } else die("index must be number or array");
                }
                vassign(slot, val); break;
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
                for (int j = 0; j < f->a; j++)
                    sset(cs, f->p[j], (j < ac) ? args[j] : nilv());
                int saved_rf = rf; rf = 0; Value saved_rv = rv; isp = -1;
                exec(f->code);
                Value result = rf ? rv : nilv();
                sfree(cs); cs = saved_cs; cur_fi = saved_cur_fi;
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
            case OC_PRINT: { print_val(istk[isp--]); printf("\n"); break; }
            case OC_INPUT: {
                char buf[1024]; int n;
                if (!fgets(buf, 1024, stdin)) { n = 0; } else { n = strlen(buf); if (n && buf[n-1]=='\n') n--; }
                Arr *a = aalloc(n); a->len = n;
                for (int j = 0; j < n; j++) a->items[j] = vnum((unsigned char)buf[j]);
                istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = a }; break;
            }
            case OC_ASSERT: {
                Code *sub = ins->sub; int ac = ins->a;
                int saved_isp = isp, saved_rf = rf; Value saved_rv = rv;
                Scp *saved_cs = cs; int saved_ac = assert_catching;
                assert_catching = 1;
                if (setjmp(assert_jmp)) {
                    assert_catching = 0; isp = saved_isp; rf = saved_rf; rv = saved_rv; cs = saved_cs;
                    assert_catching = saved_ac;
                    int n = strlen(assert_msg); Arr *a = aalloc(n); a->len = n;
                    for (int j = 0; j < n; j++) a->items[j] = vnum((unsigned char)assert_msg[j]);
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
                        Arr *a = aalloc(n); a->len = n;
                        for (int j = 0; j < n; j++) a->items[j] = vnum((unsigned char)msg[j]);
                        istk[++isp] = (Value){ .type = VAL_ARR, .as.arr = a }; }
                }
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
    if (a >= 2) {
        char *src = readf(v[1]); if (!src) { fprintf(stderr, "cannot read '%s'\n", v[1]); return 1; }
        char dir[1024] = {0}; const char *slash = strrchr(v[1], '/');
        if (slash) { memcpy(dir, v[1], slash - v[1]); } include_dir = dir;
        lex(src); Code *code = new_code(); comp_program(code); free(src); exec(code);
        code_free(code); free(ts);
    } else {
        printf("TinyLang v0.1\n");
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
            lex(buf); Code *code = new_code(); comp_program(code); exec(code);
            code_free(code); free(ts); cs = snew();
        }
        done:;
    }
    sfree(cs); return 0;
}
