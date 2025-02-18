#include <stdbool.h>
#include <stdlib.h>

#define KHOR_VERSION 0
/* ZZZ: ! */
#define KHOR_IMPLEMENTATION
/* ZZZ: ! */

#define dyarr(__elty) struct { __elty* ptr; size_t len, cap; }
#define slarr(__elty) struct { __elty* ptr; size_t len; }

typedef slarr(char) khor_slice;

/* typedef */
typedef union khor_object khor_object;

typedef struct khor_number { unsigned ty :8, pad :24; double val; } khor_number;
typedef struct khor_string { unsigned ty :8, len :24; char* ptr; } khor_string;
typedef struct khor_list   { unsigned ty :8, len :24; khor_object* ptr; } khor_list;
typedef struct khor_lambda { unsigned ty :8, ary :24; void* env; } khor_lambda;
typedef struct khor_symbol { char txt[16]; } khor_symbol;

enum khor_type { KHOR_NUMBER, KHOR_STRING, KHOR_LIST, KHOR_LAMBDA, KHOR_SYMBOL=' ' };
union khor_object {
    unsigned ty :8;
    khor_number num;
    khor_string str;
    khor_list lst;
    khor_lambda lbd;
    khor_symbol sym;
};

typedef dyarr(char) khor_bytecode;
enum khor_op {
    KHOR_OP_MKNUM,     /*   0 -- 1 (+next :8)           */
    KHOR_OP_MKSTR,     /*   0 -- 1 (+next n:3, +next n) */
    KHOR_OP_MKLST,     /*   n -- 1 (+next n:3)          */
    /*KHOR_OP_MKLBD,     *   0 -- 1                      */
    KHOR_OP_MKSYM,     /*   0 -- 1                      */
    KHOR_OP_MKNIL,     /*   0 -- 1                      */
    KHOR_OP_HALT,      /*   0 -- 0 (+next :1)           */
    KHOR_OP_DISCARD,   /*   1 -- 0                      */
    KHOR_OP_RESOLVE,   /*   1 -- 1                      */
    KHOR_OP_DEFINE,    /*   2 -- 1                      */
    KHOR_OP_APPLY,     /* 1+n -- 1 (+next n:1)          */
    KHOR_OP_JUMP,      /*   0 -- 0 (+next :2)           */
    KHOR_OP_JUMPIF,    /*   1 -- 0 (+next :2)           */
    KHOR_OP_ADD,       /*   2 -- 1                      */
    KHOR_OP_SUB,       /*   2 -- 1                      */
    KHOR_OP_MUL,       /*   2 -- 1                      */
    KHOR_OP_DIV,       /*   2 -- 1                      */
    KHOR_OP_LT,        /*   2 -- 1                      */
    KHOR_OP_GT,        /*   2 -- 1                      */
    KHOR_OP_LE,        /*   2 -- 1                      */
    KHOR_OP_GE,        /*   2 -- 1                      */
    KHOR_OP_EQ,        /*   2 -- 1                      */
    KHOR_OP_AND,       /*   2 -- 1                      */
    KHOR_OP_OR,        /*   2 -- 1                      */
    KHOR_OP_CATS,      /*   2 -- 1                      */
    KHOR_OP_FF = 255
};
#define KHOR_COUNT_OPS 24

typedef struct khor_rule {
    khor_object subst;
    slarr(khor_symbol) names;
} khor_rule;
typedef struct khor_rules {
    slarr(khor_rule) rules;
    khor_symbol key;
} khor_rules;
typedef dyarr(khor_rules) khor_ruleset;

typedef struct khor_enventry {
    khor_object value;
    khor_symbol key;
} khor_enventry;
typedef struct khor_environment {
    struct khor_environment const* parent;
    dyarr(khor_enventry) entries;
} khor_environment;
typedef dyarr(khor_object) khor_stack;

/* should only return `true` if continuing execution is ok */
typedef bool (* khor_handler)(khor_bytecode* code, khor_environment* env, khor_stack* stack, unsigned* cp, unsigned* sp, char w);

