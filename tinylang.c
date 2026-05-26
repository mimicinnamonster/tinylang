/* TinyLang — tree-walk interpreter, refcount+COW, TCO */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

/* ========================== TYPES ========================== */
typedef enum { VAL_NUM, VAL_ARR } Type;

typedef struct Arr { int refcount, len, cap; struct Value *items; } Arr;

typedef struct Value { Type type; union { double num; Arr *arr; } as; } Value;

typedef enum {
    T_EOF, T_NUM, T_ID, T_STR, T_NIL,
    T_LP, T_RP, T_LB, T_RB, T_LC, T_RC, T_CM,
    T_PL, T_MI, T_ST, T_SL, T_PC,
    T_AM, T_PI, T_CA, T_AT, T_BN,
    T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_NL, T_IF, T_ELIF, T_ELSE, T_WH, T_FN, T_RT,
} TK;

typedef struct { TK t; double n; char *s; int l; } Tok;
typedef struct { char **n; Value *v; int c, m; } Scp;
typedef struct { char *n, **p; int a, bs, be; } Fn;

/* ========================== GLOBALS ========================== */
Tok *ts; int tc, tp;
Scp *cs;
Fn *fs; int fc, fm;
int rf; Value rv;
int cur_fi; int last_tok; /* current function index for TCO (-1 = top level) */
int tco;    /* tail call flag */

/* ========================== VALUE ========================== */
Value vnum(double n) { return (Value){ .type = VAL_NUM, .as.num = n }; }
Value vempty(void) { return (Value){ .type = VAL_ARR, .as.arr = NULL }; }

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
    Arr *old = v->as.arr;
    v->as.arr = adeep_copy(old);
    arelease(old);
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
        int pr = 1;
        if (v.as.arr) for (int i = 0; i < v.as.arr->len && pr; i++) {
            if (v.as.arr->items[i].type != VAL_NUM) { pr = 0; break; }
            int c = (int)v.as.arr->items[i].as.num;
            if (c != 10 && c != 13 && c != 9 && (c < 32 || c > 126)) pr = 0;
        }
        if (pr && v.as.arr) for (int i = 0; i < v.as.arr->len; i++) putchar((int)v.as.arr->items[i].as.num);
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
    va_list ap; va_start(ap, f);
    fprintf(stderr, "error: "); vfprintf(stderr, f, ap); fprintf(stderr, "\n");
    va_end(ap); exit(1);
}

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
                if (n <= 0) return vempty();
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
        case T_EQ: return veq(l, r) ? vnum(1) : vempty();
        case T_NE: return veq(l, r) ? vempty() : vnum(1);
        case T_LT:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'<' requires numbers");
            return l.as.num < r.as.num ? vnum(1) : vempty();
        case T_GT:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'>' requires numbers");
            return l.as.num > r.as.num ? vnum(1) : vempty();
        case T_LE:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'<=' requires numbers");
            return l.as.num <= r.as.num ? vnum(1) : vempty();
        case T_GE:
            if (l.type != VAL_NUM || r.type != VAL_NUM) die("'>=' requires numbers");
            return l.as.num >= r.as.num ? vnum(1) : vempty();
        default: die("unknown op");
    } return vempty();
}

/* ========================== LEXER ========================== */
const char *src; int sl;
static int pc(void) { return (unsigned char)*src; }
static int ac(void) { int c = (unsigned char)*src++; if (c == '\n') sl++; return c; }

