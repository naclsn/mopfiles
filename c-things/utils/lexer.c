#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct lex_state lex_state;

/// add to defined macros (-D)
void lex_define(lex_state* const ls, char const* const name, char const* const value);
/// remove from defined macros (-U)
void lex_undef(lex_state* const ls, char const* const name);
/// add to include paths (-I)
void lex_include(lex_state* const ls, char const* const path);
/// set the entry file, lexer ready to go
void lex_entry(lex_state* const ls, FILE* const stream, char const* const file);
/// clear and delete everything (do not hold on to bufsl tokens!)
void lex_free(lex_state* const ls);
/// compute a preprocessor expression
long lex_eval(lex_state const* const ls, char const* const expr);
/// next token, move forward (your char* is at `ls->tokens.ptr+return`, don't hold on to the ptr but to the offset)
size_t lext(lex_state* const ls);

#define arry(...) struct { __VA_ARGS__* ptr; size_t len, cap; }
#define frry(__a) ((__a)->cap ? free((__a)->ptr) : (void)0, (__a)->ptr = NULL, (__a)->len = (__a)->cap = 0)
#define push(__a) ((__a)->len < (__a)->cap || (((__a)->cap || !(__a)->ptr) && ((__a)->ptr = realloc((__a)->ptr, ((__a)->cap+= (__a)->cap+8)*sizeof*(__a)->ptr))) ? (__a)->ptr+(__a)->len++ : (exit(EXIT_FAILURE), (__a)->ptr))
#define grow(__a, __k, __n) _grow((char**)&(__a)->ptr, &(__a)->len, &(__a)->cap, sizeof*(__a)->ptr, (__k), (__n))
#define last(__a) ((__a)->ptr+(__a)->len-1)
#define each(__a) for (void* const _ptr = (__a)->ptr,* const _end = (__a)->ptr+(__a)->len; _end == (__a)->ptr ? (__a)->ptr = _ptr, 0 : 1; ++(__a)->ptr)
static inline void* _grow(char** const ptr, size_t* const len, size_t* const cap, size_t const s, size_t const k, size_t const n) {
    size_t const nlen = *len+n;
    if (*cap < nlen && !(cap && (*ptr = realloc(*ptr, (*cap = nlen)*s)))) exit(EXIT_FAILURE);
    if (k < *len) memmove(*ptr+(k+n)*s, *ptr+k*s, (*len-k)*s);
    return *len = nlen, *ptr+k*s;
}

#define _HERE_STR(__ln) #__ln
#define _HERE_XSTR(__ln) _HERE_STR(__ln)
#define HERE "(" __FILE__ ":" _HERE_XSTR(__LINE__) ") "
#define notif(...) (fprintf(stderr, HERE __VA_ARGS__), fputc('\n', stderr))

struct lex_state {
    arry(struct lex_macro {
        size_t name; // in ls->work
        size_t value; // in ls->work
        int params;
    }) macros;
    arry(char const*) paths;

    size_t current;
    arry(struct lex_source {
        FILE* stream;
        size_t file; // in ls->work
        size_t line;
    }) sources;
    char gotc;

    arry(char) work;
    arry(char) tokens;
};

void lex_define(lex_state* const ls, char const* const name, char const* const value) {
    size_t const name_at = ls->work.len, name_len = strcspn(name, "(");
    memcpy(grow(&ls->work, ls->work.len, name_len+1), name, name_len);
    ls->work.ptr[ls->work.len-1] = '\0';

    int params = -1;
    if ('(' == name[name_len]) {
        params++;
        notif("NIY: lex_define with function-like macro");
    }

    size_t const value_at = ls->work.len;
    strcpy(grow(&ls->work, ls->work.len, strlen(value)+1), value);

    *push(&ls->macros) = (struct lex_macro){
        .name= name_at,
        .value= value_at,
        .params= params,
    };
}

void lex_undef(lex_state* const ls, char const* const name) {
    for (size_t k = 0; k < ls->macros.len; ++k) if (strcmp(name, ls->work.ptr+ls->macros.ptr[k].name)) {
        grow(&ls->macros, k, -1);
        break;
    }
}

void lex_include(lex_state* const ls, char const* const path) {
    *push(&ls->paths) = path;
}

void lex_entry(lex_state* const ls, FILE* const stream, char const* const file) {
    size_t const file_at = ls->work.len;
    strcpy(grow(&ls->work, ls->work.len, strlen(file)+1), file);
    *push(&ls->sources) = (struct lex_source){.stream= stream, .file= file_at, .line= 1};
    *push(&ls->tokens) = '\0';
    *push(&ls->tokens) = '\0';
}