khor_slice   khor_lex      (khor_slice* s);
khor_object  khor_parse    (khor_slice* s, khor_ruleset* macros);
khor_object  khor_trysubst (khor_list const* node, khor_rules const* rules, unsigned* matched);
void         khor_compile  (khor_object const* node, khor_bytecode* res);
void         khor_eval     (khor_bytecode* code, khor_environment* env, khor_stack* stack, khor_handler handle);
khor_object* khor_lookup   (khor_environment const* env, khor_symbol const* key);
khor_object  khor_duplicate(khor_object const* self);
void         khor_destroy  (khor_object* self);

#ifdef KHOR_IMPLEMENTATION

/* ZZZ: ! */
void dump(khor_object const* self);
void dumpcode(khor_bytecode const* code);
void dumpenv(khor_environment const* env);
void dumpmacros(khor_ruleset const* macros);
#include <stdio.h>
/* ZZZ: ! */

#include <string.h>

/* clears it empty and frees used memory */
#define dyarr_clear(__da)  ((__da)->len = (__da)->cap = 0, free((__da)->ptr), (__da)->ptr = NULL)
/* resizes exactly to given, new size should not be 0 */
#define dyarr_resize(__da, __rsz)  _dyarr_resize(&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->cap, (__rsz))
/* doubles the capacity if more memory is needed */
#define dyarr_push(__da)  ((__da)->len < (__da)->cap || dyarr_resize((__da), (__da)->cap ? (__da)->cap * 2 : 16) ? &(__da)->ptr[(__da)->len++] : NULL)
/* NULL if empty */
#define dyarr_pop(__da)  ((__da)->len ? &(__da)->ptr[--(__da)->len] : NULL)
/* NULL (or rather 0) if OOM, else pointer to the new sub-array (at k, of size n) */
#define dyarr_insert(__da, __k, __n)  ((__da)->ptr + _dyarr_insert(&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->cap, &(__da)->len, (__k), (__n)))
/* doesn't check bounds */
#define dyarr_remove(__da, __k, __n)  ((__da)->ptr + _dyarr_remove(&(__da)->ptr, sizeof*(__da)->ptr, &(__da)->len, (__k), (__n)))
/* ptr to begin or NULL */
#define slarr_alloc(__sl, __len)  (((__sl)->ptr = calloc((__sl)->len = (__len), sizeof*(__sl)->ptr)) ? (__sl)->ptr : NULL)
/* sets len empty and frees used memory */
#define slarr_free(__sl)  ((__sl)->len = 0, free((__sl)->ptr), (__sl)->ptr = NULL)

static bool _dyarr_resize(void* ptr, size_t isz, size_t* cap, size_t rsz) {
    void* niw = realloc(*(void**)ptr, rsz * isz);
    return niw ? *(void**)ptr = niw, *cap = rsz, true : false;
}
static size_t _dyarr_insert(void* ptr, size_t isz, size_t* cap, size_t* len, size_t k, size_t n) {
    return *len+n < *cap || _dyarr_resize(ptr, isz, cap, *len+n) ? memmove(*(char**)ptr+(k+n)*isz, *(char**)ptr+k*isz, (*len-k)*isz), *len+= n, k : -(size_t)*(char**)ptr;
}
/*static size_t _dyarr_remove(void* ptr, size_t isz, size_t* len, size_t k, size_t n) {
    return memmove(*(char**)ptr+k*isz, *(char**)ptr+(k+n)*isz, ((*len-= n)-k)*isz), k;
}*/

khor_slice khor_lex(khor_slice* s) {
    khor_slice r;
#   define next() (--s->len, ++s->ptr)
    while (s->len && strchr(" \t\r\n", *s->ptr)) next();
    if (';' == *s->ptr) {
        while (s->len && '\n' != *next());
        return khor_lex(s);
    }
    r.ptr = s->ptr;
    if (!s->len) r.ptr = NULL;
    else if ('"' == *s->ptr) {
        do if ('\\' == *next()) (next(), next());
        while (s->len && '"' != *s->ptr);
        if (s->len) next();
    } else if ('(' == *s->ptr || ')' == *s->ptr) next();
    else while (s->len && !strchr(" \t\r\n\"();", *s->ptr)) next();
    r.len = s->ptr - r.ptr;
#   undef next
    return r;
}