void lex(const char *s) {
    src = s; sl = 1; tc = 0; tp = 0; int m = 8192; ts = malloc(m * sizeof(Tok));
    for (;;) {
        while (pc() == ' ' || pc() == '\t') ac();
        int c = pc(); Tok tk = { .l = sl };
        if (c == '\n') {
            ac();
            /* Go-style semicolon insertion */
            if (last_tok == T_NUM || last_tok == T_ID || last_tok == T_STR ||
                last_tok == T_NIL || last_tok == T_RP || last_tok == T_RB || last_tok == T_RC)
                { tk.t = T_NL; last_tok = T_NL; goto em; }
            continue;
        }
        if (c == 0) { tk.t = T_EOF; goto em; }
        if (c == '/' && src[1] == '/') { while (pc() && pc() != '\n') ac(); continue; }
        if (isdigit(c) || (c == '.' && isdigit(src[1]))) {
            char b[64]; int i = 0, dt = 0;
            if (c == '.') { dt = 1; b[i++] = '.'; ac(); c = pc(); }
            while (isdigit(c)) { b[i++] = c; ac(); c = pc(); }
            if (c == '.' && !dt) {
                dt = 1; b[i++] = '.'; ac(); c = pc();
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
            case ',': tk.t=T_CM; break;
            case '+': tk.t=T_PL; break; case '-': tk.t=T_MI; break;
            case '*': tk.t=T_ST; break; case '/': tk.t=T_SL; break;
            case '%': tk.t=T_PC; break;
            case '&': tk.t=T_AM; break; case '|': tk.t=T_PI; break;
            case '^': tk.t=T_CA; break; case '@': tk.t=T_AT; break;
            case '!': if (pc()=='='){ac();tk.t=T_NE;}else tk.t=T_BN; break;
            case '=': tk.t=T_EQ; break;
            case '<': if (pc()=='='){ac();tk.t=T_LE;}else tk.t=T_LT; break;
            case '>': if (pc()=='='){ac();tk.t=T_GE;}else tk.t=T_GT; break;
            default: { char m[2]={c,0}; die("unexpected '%s'",m); }
        }
        em: if (tc>=m){m*=2;ts=realloc(ts,m*sizeof(Tok));} ts[tc++]=tk;
        last_tok = tk.t;
        if (tk.t==T_EOF) break;
    }
}

/* ========================== SCOPE ========================== */
Scp *snew(void) { return calloc(1, sizeof(Scp)); }
void sfree(Scp *s) {
    for (int i = 0; i < s->c; i++) { if (s->v[i].type == VAL_ARR) arelease(s->v[i].as.arr); free(s->n[i]); }
    free(s->n); free(s->v); free(s);
}
Value sget(Scp *s, const char *n) {
    for (int i = 0; i < s->c; i++) if (!strcmp(s->n[i], n)) return s->v[i];
    die("undefined '%s'", n); return vempty();
}
void sset(Scp *s, const char *n, Value v) {
    for (int i = 0; i < s->c; i++)
        if (!strcmp(s->n[i], n)) { vassign(&s->v[i], v); return; }
    if (s->c >= s->m) { s->m = s->m ? s->m*2 : 4;
        s->n = realloc(s->n, s->m*sizeof(char*)); s->v = realloc(s->v, s->m*sizeof(Value)); }
    s->n[s->c] = strdup(n);
    s->v[s->c] = v; if (v.type == VAL_ARR) aretain(v.as.arr);
    s->c++;
}
int ffind(const char *n) { for (int i = 0; i < fc; i++) if (!strcmp(fs[i].n, n)) return i; return -1; }

/* ========================== PARSER ========================== */
Tok peek(void) { return ts[tp]; }
Tok adv(void) { return ts[tp++]; }
int mtch(TK t) { if (ts[tp].t == t) { tp++; return 1; } return 0; }
void xpct(TK t) { if (!mtch(t)) die("expected token at line %d", ts[tp].l); }
Value expr(void); Value prim(void); void stmt(void); void blk(void);

Value prim(void) {
    while (peek().t == T_NL) adv();
    Tok t = adv();
    switch (t.t) {
        case T_NUM: return vnum(t.n);
        case T_NIL: return vempty();
        case T_STR: { Arr *a = (Arr*)t.s; return (Value){ .type = VAL_ARR, .as.arr = a }; }
        case T_ID: {
            if (peek().t == T_LP) {
                adv(); Value as[64]; int ac2 = 0;
                if (peek().t != T_RP) do as[ac2++] = expr(); while (mtch(T_CM));
                xpct(T_RP);
                if (!strcmp(t.s, "print")) {
                    if (ac2 < 1) die("print needs 1 arg"); print_val(as[0]); printf("\n"); return vempty();
                }
                if (!strcmp(t.s, "len")) {
                    if (ac2 < 1) die("len needs 1 arg");
                    if (as[0].type != VAL_ARR) die("len needs array");
                    return vnum((double)(as[0].as.arr ? as[0].as.arr->len : 0));
                }
                if (!strcmp(t.s, "input")) {
                    char b[1024]; if (!fgets(b,1024,stdin)) return vempty();
                    int n = strlen(b); if (n && b[n-1]=='\n') n--;
                    Arr *a = aalloc(n); a->len = n;
                    for (int i = 0; i < n; i++) a->items[i] = vnum((unsigned char)b[i]);
                    return (Value){ .type = VAL_ARR, .as.arr = a };
                }
                /* TCO: if in tail position and calling ourselves, optimize */
                if (tco && cur_fi >= 0 && !strcmp(t.s, fs[cur_fi].n)) {
                    /* self-tail-call — rebind params and restart */
                    Fn *f = &fs[cur_fi];
                    for (int i = 0; i < f->a; i++)
                        sset(cs, f->p[i], (i < ac2) ? as[i] : vempty());
                    tp = f->bs; rf = 0; tco = 0;
                    return vempty(); /* return value ignored */
                }
                /* normal function call */
                int fi = ffind(t.s); if (fi < 0) die("undefined function '%s'", t.s);
                Fn *f = &fs[fi]; int sp = tp; Scp *ss = cs; int sf = cur_fi;
                cs = snew(); cur_fi = fi;
                for (int i = 0; i < f->a; i++)
                    sset(cs, f->p[i], (i < ac2) ? as[i] : vempty());
                tp = f->bs; int sr = rf; rf = 0;
                while (tp < f->be && !rf) stmt();
                Value r = rf ? rv : vempty(); rf = sr;
                sfree(cs); cs = ss; tp = sp; cur_fi = sf;
                return r;
            }
            Value v = sget(cs, t.s);
            while (peek().t == T_LB) {
                adv(); Value is[64]; int n = 0;
                do is[n++] = expr(); while (mtch(T_CM));
                xpct(T_RB);
                for (int i = 0; i < n; i++) {
                    if (is[i].type == VAL_NUM) {
                        Arr *a = v.as.arr; int ii = (int)is[i].as.num;
                        if (!a || ii < 0 || ii >= a->len) die("index out of bounds");
                        v = a->items[ii];
                    } else if (is[i].type == VAL_ARR) {
                        if (is[i].as.arr) for (int j = 0; j < is[i].as.arr->len; j++) {
                            Arr *a = v.as.arr; int ii = (int)is[i].as.arr->items[j].as.num;
                            if (!a || ii < 0 || ii >= a->len) die("index out of bounds");
                            v = a->items[ii];
                        }
                    } else die("index must be number or array");
                }
            }
            return v;
        }
        case T_LB: {
            if (mtch(T_RB)) return vempty();
            Value is[8192]; int n = 0;
            do is[n++] = expr(); while (mtch(T_CM));
            xpct(T_RB);
            Arr *a = aalloc(n); a->len = n;
            for (int i = 0; i < n; i++) {
                a->items[i] = is[i];
                if (is[i].type == VAL_ARR) aretain(is[i].as.arr);
            }
            return (Value){ .type = VAL_ARR, .as.arr = a };
        }
        case T_LP: { Value v = expr(); xpct(T_RP); return v; }
        case T_BN: { Value v = prim(); return truthy(v) ? vempty() : vnum(1); }
        case T_MI: { Value v = prim(); if (v.type != VAL_NUM) die("minus on non-number"); return vnum(-v.as.num); }
        default: die("unexpected token at line %d", t.l); return vempty();
    }
}

Value expr(void) {
    Value l = prim(); TK op = peek().t;
    if (op >= T_PL && op <= T_GE) {
        adv(); Value r = prim();
        if (peek().t >= T_PL && peek().t <= T_GE) die("chaining ops needs () at line %d", peek().l);
        return apply(op, l, r);
    }
    return l;
}

void blk(void) {
    while (peek().t == T_NL) adv();
    xpct(T_LC);
    while (peek().t != T_RC && peek().t != T_EOF && !rf) {
        if (peek().t == T_NL) { adv(); continue; }
        stmt();
    }
    if (rf) {
        int d = 1; while (d > 0 && peek().t != T_EOF) {
            if (peek().t == T_LC) d++; if (peek().t == T_RC) d--; tp++;
        }
    } else xpct(T_RC);
}

void skip_blk(void) {
    int d = 1; while (peek().t == T_NL) adv(); xpct(T_LC);
    while (d > 0 && peek().t != T_EOF) { if (peek().t == T_LC) d++; if (peek().t == T_RC) d--; adv(); }
}

void iff(void) {
    Value c = expr(); int hit = truthy(c);
    if (hit) blk(); else skip_blk();
    while (peek().t == T_ELIF && !rf) {
        adv();
        if (!hit) { c = expr(); if (truthy(c)) { hit = 1; blk(); } else skip_blk(); }
        else skip_blk();
    }
    if (peek().t == T_ELSE && !rf) { adv(); if (hit) skip_blk(); else blk(); }
}

void wh(void) {
    int cond = tp; expr();
    while (peek().t == T_NL) adv();
    int bs = tp; xpct(T_LC); int d = 1;
    while (d > 0) { if (peek().t == T_LC) d++; if (peek().t == T_RC) d--;
        if (peek().t == T_EOF) die("unterminated while"); tp++; }
    int be = tp; tp = cond;
    while (truthy(expr())) {
        tp = bs; rf = 0;
        while (tp < be && !rf) stmt(); if (rf) break; tp = cond;
    }
    tp = be;
}

void fn_def(void) {
    Tok t = adv(); if (t.t != T_ID) die("expected function name");
    xpct(T_LP); char *ps[64]; int pa = 0;
    if (peek().t != T_RP) do { Tok p = adv(); if (p.t != T_ID) die("expected param");
        ps[pa++] = strdup(p.s); } while (mtch(T_CM));
    xpct(T_RP); int bs = tp, depth = 1;
    while (peek().t == T_NL) adv(); xpct(T_LC);
    while (depth > 0) {
        if (peek().t == T_EOF) die("unterminated function body");
        if (peek().t == T_LC) depth++; if (peek().t == T_RC) depth--; adv();
    }
    int be = tp;
    if (ffind(t.s) >= 0) die("'%s' already defined", t.s);
    if (fc >= fm) { fm = fm ? fm*2 : 8; fs = realloc(fs, fm*sizeof(Fn)); }
    Fn *f = &fs[fc++]; f->n = strdup(t.s); f->p = malloc(pa*sizeof(char*));
    for (int i = 0; i < pa; i++) f->p[i] = ps[i]; f->a = pa; f->bs = bs; f->be = be;
}

void stmt(void) {
    if (peek().t == T_NL) { adv(); return; }  /* skip bare newlines */
    switch (peek().t) {
        case T_IF: adv(); iff(); break;
        case T_WH: adv(); wh(); break;
        case T_FN: adv(); fn_def(); break;
        case T_RT: {
            adv(); int sv = tp;
            int is_tc = (cur_fi >= 0 && peek().t == T_ID &&
                         !strcmp(ts[tp].s, fs[cur_fi].n) && ts[tp+1].t == T_LP);
            if (is_tc) {
                tco = 1; rv = expr();
                if (tco) { /* TCO didn't trigger, normal return */
                    tp = sv; rv = expr(); rf = 1;
                } /* else: TCO rebind happened, rf=0, loop continues */
            } else {
                tp = sv; rv = expr(); rf = 1;
            }
            break;
        }
        case T_LC: blk(); break;
        case T_ID: {
            int idp = tp; char *nm = ts[tp].s; tp++;
            if (peek().t == T_EQ) {
                tp = idp; tp++; xpct(T_EQ); Value v = expr(); sset(cs, nm, v);
            } else if (peek().t == T_LB) {
                int si = -1;
                for (int i = 0; i < cs->c; i++) if (!strcmp(cs->n[i], nm)) { si = i; break; }
                if (si < 0) die("undefined '%s'", nm);
                Value *slot = &cs->v[si];
                while (peek().t == T_LB) {
                    amake_uniq(slot); /* COW before mutation */
                    adv(); Value is[64]; int n = 0;
                    do is[n++] = expr(); while (mtch(T_CM));
                    xpct(T_RB);
                    for (int i = 0; i < n; i++) {
                        if (is[i].type == VAL_NUM) {
                            int ii = (int)is[i].as.num;
                            if (slot->type != VAL_ARR || !slot->as.arr || ii < 0 || ii >= slot->as.arr->len)
                                die("index out of bounds");
                            slot = &slot->as.arr->items[ii];
                        } else if (is[i].type == VAL_ARR) {
                            for (int j = 0; j < is[i].as.arr->len; j++) {
                                int ii = (int)is[i].as.arr->items[j].as.num;
                                if (slot->type != VAL_ARR || !slot->as.arr || ii < 0 || ii >= slot->as.arr->len)
                                    die("index out of bounds");
                                slot = &slot->as.arr->items[ii];
                            }
                        } else die("index must be number or array");
                    }
                }
                if (peek().t == T_EQ) { adv(); Value v = expr(); vassign(slot, v); }
            } else { tp = idp; expr(); }
            break;
        }
        default: expr(); break;
    }
}

/* ========================== MAIN ========================== */
char *readf(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *b = malloc(sz + 1); fread(b, 1, sz, f); b[sz] = 0;
    fclose(f); return b;
}

void run(const char *src) {
    if (ts) { free(ts); ts = NULL; }
    lex(src);
    while (peek().t != T_EOF) {
        if (peek().t == T_NL) { adv(); continue; }
        stmt();
    }
}

int main(int a, char **v) {
    cs = snew(); cur_fi = -1;
    if (a >= 2) {
        char *src = readf(v[1]);
        if (!src) { fprintf(stderr, "cannot read '%s'\n", v[1]); return 1; }
        run(src); free(src);
    } else {
        printf("TinyLang v0.1\n");
        char line[4096];
        while (1) {
            printf("> "); fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) { printf("\n"); break; }
            run(line);
        }
    }
    sfree(cs); if (ts) free(ts); return 0;
}