long lex_eval(lex_state const* const ls, char const* const expr) {
    notif("NIY: lex_eval");
    return 0;
}

void lex_free(lex_state* const ls) {
    frry(&ls->macros);
    frry(&ls->paths);
    each (&ls->sources) if (stdin != ls->sources.ptr->stream) fclose(ls->sources.ptr->stream);
    frry(&ls->sources);
    frry(&ls->work);
    frry(&ls->tokens);
}

#define curr  (ls->sources.ptr+ls->current)

static char _lex_getc(lex_state* const ls);

static void _lex_getdelim(lex_state* const ls, char const d) {
    char c;
    while (d != (c = _lex_getc(ls)) && !feof(curr->stream) && !ferror(curr->stream)) *push(&ls->work) = c;
}

static void _lex_comment(lex_state* const ls) {
    do _lex_getdelim(ls, '*');
    while ('/' != (*push(&ls->work) = _lex_getc(ls)));
}

static char _lex_getc(lex_state* const ls) {
    int c = ls->gotc ? ls->gotc : fgetc(curr->stream);
    ls->gotc = '\0';
    curr->line+= '\n' == c;
#   define case2(c1, c2, ret) if (c1 == c) { if (c2 == (c = fgetc(curr->stream))) return ret; else ungetc(c, curr->stream); }
    case2('\\', '\n', (++curr->line, _lex_getc(ls)));
    case2('<', '%', '{');
    case2('%', '>', '}');
    case2('<', ':', '[');
    case2(':', '>', ']');
    case2('%', ':', '#');
    size_t const comment_at = ls->work.len;
    case2('/', '*', (
        _lex_comment(ls),
#       ifdef on_lex_comment
            *push(&ls->work) = '\0',
            (on_lex_comment(ls->work.ptr+comment_at)),
#       endif
        ls->work.len = comment_at,
        ' '
    ));
    case2('/', '/', (
        _lex_getdelim(ls, '\n'),
#       ifdef on_lex_comment
            *push(&ls->work) = '\0',
            (on_lex_comment(ls->work.ptr+comment_at)),
#       endif
        ls->work.len = comment_at,
        '\n'
    ));
#   undef case2
    return c;
}

static void _lex_ungetc(lex_state* const ls, char const c) {
    ls->gotc = c;
    curr->line-= '\n' == c;
}