khor_object khor_parse(khor_slice* s, khor_ruleset* macros) {
    unsigned k;
    khor_object r = {0};
    khor_slice token = khor_lex(s);
    if (!token.ptr) return r;
    if (strchr("0123456789", *token.ptr) || ('-' == *token.ptr && strchr("0123456789", token.ptr[1]))) {
        r.ty = KHOR_NUMBER;
        r.num.val = 0;
        for (k = '-' == *token.ptr; k < token.len; k++) r.num.val = r.num.val*10 + token.ptr[k]-'0';
        if ('-' == *token.ptr) r.num.val*= -1;
    } else if ('"' == *token.ptr) {
        dyarr(char) arr = {0};
        dyarr_resize(&arr, token.len-2);
        for (k = 1; k < token.len-1; k++) {
            char* at = dyarr_push(&arr);
            if ('\\' == token.ptr[k]) switch (token.ptr[++k]) {
                case 'a': *at =  7; break;
                case 'b': *at =  8; break;
                case 'e': *at = 27; break;
                case 'f': *at = 12; break;
                case 'n': *at = 10; break;
                case 'r': *at = 13; break;
                case 't': *at =  9; break;
                case 'v': *at = 11; break;
                default: *at = token.ptr[k];
            } else *at = token.ptr[k];
        }
        r.ty = KHOR_STRING;
        r.str.len = arr.len;
        r.str.ptr = arr.ptr;
    } else if ('(' == *token.ptr) {
        dyarr(khor_object) arr = {0};
        while (s->len) {
            khor_slice p = *s;
            token = khor_lex(s);
            if (!token.ptr || ')' == *token.ptr) break;
            *s = p;
            *dyarr_push(&arr) = khor_parse(s, macros);
        }
        r.ty = KHOR_LIST;
        r.lst.len = arr.len;
        r.lst.ptr = arr.ptr;
        if (0 < arr.len && KHOR_SYMBOL < arr.ptr[0].ty) {
            if (!strcmp("rules", arr.ptr[0].sym.txt)) {
                unsigned j;
                khor_rules* niw;
                if (arr.len <3 || arr.len & 1 || arr.ptr[1].ty < KHOR_SYMBOL) goto rules_fail;
                for (k = 2; k < arr.len; k+= 2) {
                    bool last_had_q = false;
                    if (KHOR_LIST != arr.ptr[k].ty) goto rules_fail;
                    for (j = 0; j < arr.ptr[k].lst.len; j++) {
                        khor_object const* it = arr.ptr[k].lst.ptr+j;
                        if (it->ty < KHOR_SYMBOL || (last_had_q && '$' == *it->sym.txt)) goto rules_fail;
                        last_had_q = '$' == *it->sym.txt && strchr("?+*", it->sym.txt[strlen(it->sym.txt)-1]);
                    }
                }
                niw = dyarr_push(macros);
                niw->key = arr.ptr[1].sym;
                slarr_alloc(&niw->rules, arr.len/2-1);
                for (k = 2; k < arr.len; k+= 2) {
                    khor_rule* it = niw->rules.ptr+k/2-1;
                    slarr_alloc(&it->names, arr.ptr[k].lst.len);
                    for (j = 0; j < arr.ptr[k].lst.len; j++)
                        it->names.ptr[j] = arr.ptr[k].lst.ptr[j].sym;
                    it->subst = khor_duplicate(arr.ptr+k+1);
                }
            rules_fail:
                slarr_free(&r.lst);
            } else for (k = 0; k < macros->len; k++) {
                unsigned matched;
                khor_object niw = khor_trysubst(&r.lst, macros->ptr+k, &matched);
                if (matched < macros->ptr[k].rules.len) {
                    khor_destroy(&r);
                    r = niw;
                    break;
                }
                /* TODO: again */
            }
        }
    } else memcpy(r.sym.txt, token.ptr, token.len < 15 ? token.len : 15);
    return r;
}

