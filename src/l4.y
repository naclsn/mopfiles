%{
#include <stdbool.h>

#ifndef arry
  #define arry(...) struct { __VA_ARGS__* ptr; size_t len, cap; }
  #define frry(__a) ((__a)->cap ? free((__a)->ptr) : (void)0, (__a)->ptr = NULL, (__a)->len = (__a)->cap = 0)
  #define push(__a) ((__a)->len < (__a)->cap || (((__a)->cap || !(__a)->ptr) && ((__a)->ptr = realloc((__a)->ptr, ((__a)->cap+= (__a)->cap+8)*sizeof*(__a)->ptr))) ? (__a)->ptr+(__a)->len++ : NULL)
  #define grow(__a, __k, __n) _grow((char**)&(__a)->ptr, &(__a)->len, &(__a)->cap, sizeof*(__a)->ptr, (__k), (__n))
  #define last(__a) ((__a)->ptr+(__a)->len-1)
  #define each(__a) for (void* const _ptr = (__a)->ptr,* const _end = (__a)->ptr+(__a)->len; _end == (__a)->ptr ? (__a)->ptr = _ptr, 0 : 1; ++(__a)->ptr)
  static inline void* _grow(char** const ptr, size_t* const len, size_t* const cap, size_t const s, size_t const k, size_t const n) {
      size_t const nlen = *len+n;
      if (*cap < nlen && !(cap && (*ptr = realloc(*ptr, (*cap = nlen)*s)))) return NULL;
      if (k < *len) memmove(*ptr+(k+n)*s, *ptr+k*s, (*len-k)*s);
      return *len = nlen, *ptr+k*s;
  }
#endif

//#define strdup(__s) strcpy(malloc(strlen((__s))), (__s))
#define cass if (0) case
#define sz size_t

#define colo(__c, __l) "\x1b[" #__c "m" #__l "\x1b[m"

typedef arry(struct decl) arry_decls;

//struct struc {
//    char* name;
//    arry_decls decls;
//};

struct type {
    union {
        char* tag;
        struct { struct type* to; } ptr;
        struct {
            struct type* of;
            unsigned size;
        } arr;
        struct {
            struct type* ret;
            arry_decls params;
        } fun;
    };
    enum { TY_TAG, TY_PTR, TY_ARR, TY_FUN } kind;
};
static char const* const tostr_type_kind[] = { "TY_TAG", "TY_PTR", "TY_ARR", "TY_FUN" };
void free_type(struct type type);
void dump_type(struct type type);

struct decl {
    char* name;
    struct type type;
};
void free_decl(struct decl decl);
void dump_decl(struct decl decl);

void free_type(struct type type) {
    switch (type.kind) {
        case TY_TAG: free(type.tag);
        cass TY_PTR: free_type(*type.ptr.to);
        cass TY_ARR: free_type(*type.arr.of);
        cass TY_FUN: {
            free_type(*type.fun.ret);
            each (&type.fun.params) free_decl(*type.fun.params.ptr);
            frry(&type.fun.params);
        }
    }
}
void dump_type(struct type type) {
    switch (type.kind) {
        case TY_TAG: printf(colo(32, %s), type.tag);
        cass TY_PTR: printf("*"), dump_type(*type.ptr.to);
        cass TY_ARR: printf("[" colo(34, %u) "]", type.arr.size), dump_type(*type.arr.of);
        cass TY_FUN: {
            printf("(");
            sz const l = type.fun.params.len;
            for (sz k = 0; k < l; ++k) {
                if (k) printf(", ");
                dump_decl(type.fun.params.ptr[l-1-k]);
            }
            printf(") ");
            dump_type(*type.fun.ret);
        }
    }
}

void free_decl(struct decl decl) {
    free(decl.name);
    free_type(decl.type);
}
void dump_decl(struct decl decl) {
    printf("%s: ", decl.name);
    dump_type(decl.type);
}

typedef union stype {
    int i;
    unsigned u;
    char* s;

    struct type type;
    struct decl decl;
    arry_decls decls;
} syg;
#define YYSTYPE union stype
%}

start
    = - it_:decl ';'
        {
            dump_decl(it_.decl);
            printf(";\n");
            free_decl(it_.decl);
        }
    ;

##

decl
    = id_:ident - ':' - ty_:type -
        { $$ = (syg){.decl= {.name= id_.s, .type=ty_.type}}; }
    ;

type
    = tag_:ident -
        { $$ = (syg){.type= {.kind= TY_TAG, .tag= tag_.s, }}; }
    | '*' - to_:type -
        { $$ = (syg){.type= {.kind= TY_PTR, .ptr= {.to= &to_.type}}}; }
    | '[' - size_:num - ']' - of_:type -
        { $$ = (syg){.type= {.kind= TY_ARR, .arr= {.of= &of_.type, .size= size_.u}}}; }
    | '(' - params_:params - ')' - ret_:type -
        { $$ = (syg){.type= {.kind= TY_FUN, .fun= {.ret= &ret_.type, .params= params_.decls}}}; }
    ;

params
    = &')'
        { $$ = (syg){.decls= {0}}; }
    | head_:decl - (','|&')') - tail_:params -
        { $$ = tail_; *push(&$$.decls) = head_.decl; }
    ;

##

ident = <[A-Z_a-z][0-9A-Z_a-z]*> { $$.s = strdup(yytext); }
num = <[0-9]+> { $$.u = atoi(yytext); }

- = (blank | comment)*
blank = ' ' | '\t' | '\n' | '\v' | '\f' | '\r'
comment = ('//'|'#') !'\n' | '/*' .* '*/'

%%

int main(void) {
    while (yyparse());
}