size_t lext(lex_state* const ls) {
    size_t const pline = curr->line;

    char c;
#   define next() (c = _lex_getc(ls))

    while (strchr(" \t\n\r", next()));

    size_t const token_at = ls->tokens.len;
    if (feof(curr->stream) || ferror(curr->stream)) {
        *push(&ls->tokens) = '\0';
        return token_at;
    }

    if (('\0' == ls->tokens.ptr[token_at-2] || pline != curr->line) && '#' == c) {
        while (strchr(" \t\n\r", next()));

        size_t const dir = ls->work.len;

#       define accu() (*push(&ls->work) = next())

        *push(&ls->work) = c;
        do accu();
        while ('a' <= c && c <= 'z');
        unsigned const len = ls->work.len - dir - 1;
        ls->work.len = dir;
#       define diris(__l) (strlen(__l) == len && !memcmp(__l, ls->work.ptr+dir, strlen(__l)))
        if (0) ;

        else if (diris("include")) {
            while (strchr(" \t", next()));

            size_t const path_at = ls->work.len;
            if ('"' == c) {
                while ('"' != accu());
                ls->work.ptr[ls->work.len-1] = '\0';
                notif("INFO: including %s", ls->work.ptr+path_at);
                _lex_getdelim(ls, '\n');
                ls->work.len = dir;
            }

            else if ('<' == c) {
                while ('>' != accu());
                ls->work.ptr[ls->work.len-1] = '\0';
                notif("INFO: skipping system inclusion of %s", ls->work.ptr+path_at);
#               ifdef on_lex_system
                    on_lex_system(ls->work.ptr+path_at);
#               endif
                _lex_getdelim(ls, '\n');
                ls->work.len = dir;
            }
        }

        else if (diris("define")) {
            while (strchr(" \t", next()));

            size_t const name_at = ls->work.len;
            *push(&ls->work) = c;
            do accu();
            while (('a' <= (c|32) && (c|32) <= 'z') || '_' == c || ('0' <= c && c <= '9'));
            ls->work.ptr[ls->work.len-1] = '\0';

            int params = -1;
            if ('(' == c) {
                ++params;
                while (strchr(" \t", next()));

                while (')' != c) {
                    size_t const a = ls->work.len;

                    *push(&ls->work) = c;
                    do accu();
                    while (('a' <= (c|32) && (c|32) <= 'z') || '_' == c || ('0' <= c && c <= '9') || '.' == c);
                    ls->work.ptr[ls->work.len-1] = '\0';

                    notif("INFO: macro param: %s", ls->work.ptr+a);
                    if (')' == c) break;

                    ++params;
                    while (strchr(" \t", next()));

                    if (',' == c) while (strchr(" \t", next()));
                }
            }

            size_t const value_at = ls->work.len;
            if ('\n' == c) *push(&ls->work) = '\0';
            else {
                while ('\n' != c) accu();
                ls->work.ptr[ls->work.len-1] = '\0';
            }

            *push(&ls->macros) = (struct lex_macro){
                .name= name_at,
                .value= value_at,
                .params= params,
            };
            notif("INFO: defined %s with value: \"%s\"", ls->work.ptr+name_at, ls->work.ptr+value_at);
        }

        else if (diris("undef")) {
            while (strchr(" \t", next()));

            size_t const name_at = ls->work.len;

            *push(&ls->work) = c;
            do accu();
            while (!strchr(" \t\n\r", c));
            ls->work.ptr[ls->work.len-1] = '\0';

            lex_undef(ls, ls->work.ptr+name_at);
            notif("INFO: undefined %s", ls->work.ptr+name_at);
        }

        else {
            notif("WARN: unknown or unhandled preprocessor directive #%.*s", len, ls->work.ptr+dir);
            _lex_getdelim(ls, '\n');
            ls->work.len = dir;
        }

#       undef diris

#       undef accu

        return lext(ls);
    }

#   define accu() (*push(&ls->tokens) = next())
#   define undo() (_lex_ungetc(ls, ls->tokens.ptr[--ls->tokens.len]))
    char const c0 = *push(&ls->tokens) = c;

    switch (c0) {
    case '"':
        do {
            if ('"' == (c = _lex_getc(ls))) {
                while (strchr(" \t\n\r", next()));
                if ('"' != c) {
                    _lex_ungetc(ls, c);
                    break;
                }
                next();
            }
            *push(&ls->tokens) = c;
            if ('\\' == c) accu();
        } while (1);
        *push(&ls->tokens) = '"';
        break;

    case '\'':
        do if ('\\' == accu()) accu();
        while ('\'' != c);
        break;

    case '-':
    case '+': case '&': case '|': case '<': case '>':
    case '*': case '/': case '%': case '^': case '=': case '!':
        accu();
        switch (c0) {
        case '-':
            if ('>' == c) break;
            else
        case '+': case '&': case '|': case '<': case '>':
            if (c0 == c) {
                if ('<' == c || '>' == c) {
                    if ('=' == next()) *push(&ls->tokens) = '=';
                    else _lex_ungetc(ls, c);
                }
                break;
            } else
        case '*': case '/': case '%': case '^': case '=': case '!':
            if ('=' == c) break;
            else
        default:
            undo();
        }
        break;

    default:
        if (('a' <= (c|32) && (c|32) <= 'z') || '_' == c) {
            do accu();
            while (('a' <= (c|32) && (c|32) <= 'z') || '_' == c || ('0' <= c && c <= '9'));
            undo();
            break;
        }

        if (('0' <= c && c <= '9') || '.' == c) {
            if ('.' == c0) {
                accu();
                if (!('0' <= c && c <= '9')) {
                    if ('.' != c) undo();
                    else {
                        accu();
                        if ('.' != c) undo();
                    }
                    break;
                }
            }

            do accu();
            while ('0' <= c && c <= '9');
            undo();
            break;
        }
    }

#   undef accu
#   undef undo

#   undef next

    *push(&ls->tokens) = '\0';
    return token_at;
}

#undef curr

#undef notif
#undef HERE
#undef _HERE_XSTR
#undef _HERE_STR

#undef each
#undef last
#undef grow
#undef push
#undef frry
#undef arry


int main(void) {
    lex_state* const ls = &(lex_state){0};
    lex_entry(ls, stdin, "<stdin>");

    size_t k;
    while (k = lext(ls), ls->tokens.ptr[k])
        printf("%s:%zu: @\x1b[36m%s\x1b[m@\n",
                ls->work.ptr+ls->sources.ptr[ls->current].file,
                ls->sources.ptr[ls->current].line,
                ls->tokens.ptr+k);

    lex_free(ls);
}