void _khor_rsubst(khor_list* self, khor_symbol const* name, khor_object** ptr, size_t len) {
    unsigned k;
    dyarr(khor_object) arr = {0};
    arr.ptr = self->ptr;
    arr.len = arr.cap = self->len;
    for (k = 0; k < arr.len; k++) {
        if (KHOR_LIST == arr.ptr[k].ty) _khor_rsubst(&arr.ptr[k].lst, name, ptr, len);
        else if (KHOR_SYMBOL < arr.ptr[k].ty && !strcmp(name->txt, arr.ptr[k].sym.txt)) {
            khor_object* at = dyarr_insert(&arr, k+1, len-1)-1;
            unsigned l;
            for (l = 0; l < len; l++) at[l] = khor_duplicate(ptr[l]);
            k+= len-1;
        }
    }
    self->len = arr.len;
    self->ptr = arr.ptr;
}

khor_object khor_trysubst(khor_list const* list, khor_rules const* rules, unsigned* matched) {
    unsigned k, s;
    dyarr(khor_object*)* map;
    khor_object r;
    r.ty = KHOR_LIST;
    r.lst.len = 0;
    r.lst.ptr = NULL;
    if (!list->len || strcmp(rules->key.txt, list->ptr->sym.txt)) {
        *matched = rules->rules.len;
        return r;
    }
    for (k = 0, s = 0; k < rules->rules.len; ++k)
        if (s < rules->rules.ptr[k].names.len) s = rules->rules.ptr[k].names.len;
    map = calloc(s, sizeof(dyarr(khor_object*)));
    for (*matched = 0; *matched < rules->rules.len; ++*matched) {
        khor_rule const* it = rules->rules.ptr+*matched;
        for (k = 1, s = 0; k < list->len; k++) {
            char* cpat, * npat;
            bool issym;
            if (it->names.len == s) {
                s++;
                break;
            }
            cpat = it->names.ptr[s].txt;
            issym = KHOR_SYMBOL < list->ptr[k].ty;
            if ('$' != *cpat) {
                if (!issym || strcmp(list->ptr[k].sym.txt, cpat)) break;
                s++;
                continue;
            }
            npat = it->names.len == s+1 ? NULL : it->names.ptr[s+1].txt;
            switch (cpat[strlen(cpat)-1]) {
                case '?':
                    if (!npat) {
                        if (list->len-k < 2) {
                            if (list->len == k+1) *dyarr_push(map+s) = list->ptr+k;
                            s++;
                        } else k = list->len;
                    } else if (issym && !strcmp(list->ptr[k].sym.txt, npat)) s+= 2;
                    else *dyarr_push(map+s) = list->ptr+k;
                    continue;
                case '+':
                case '*':
                    if (!npat) for (s++; k < list->len; k++) *dyarr_push(map+s-1) = list->ptr+k;
                    else if (issym && !strcmp(list->ptr[k].sym.txt, npat)) s+= 2;
                    else *dyarr_push(map+s) = list->ptr+k;
                    continue;
                default:
                    *dyarr_push(map+s) = list->ptr+k;
                    s++;
                    continue;
            }
        }
        if (it->names.len == s+1 && '$' == *it->names.ptr[s].txt) {
            char ch = it->names.ptr[s].txt[strlen(it->names.ptr[s].txt)-1];
            if ('?' == ch || '*' == ch) s++;
        }
        if (it->names.len == s) {
            r = khor_duplicate(&it->subst);
            while (s) if ('$' == *it->names.ptr[--s].txt) {
                switch (r.ty) {
                    case KHOR_NUMBER:
                    case KHOR_STRING:
                        break;
                    case KHOR_LIST:
                        _khor_rsubst(&r.lst, it->names.ptr+s, map[s].ptr, map[s].len);
                        break;
                    default:
                        if (map[s].len && !strcmp(it->names.ptr[s].txt, r.sym.txt))
                            r = khor_duplicate(*map[s].ptr);
                        break;
                }
                dyarr_clear(map+s);
            }
            break;
        }
        while (s) if ('$' == *it->names.ptr[--s].txt) dyarr_clear(map+s);
    }
    free(map);
    return r;
}

void khor_compile(khor_object const* node, khor_bytecode* res) {
    unsigned k;
    switch (node->ty) {
        case KHOR_NUMBER:
            *dyarr_push(res) = KHOR_OP_MKNUM;
            /* XXX: endian whever */
            memcpy(dyarr_insert(res, res->len, 8), (char*)&node->num.val, 8);
            break;
        case KHOR_STRING:
            *dyarr_push(res) = KHOR_OP_MKSTR;
            *dyarr_push(res) = (node->str.len>>16) & 255;
            *dyarr_push(res) = (node->str.len>>8) & 255;
            *dyarr_push(res) = node->str.len & 255;
            memcpy(dyarr_insert(res, res->len, node->str.len), node->str.ptr, node->str.len);
            break;
        case KHOR_LIST:
            if (0 == node->lst.len) *dyarr_push(res) = KHOR_OP_MKNIL;
            else {
                unsigned argc = node->lst.len;
                khor_object* argv = node->lst.ptr;
                if (KHOR_NUMBER == argv->ty || KHOR_STRING == argv->ty) {
                    *dyarr_push(res) = KHOR_OP_MKNIL;
                    break;
                }
                if (KHOR_LIST != argv->ty) {
#                   define symeq(__c) (!strcmp(__c, argv->sym.txt))
                    if (symeq("halt")) {
                        *dyarr_push(res) = KHOR_OP_HALT;
                        *dyarr_push(res) = 0;
                        if (2 == argc) khor_compile(argv+1, res);
                        else if (3 == argc && KHOR_NUMBER == argv[1].ty) {
                            res->ptr[res->len-1] = argv[1].num.val;
                            khor_compile(argv+2, res);
                        } else *dyarr_push(res) = KHOR_OP_MKNIL;
                        break;
                    } else if (symeq("progn")) {
                        if (1 == argc) *dyarr_push(res) = KHOR_OP_MKNIL;
                        else {
                            khor_compile(argv+1, res);
                            for (k = 2; k < argc; k++) {
                                *dyarr_push(res) = KHOR_OP_DISCARD;
                                khor_compile(argv+k, res);
                            }
                        }
                        break;
                    } else if (symeq("list")) {
                        for (k = 1; k < (argc & 65535); k++) khor_compile(argv+k, res);
                        *dyarr_push(res) = KHOR_OP_MKLST;
                        *dyarr_push(res) = ((argc-1)>>16) & 255;
                        *dyarr_push(res) = ((argc-1)>>8) & 255;
                        *dyarr_push(res) = (argc-1) & 255;
                        break;
                    } else if (symeq("define")) {
                        if (3 != argc || argv[1].ty < KHOR_SYMBOL) {
                            *dyarr_push(res) = KHOR_OP_MKNIL;
                            break;
                        }
                        khor_compile(argv+2, res);
                        *dyarr_push(res) = KHOR_OP_MKSYM;
                        k = strlen(argv[1].sym.txt);
                        memcpy(dyarr_insert(res, res->len, k), argv[1].sym.txt, k);
                        *dyarr_push(res) = KHOR_OP_DEFINE;
                        break;
                    } else if (symeq("lambda")) {
                        if (3 != argc || KHOR_LIST != argv[1].ty) {
                        lambda_fail:
                            *dyarr_push(res) = KHOR_OP_MKNIL;
                            break;
                        }
                        for (k = 0; k < argv[1].lst.len; k++)
                            if (argv[1].lst.ptr[k].ty < KHOR_SYMBOL) goto lambda_fail;
                        puts("NIY: (lambda (params) body)");
                        *dyarr_push(res) = KHOR_OP_MKNIL;
                        break;
                    } else if (symeq("if")) {
                        unsigned plen;
                        char* hi, * lo;
                        int joff;
                        if (4 != argc) {
                            *dyarr_push(res) = KHOR_OP_MKNIL;
                            break;
                        }
                        khor_compile(argv+1, res);
                        *dyarr_push(res) = KHOR_OP_JUMPIF;
                        plen = res->len;
                        hi = dyarr_push(res);
                        lo = dyarr_push(res);
                        khor_compile(argv+3, res);
                        joff = res->len-plen+1;
                        *hi = (joff>>8) & 255;
                        *lo = joff & 255;
                        *dyarr_push(res) = KHOR_OP_JUMP;
                        hi = dyarr_push(res);
                        lo = dyarr_push(res);
                        plen = res->len;
                        khor_compile(argv+2, res);
                        joff = res->len-plen;
                        *hi = (joff>>8) & 255;
                        *lo = joff & 255;
                        break;
                    } else if (symeq("<") || symeq(">") || symeq("<=") || symeq(">=") || symeq("=")) {
                        char const op = '<' == *argv->sym.txt ? ('=' == argv->sym.txt[1] ? KHOR_OP_LE : KHOR_OP_LT)
                                      : '>' == *argv->sym.txt ? ('=' == argv->sym.txt[1] ? KHOR_OP_GE : KHOR_OP_GT)
                                      : '=' == *argv->sym.txt ? KHOR_OP_EQ
                                      : 0;
                        if (argc <3) *dyarr_push(res) = KHOR_OP_MKNIL;
                        else {
                            khor_compile(argv+1, res);
                            khor_compile(argv+2, res);
                            *dyarr_push(res) = op;
                        }
                        break;
                    } else if (symeq("+") || symeq("-") || symeq("*") || symeq("/") || symeq("and") || symeq("or") || symeq("..")) {
                        char const op = '+' == *argv->sym.txt ? KHOR_OP_ADD
                                      : '-' == *argv->sym.txt ? KHOR_OP_SUB
                                      : '*' == *argv->sym.txt ? KHOR_OP_MUL
                                      : '/' == *argv->sym.txt ? KHOR_OP_DIV
                                      : 'a' == *argv->sym.txt ? KHOR_OP_AND
                                      : 'o' == *argv->sym.txt ? KHOR_OP_OR
                                      : '.' == *argv->sym.txt ? KHOR_OP_CATS
                                      : 0;
                        if (1 == argc) *dyarr_push(res) = KHOR_OP_MKNIL;
                        else {
                            khor_compile(argv+1, res);
                            for (k = 2; k < argc; k++) {
                                khor_compile(argv+k, res);
                                *dyarr_push(res) = op;
                            }
                        }
                        break;
                    }
#                   undef symeq
                }
                /* TODO: rethinking order, need a slot for the cp for OP_RETURN */
                for (k = 0; k < argc; k++) khor_compile(argv+k, res);
                *dyarr_push(res) = KHOR_OP_APPLY;
                *dyarr_push(res) = (argc-1) & 255;
            }
            break;
        default:
            *dyarr_push(res) = KHOR_OP_MKSYM;
            k = strlen(node->sym.txt);
            memcpy(dyarr_insert(res, res->len, k), node->sym.txt, k);
            *dyarr_push(res) = KHOR_OP_RESOLVE;
            break;
    }
}

void khor_eval(khor_bytecode* code, khor_environment* env, khor_stack* stack, khor_handler handle) {
    unsigned cp, sp, k;
#   define isnil(__at) (KHOR_LIST == (__at)->ty && !(__at)->lst.ptr)
#   define mknil(__at) ((__at)->ty = KHOR_LIST, (__at)->lst.len = 0, (__at)->lst.ptr = NULL)
    for (cp = 0, sp = 0; cp < code->len && (sp < stack->len || handle(code, env, stack, &cp, &sp, -1)); cp++) switch (code->ptr[cp]) {
        case KHOR_OP_MKNUM:
            stack->ptr[sp].ty = KHOR_NUMBER;
            memcpy(&stack->ptr[sp].num.val, code->ptr+cp+1, 8);
            cp+= 8;
            sp++;
            break;
        case KHOR_OP_MKSTR:
            stack->ptr[sp].ty = KHOR_STRING;
            stack->ptr[sp].str.len = code->ptr[cp+1]<<16 | code->ptr[cp+2]<<8 | code->ptr[cp+3];
            stack->ptr[sp].str.ptr = memcpy(malloc(stack->ptr[sp].str.len), code->ptr+cp+4, stack->ptr[sp].str.len);
            cp+= 3+stack->ptr[sp].str.len;
            sp++;
            break;
        case KHOR_OP_MKLST: {
                unsigned len = code->ptr[cp+1]<<16 | code->ptr[cp+2]<<8 | code->ptr[cp+3];
                khor_object it;
                it.ty = KHOR_LIST;
                it.lst.len = len;
                it.lst.ptr = calloc(len, sizeof(khor_object));
                for (k = 0; k < len; k++) it.lst.ptr[len-1-k] = stack->ptr[--sp];
                cp+= 3;
                stack->ptr[sp++] = it;
            }
            break;
        /*case KHOR_OP_MKLBD:
            puts("NIY: KHOR_OP_MKLBD");
            break;*/
        case KHOR_OP_MKSYM:
            for (k = 0; KHOR_SYMBOL < code->ptr[cp+1]; k++, cp++) stack->ptr[sp].sym.txt[k] = code->ptr[cp+1];
            stack->ptr[sp].sym.txt[k] = '\0';
            sp++;
            break;
        case KHOR_OP_MKNIL:
            mknil(stack->ptr+sp);
            sp++;
            break;
        case KHOR_OP_HALT:
            if (!handle(code, env, stack, &cp, &sp, code->ptr[cp+1])) return;
            cp++;
            break;
        case KHOR_OP_DISCARD:
            khor_destroy(stack->ptr+--sp);
            break;
        case KHOR_OP_RESOLVE:
            if (KHOR_SYMBOL < stack->ptr[sp-1].ty) {
                khor_object* found = khor_lookup(env, &stack->ptr[sp-1].sym);
                khor_destroy(stack->ptr+sp-1);
                if (found) {
                    stack->ptr[sp-1] = khor_duplicate(found);
                    break;
                }
            } else khor_destroy(stack->ptr+sp-1);
            stack->ptr[sp-1].ty = KHOR_LIST;
            stack->ptr[sp-1].lst.len = 0;
            stack->ptr[sp-1].lst.ptr = NULL;
            break;
        case KHOR_OP_DEFINE:
            if (KHOR_SYMBOL < stack->ptr[--sp].ty) {
                khor_enventry* niw = dyarr_push(&env->entries);
                niw->key = stack->ptr[sp].sym;
                niw->value = khor_duplicate(stack->ptr+sp-1);
            } else khor_destroy(stack->ptr+sp+1);
            break;
        case KHOR_OP_APPLY: {
                int argc = code->ptr[++cp];
                khor_object* argv = stack->ptr+sp-argc;
                khor_object* retaddr = stack->ptr+sp-(argc+1);
                printf("NIY: KHOR_OP_APPLY (%d args)\n", argc);
                (void)argc;
                (void)argv;
                (void)retaddr;
                sp-= argc;
            }
            break;
        case KHOR_OP_JUMP:
            cp+= code->ptr[cp+1]<<8 | code->ptr[cp+2];
            cp+= 2;
            break;
        case KHOR_OP_JUMPIF:
            if (!isnil(stack->ptr+sp-1)) cp+= code->ptr[cp+1]<<8 | code->ptr[cp+2];
            sp--;
            khor_destroy(stack->ptr+sp);
            cp+= 2;
            break;
#       define numbin(__op) do {                                                                        \
                if (KHOR_NUMBER == stack->ptr[sp-2].ty && KHOR_NUMBER == stack->ptr[sp-1].ty)           \
                    stack->ptr[sp-2].num.val = stack->ptr[sp-2].num.val __op stack->ptr[sp-1].num.val;  \
                else {                                                                                  \
                    khor_destroy(stack->ptr+sp-2);                                                      \
                    khor_destroy(stack->ptr+sp-1);                                                      \
                    stack->ptr[sp-2].ty = KHOR_NUMBER;                                                  \
                    stack->ptr[sp-2].num.val = 0;                                                       \
                }                                                                                       \
                sp--;                                                                                   \
            } while (0)
        case KHOR_OP_ADD: numbin(+); break;
        case KHOR_OP_SUB: numbin(-); break;
        case KHOR_OP_MUL: numbin(*); break;
        case KHOR_OP_DIV: numbin(/); break;
        case KHOR_OP_LT: numbin(<); break;
        case KHOR_OP_GT: numbin(>); break;
        case KHOR_OP_LE: numbin(<=); break;
        case KHOR_OP_GE: numbin(>=); break;
        case KHOR_OP_EQ: numbin(==); break;
#       undef numbin
        case KHOR_OP_AND:
            if (isnil(stack->ptr+sp-2) || isnil(stack->ptr+sp-1)) {
                khor_destroy(stack->ptr+sp-2);
                khor_destroy(stack->ptr+sp-1);
                mknil(stack->ptr+sp-2);
            } else {
                khor_destroy(stack->ptr+sp-2);
                stack->ptr[sp-2] = stack->ptr[sp-1];
            }
            sp--;
            break;
        case KHOR_OP_OR:
            if (isnil(stack->ptr+sp-2)) stack->ptr[sp-2] = stack->ptr[sp-1];
            else khor_destroy(stack->ptr+sp-1);
            sp--;
            break;
        case KHOR_OP_CATS:
            if (KHOR_STRING == stack->ptr[sp-2].ty && KHOR_STRING == stack->ptr[sp-1].ty) {
                unsigned len1 = stack->ptr[sp-2].str.len;
                unsigned len2 = stack->ptr[sp-1].str.len;
                khor_object niw;
                niw.ty = KHOR_STRING;
                niw.str.len = len2 + len1;
                niw.str.ptr = memcpy(malloc(len1), stack->ptr[sp-2].str.ptr, len1);
                memcpy(niw.str.ptr+len1, stack->ptr[sp-1].str.ptr, len2);
                khor_destroy(stack->ptr+sp-2);
                stack->ptr[sp-2] = niw;
            } else {
                khor_destroy(stack->ptr+sp-2);
                stack->ptr[sp-2].ty = KHOR_STRING;
                stack->ptr[sp-2].str.len = 0;
                stack->ptr[sp-2].str.ptr = NULL;
            }
            khor_destroy(stack->ptr+sp-1);
            sp--;
            break;
    }
#   undef isnil
#   undef mknil
#   if 24 != KHOR_COUNT_OPS
#   error "missing op in khor_eval!"
#   endif
}

khor_object* khor_lookup(khor_environment const* env, khor_symbol const* key) {
    unsigned k;
    for (k = 0; k < env->entries.len; k++)
        if (!strcmp(key->txt, env->entries.ptr[k].key.txt))
            return &env->entries.ptr[k].value;
    return env->parent ? khor_lookup(env->parent, key) : NULL;
}

khor_object khor_duplicate(khor_object const* self) {
    unsigned k;
    khor_object r = *self;
    switch (r.ty) {
        case KHOR_STRING:
            r.str.ptr = memcpy(malloc(r.str.len), self->str.ptr, r.str.len);
            break;
        case KHOR_LIST:
            r.lst.ptr = calloc(r.lst.len, sizeof(khor_object));
            for (k = 0; k < r.lst.len; k++) r.lst.ptr[k] = khor_duplicate(self->lst.ptr+k);
            break;
        case KHOR_LAMBDA:
            /* TODO: !! */
            break;
    }
    return r;
}

void khor_destroy(khor_object* self) {
    unsigned k;
    switch (self->ty) {
        case KHOR_STRING:
            free(self->str.ptr);
            break;
        case KHOR_LIST:
            for (k = 0; k < self->lst.len; k++) khor_destroy(self->lst.ptr+k);
            free(self->lst.ptr);
            break;
        case KHOR_LAMBDA:
            /* TODO: !! */
            break;
    }
}

#endif /* KHOR_IMPLEMENTATION */
