/// C lexer with inbuilt preprocessor; example:
/// ```c
/// lex_state ls = {0};                    // $ cpp
/// lex_define(&ls, "_XOPEN_SOURCE", "1"); //     -D_XOPEN_SOURCE=1
/// lex_include(&ls, "lib/include");       //     -Ilib/include
/// lex_entry(&ls, "./main.c");            //     ./main.c
///
/// size_t k;
/// // EOF is an empty token ("", so starts with '\0')
/// while (k = lext(ls), ls.tokens.ptr[k]) {
///     printf("[%s:%3zu]\t%s\n",
///             // current file name as C-string
///             ls.work.ptr+ls.sources.ptr[ls.sources.len-1].file,
///             // current line number, 1-based
///             ls.sources.ptr[ls.sources.len-1].line,
///             // returned token `k` as C-string
///             ls.tokens.ptr+k);
/// }
///
/// lex_free(&ls);
/// ```
///
/// Recognised 'user config' macros are the following:
/// - on_lex_comment
/// - on_lex_sysinclude
/// - on_lex_missinclude
/// - on_lex_unknowndir
/// - on_lex_preprocerr

// TODO: remove `notif`s

#ifndef LEXER_H
#define LEXER_H

// this for testin, you don need that
#ifdef LEXER_H_SIMPLE_CPP
#define LEXER_H_IMPLEMENTATION
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct lex_state lex_state;

/// add to defined macros (-D)
void lex_define(lex_state* const ls, char const* const name, char const* const value);
/// remove from defined macros (-U)
void lex_undef(lex_state* const ls, char const* const name);
/// add to include paths (-I)
void lex_incdir(lex_state* const ls, char const* const path);
/// set the entry file, lexer ready to go
void lex_entry(lex_state* const ls, FILE* const stream, char const* const file);
/// go back (at most) `count` tokens (only reason it'd do less is if it found the begining since last `lex_entry`)
void lex_rewind(lex_state* const ls, size_t const count);
/// clear and delete everything (do not hold on to tokens!)
void lex_free(lex_state* const ls);
/// compute a preprocessor expression
long lex_eval(lex_state* const ls, char const* const expr);
/// next token, move forward (your char* is at `ls->tokens.ptr+return`, don't hold on to the ptr but to the offset)
size_t lext(lex_state* const ls);

/// make `"a\nb"` (including the quotes and the backslash) from `{0x61, 0x0a, 0x62}`
size_t lex_strquo(char* const quoted, size_t const size, char const* const unquoted, size_t const length);
/// make `{0x61, 0x0a, 0x62}` from `"a\nb"` (including the quotes and the backslash)
size_t lex_struqo(char* const unquoted, size_t const size, char const* const quoted);

#define arry(...) struct { __VA_ARGS__* ptr; size_t len, cap; }

struct lex_state {
    arry(struct lex_macro {
        size_t name; // in ls->work
        size_t value; // in ls->work
        size_t length; // of value (can be multiple \0-separated tokens)
        int params; // -1 if object-like
        int marked; // 0 if can expand
    }) macros;
    arry(char const*) paths;

    arry(struct lex_source {
        FILE* stream;
        size_t file; // in ls->work
        size_t line; // 1 based
    }) sources;
    char gotc; // private
    char const* cstream; // (essentially) private

    arry(char) work; // (mostly) private

    size_t ahead; // (preferably) private
    arry(char) tokens; // \0 \0 <token1> \0 <token2> \0 ... you can look back all you want, but do not interpret anything that's ahead
};

#ifdef LEXER_H_IMPLEMENTATION

#define frry(__a) (free((__a)->cap ? (__a)->ptr : NULL), (__a)->ptr = NULL, (__a)->len = (__a)->cap = 0)
#define push(__a) ((__a)->len < (__a)->cap || (((__a)->cap || !(__a)->ptr) && ((__a)->ptr = realloc((__a)->ptr, ((__a)->cap+= (__a)->cap+8)*sizeof*(__a)->ptr))) ? (__a)->ptr+(__a)->len++ : (exit(EXIT_FAILURE), (__a)->ptr))
#define grow(__a, __k, __n) _lex_arrygrow((char**)&(__a)->ptr, &(__a)->len, &(__a)->cap, sizeof*(__a)->ptr, (__k), (__n))
#define last(__a) ((__a)->ptr+(__a)->len-1)
#define each(__a) for (void* const _ptr = (__a)->ptr,* const _end = (__a)->ptr+(__a)->len; _end == (__a)->ptr ? (__a)->ptr = _ptr, 0 : 1; ++(__a)->ptr)
static char* _lex_arrygrow(char** const ptr, size_t* const len, size_t* const cap, size_t const s, size_t const k, size_t const n) {
    size_t const nlen = *len+n;
    if (*cap < nlen && !(cap && (*ptr = realloc(*ptr, (*cap = nlen)*s)))) exit(EXIT_FAILURE);
    if (k < *len) memmove(*ptr+(k+n)*s, *ptr+k*s, (*len-k)*s);
    return *len = nlen, *ptr+k*s;
}

// everything between / * and * / or // and newline, 0-terminated, no newline
#ifndef on_lex_comment
#define on_lex_comment(ls, cstr) (void)cstr
#endif
// the text given in <>, 0-terminated
#ifndef on_lex_sysinclude
#define on_lex_sysinclude(ls, cstr) (void)cstr
#endif
// the text given in "", 0-terminated
#ifndef on_lex_missinclude
#define on_lex_missinclude(ls, cstr) (void)cstr
#endif
// everything after the '#' exclusive, 0-terminated, no newline
#ifndef on_lex_unknowndir
#define on_lex_unknowndir(ls, cstr) (void)cstr
#endif
// everything after the 'error' exclusive, 0-terminated, no newline
#ifndef on_lex_preprocerr
#define on_lex_preprocerr(ls, cstr) (void)cstr
#endif

#define _HERE_STR(__ln) #__ln
#define _HERE_XSTR(__ln) _HERE_STR(__ln)
#define HERE "(" __FILE__ ":" _HERE_XSTR(__LINE__) ") "
#define notif(...) (fprintf(stderr, HERE __VA_ARGS__), fputc('\n', stderr))

static void _lex_dump(lex_state* const ls, FILE* const strm) {
#   define wstr(at) (ls->work.ptr+(at))
    fprintf(strm, "lex_state {\n");
    fprintf(strm, "    macros [\n");
    each (&ls->macros) {
        fprintf(strm, "        {\n");
        fprintf(strm, "            name= \"%s\"\n", wstr(ls->macros.ptr->name));
        fprintf(strm, "            value= [\n");
        for (size_t k = 0; k < ls->macros.ptr->length; k+= strlen(wstr(ls->macros.ptr->value+k))+1)
            fprintf(strm, "                %s\n", wstr(ls->macros.ptr->value+k));
        fprintf(strm, "            ]\n");
        fprintf(strm, "            length= %zu\n", ls->macros.ptr->length);
        fprintf(strm, "            params= %d\n", ls->macros.ptr->params);
        fprintf(strm, "            marked= %s\n", ls->macros.ptr->marked ? "true" : "false");
        fprintf(strm, "        }\n");
    }
    fprintf(strm, "    ]\n");
    fprintf(strm, "    paths [\n");
    each (&ls->paths) fprintf(strm, "        %s\n", *ls->paths.ptr);
    fprintf(strm, "    ]\n");
    fprintf(strm, "    sources [\n");
    each (&ls->sources) fprintf(strm, "        %s:%zu\n", wstr(ls->sources.ptr->file), ls->sources.ptr->line);
    fprintf(strm, "    ]\n");
    fprintf(strm, "    gotc= 0x%02hhX\n", ls->gotc);
    fprintf(strm, "    cstream= %s@\n", ls->cstream);
    fprintf(strm, "    work\n");
    for (size_t k = 0; k < ls->work.len; k+= strlen(wstr(k))+1)
        fprintf(strm, "%s@", wstr(k));
    fprintf(strm, "\n");
    fprintf(strm, "    ahead= %zu\n", ls->ahead);
    fprintf(strm, "    tokens\n");
    for (size_t k = 0; k < ls->tokens.len; k+= strlen(ls->tokens.ptr+k)+1)
        fprintf(strm, "%s@", ls->tokens.ptr+k);
    fprintf(strm, "\n%*s^\n", (unsigned)(ls->tokens.len - ls->ahead), "");
    fprintf(strm, "}\n");
#   undef wstr
}

#define curr  (ls->sources.ptr+ls->sources.len-1)
#define endd  (ls->cstream?! *ls->cstream : feof(curr->stream) || ferror(curr->stream))

#define isnum(__c) ('0' <= (__c) && (__c) <= '9')
#define isidh(__c) (('a' <= ((__c)|32) && ((__c)|32) <= 'z') || '_' == (__c))
#define isidt(__c) (isidh((__c)) || isnum((__c)))

static char _lex_getc(lex_state* const ls);

static void _lex_getdelim(lex_state* const ls, char const d) {
    char c;
    while (d != (c = _lex_getc(ls)) && !endd) *push(&ls->work) = c;
}

static void _lex_comment(lex_state* const ls) {
    do {
        _lex_getdelim(ls, '*');
        char const c = _lex_getc(ls);
        if ('/' == c) break;
        *push(&ls->work) = '*';
        *push(&ls->work) = c;
    } while (!endd);
}

static char _lex_getc(lex_state* const ls) {
    if (ls->cstream) {
        char const c = *ls->cstream++;
        return c ? c : '\n';
    }

    char c = ls->gotc ? ls->gotc : fgetc(curr->stream);
    ls->gotc = '\0';
    curr->line+= '\n' == c;
#   define case2(c1, c2, ret) if (c1 == c) { if (c2 == (c = fgetc(curr->stream))) return ret; else ungetc(c, curr->stream), c = c1; }
    case2('\\', '\n', (++curr->line, _lex_getc(ls)));
    case2('<', '%', '{');
    case2('%', '>', '}');
    case2('<', ':', '[');
    case2(':', '>', ']');
    case2('%', ':', '#');
    size_t const comment_at = ls->work.len;
    case2('/', '*', (
        _lex_comment(ls),
        *push(&ls->work) = '\0',
        ls->work.len = comment_at,
        (on_lex_comment(ls, (ls->work.ptr+comment_at))),
        ' '
    ));
    case2('/', '/', (
        _lex_getdelim(ls, '\n'),
        *push(&ls->work) = '\0',
        ls->work.len = comment_at,
        (on_lex_comment(ls, (ls->work.ptr+comment_at))),
        '\n'
    ));
#   undef case2
    return c;
}

static void _lex_ungetc(lex_state* const ls, char const c) {
    if (ls->cstream) {
        --ls->cstream;
        return;
    }

    if (ls->gotc) notif("ERR: double ungetc: %c, %c", ls->gotc, c), exit(1);
    ls->gotc = c;
    curr->line-= '\n' == c;
}

static size_t _lex_simple(lex_state* const ls) {
    char c;
#   define next() (c = _lex_getc(ls))
#   define accu() (*push(&ls->tokens) = next())
#   define undo() (_lex_ungetc(ls, ls->tokens.ptr[ls->tokens.len-1]), ls->tokens.ptr[ls->tokens.len-1] = '\0')

    size_t const token_at = ls->tokens.len;
    char const c0 = accu();

    switch (c0) {
    case '"':
    case '\'':
        do if ('\\' == accu()) accu(), accu();
        while (c0 != c && !endd);
        break;

    case '-':
    case '+': case '&': case '|': case '<': case '>': case '#':
    case '*': case '/': case '%': case '^': case '=': case '!':
        accu();
        switch (c0) {
        case '-':
            if ('>' == c) break;
            else
            // fall through
        case '+': case '&': case '|': case '<': case '>': case '#':
            if (c0 == c) {
                if ('<' == c || '>' == c) {
                    if ('=' == next()) *push(&ls->tokens) = '=';
                    else _lex_ungetc(ls, c);
                }
                break;
            } else
            // fall through
        case '*': case '/': case '%': case '^': case '=': case '!':
            if ('=' == c) break;
            else
            // fall through
        default:
            undo();
            return token_at;
        }
        break;

    default:
        if (isidh(c0)) {
            do accu();
            while (isidt(c));
            undo();
            return token_at;
        }

        if (isnum(c0) || '.' == c0) {
            if ('.' == c0 && (accu(), !isnum(c))) {
                if ('.' == c && '.' == accu()) break;
                undo();
                return token_at;
            }

            char const* dgts = "0123456789";
            int fp = 1;
            if ('.' != c0 && '0' == c) switch (accu()|32) {
            case 'b': dgts = "01";       fp = 0; accu(); break;
            case 'o': dgts = "01234567"; fp = 0; accu(); break;
            case 'x': dgts = "0123456789abcdef"; accu(); break;
            }

            while (strchr(dgts, c|32) || '\'' == c) accu();
            if (fp) {
                if ('.' != c0 && '.' == c) {
                    do accu();
                    while (strchr(dgts, c|32) || '\'' == c);
                }
                if ('e' == (c|32) || 'p' == (c|32)) {
                    accu();
                    if ('-' == c || '+' == c) c = '\'';
                    do accu();
                    while (strchr("0123456789", c|32) || '\'' == c);
                }
            }
            while ('f' == c || 'l' == c || 'u' == c) accu();

            undo();
            return token_at;
        }

        if (c0 < ' ' || '~' < c0) {
            ls->tokens.ptr[ls->tokens.len-1] = '\0';

            while (strchr(" \t\n", c = _lex_getc(ls)));
            _lex_ungetc(ls, c);

            return endd ? token_at : _lex_simple(ls);
        }
    }

#   undef undo
#   undef accu
#   undef next

    *push(&ls->tokens) = '\0';
    return token_at;
}

static size_t _lex_ahead(lex_state* const ls) {
    size_t const token_at = ls->tokens.len - ls->ahead;
    size_t const toklen = strlen(ls->tokens.ptr+token_at);

    if (0x1a == ls->tokens.ptr[token_at]) {
        char const* const name = ls->tokens.ptr+token_at+1;
        ls->ahead-= toklen+1;
        notif("INFO: end expansion of '%s' (ahead: %zu)", name, ls->ahead);
        for (size_t k = 0; k < ls->macros.len; ++k) if (!strcmp(ls->work.ptr+ls->macros.ptr[k].name, name)) {
            notif("      yes");
            ls->macros.ptr[k].marked = 0;
            grow(&ls->tokens, token_at+toklen+1, -toklen-1);
            return lext(ls);
        }

        // should be unreachable
        ls->ahead = 0;
    }

    ls->ahead-= toklen+1;
    return token_at;
}

static long _lex_eval(lex_state* const ls) {
    notif("NIY: _lex_eval");
    char c;
    do {
        while (strchr(" \t", c = _lex_getc(ls)));
        if ('\n' == c) break;
        _lex_ungetc(ls, c);

        size_t const plen = ls->tokens.len, token_at = _lex_simple(ls);
        ls->tokens.len = plen;

        char const* const token = ls->tokens.ptr+token_at;
        long const value = atol(token);
        notif("if token: '%s' -> %ld", token, value);
        if (value) return 1;
    } while (endd);
    return 0;
}

static void _lex_skipblock(lex_state* const ls, int const can_else) {
    size_t depth = 1;
    int done = 0;
    do {
        size_t const dir_at = ls->work.len;

        char c;
        while (strchr(" \t", c = _lex_getc(ls)));

        if ('#' == c) {
            do *push(&ls->work) = c = _lex_getc(ls);
            while (isidt(c));
            _lex_ungetc(ls, ls->work.ptr[ls->work.len-1]);
            ls->work.ptr[ls->work.len-1] = '\0';

            notif("INFO: directive in disabled preproc block: '%s'", ls->work.ptr+dir_at);
            notif("      (depth: %zu)", depth);

#           define diris(__l) (!strcmp(__l, ls->work.ptr+dir_at))

            if (diris("ifdef") || diris("ifndef") || diris("if")) ++depth;
            else if (diris("endif") && !--depth) done = 1;
            else if (can_else && 1 == depth) {
                if (diris("else")) done = 1;
                else if (diris("elif") && (c = '\n', _lex_eval(ls))) done = 1;
            }

#           undef diris
        }

        if ('\n' != c) _lex_getdelim(ls, '\n');
        ls->work.len = dir_at;
    } while (!done && (!endd || (fclose(curr->stream), --ls->sources.len)));
}

static void _lex_directive(lex_state* const ls) {
    char c;

#   define next() (c = _lex_getc(ls))
#   define accu() (*push(&ls->work) = next())

    size_t const dir_at = ls->work.len;

    while (strchr(" \t", next()));
    if ('\n' == c) return;
    *push(&ls->work) = c;

    do accu();
    while (isidt(c));
    _lex_ungetc(ls, ls->work.ptr[ls->work.len-1]);
    ls->work.ptr[ls->work.len-1] = '\0';
    ls->work.len = dir_at;

    while (strchr(" \t", next()));

#   define diris(__l) (!strcmp(__l, ls->work.ptr+dir_at))
    if (0) ;

    else if (diris("error")) {
        size_t const message_at = ls->work.len;
        *push(&ls->work) = c;
        _lex_getdelim(ls, '\n');
        *push(&ls->work) = '\0';
        ls->work.len = dir_at;
        on_lex_preprocerr(ls, (ls->work.ptr+message_at));

        fclose(curr->stream);
        --ls->sources.len;
    }

    else if (diris("include")) {
        size_t const path_at = ls->work.len;

        if ('"' == c) {
            while ('"' != accu() && !endd);
            ls->work.ptr[ls->work.len-1] = '\0';
            _lex_getdelim(ls, '\n');
            ls->work.len = dir_at;

            char file[2048];

            char const* const dirname_end = strrchr(ls->work.ptr+curr->file, '/');
            if (dirname_end) {
                memcpy(file, ls->work.ptr+curr->file, dirname_end+1 - ls->work.ptr+curr->file);
                file[dirname_end+1 - ls->work.ptr+curr->file] = '\0';
            } else file[0] = '\0';

            size_t k = 0;
            do {
                if (file[0] && '/' != file[strlen(file)-1]) file[strlen(file)] = '/';
                strcat(file, ls->work.ptr+path_at);

                FILE* const stream = fopen(file, "r");
                if (stream) {
                    size_t const file_at = ls->work.len;
                    strcpy(grow(&ls->work, ls->work.len, strlen(file)+1), file);
                    *push(&ls->sources) = (struct lex_source){.stream= stream, .file= file_at, .line= 1};
                    break;
                }

                if (k >= ls->paths.len) {
                    on_lex_missinclude(ls, (ls->work.ptr+path_at));
                    break;
                }
                strcpy(file, ls->paths.ptr[k++]);
            } while (1);
        }

        else if ('<' == c) {
            while ('>' != accu() && !endd);
            ls->work.ptr[ls->work.len-1] = '\0';
            _lex_getdelim(ls, '\n');
            ls->work.len = dir_at;
            on_lex_sysinclude(ls, (ls->work.ptr+path_at));
        }

        else notif("NIY: expand preprocessor identifier for file path");
    }

    else if (diris("define")) {
        size_t const name_at = ls->work.len;
        *push(&ls->work) = c;
        do accu();
        while (isidt(c));
        ls->work.ptr[ls->work.len-1] = '\0';

        int params = -1;
        if ('(' == c) {
            ++params;
            while (strchr(" \t", next()));

            while (')' != c && !endd) {
                *push(&ls->work) = c;
                do accu();
                while (isidt(c) || '.' == c);
                ls->work.ptr[ls->work.len-1] = '\0';

                ++params;
                if (')' == c) break;
                while (strchr(" \t", next()));
                if (',' == c) while (strchr(" \t", next()));
            }
        }

        size_t const value_at = ls->work.len;
        size_t length = 0;
        if ('\n' != c) do {
            while (strchr(" \t", next()));
            if ('\n' == c) break;
            _lex_ungetc(ls, c);

            size_t const plen = ls->tokens.len, token_at = _lex_simple(ls);
            ls->tokens.len = plen;

            size_t const toklen = strlen(ls->tokens.ptr+token_at);
            length+= toklen+1;
            strcpy(grow(&ls->work, ls->work.len, toklen+1), ls->tokens.ptr+token_at);
        } while (endd);

        struct lex_macro* found = NULL;
        for (size_t k = 0; k < ls->macros.len; ++k) if (!strcmp(ls->work.ptr+ls->macros.ptr[k].name, ls->work.ptr+name_at)) {
            found = ls->macros.ptr+k;
            break;
        }
        *(found ? found : push(&ls->macros)) = (struct lex_macro){
            .name= name_at,
            .value= value_at,
            .length= length,
            .params= params,
        };
    }

    else if (diris("undef")) {
        size_t const name_at = ls->work.len;

        *push(&ls->work) = c;
        do accu();
        while (!strchr(" \t\n\r", c));
        ls->work.ptr[ls->work.len-1] = '\0';
        ls->work.len = dir_at;

        lex_undef(ls, ls->work.ptr+name_at);
        if ('\n' != c) _lex_getdelim(ls, '\n');
    }

    else if (diris("ifdef") || diris("ifndef") || diris("if")) {
        _lex_ungetc(ls, c);

        int cond;
        if ('\0' == ls->work.ptr[dir_at+2]) cond = !!_lex_eval(ls);
        else {
            size_t const name_at = ls->work.len;
            int const n = 'n' == ls->work.ptr[dir_at+2];

            do accu();
            while (!strchr(" \t\n\r", c));
            ls->work.ptr[ls->work.len-1] = '\0';
            ls->work.len = dir_at;

            for (size_t k = 0; k < ls->macros.len; ++k) if (!strcmp(ls->work.ptr+ls->macros.ptr[k].name, ls->work.ptr+name_at)) {
                cond = 1;
                break;
            }
            if (n) cond = !cond;
        }

        if (!cond) _lex_skipblock(ls, 1);
    }

    else if (diris("else") || diris("elif")) {
        ls->work.len = dir_at;
        _lex_skipblock(ls, 0);
    }

    else if (diris("endif")) {
        _lex_getdelim(ls, '\n');
        ls->work.len = dir_at;
    }

    else {
        ls->work.len+= strlen(ls->work.ptr+dir_at)+1;
        ls->work.ptr[ls->work.len-1] = ' ';
        *push(&ls->work) = c;
        _lex_getdelim(ls, '\n');
        *push(&ls->work) = '\0';
        ls->work.len = dir_at;
        on_lex_unknowndir(ls, (ls->work.ptr+dir_at));
    }

#   undef diris

#   undef accu
#   undef next
}

void lex_define(lex_state* const ls, char const* const name, char const* const value) {
    size_t const name_at = ls->work.len, name_len = strcspn(name, "(");
    memcpy(grow(&ls->work, ls->work.len, name_len+1), name, name_len);
    ls->work.ptr[ls->work.len-1] = '\0';

    int params = -1;
    if ('(' == name[name_len]) {
        size_t k = name_len;
        ++params;
        while (strchr(" \t", name[++k]));

        while (')' != name[k] && name[k]) {
            do *push(&ls->work) = name[k++];
            while (isidt(name[k]) || '.' == name[k]);
            *push(&ls->work) = '\0';

            ++params;
            if (')' == name[k]) break;
            while (strchr(" \t", name[++k]));
            if (',' == name[k]) while (strchr(" \t", name[++k]));
        }
    }

    ls->cstream = value;
    char c;

    size_t const value_at = ls->work.len;
    size_t length = 0;
    if ('\n' != *value) do {
        while (strchr(" \t", c = _lex_getc(ls)));
        if ('\n' == c) break;
        _lex_ungetc(ls, c);

        size_t const plen = ls->tokens.len, token_at = _lex_simple(ls);
        ls->tokens.len = plen;

        size_t const toklen = strlen(ls->tokens.ptr+token_at);
        length+= toklen+1;
        strcpy(grow(&ls->work, ls->work.len, toklen+1), ls->tokens.ptr+token_at);
    } while (*ls->cstream);

    ls->cstream = NULL;

    struct lex_macro* found = NULL;
    for (size_t k = 0; k < ls->macros.len; ++k) if (!strcmp(ls->work.ptr+ls->macros.ptr[k].name, ls->work.ptr+name_at)) {
        found = ls->macros.ptr+k;
        break;
    }
    *(found ? found : push(&ls->macros)) = (struct lex_macro){
        .name= name_at,
        .value= value_at,
        .length= length,
        .params= params,
    };
}

void lex_undef(lex_state* const ls, char const* const name) {
    for (size_t k = 0; k < ls->macros.len; ++k) if (!strcmp(name, ls->work.ptr+ls->macros.ptr[k].name)) {
        grow(&ls->macros, k+1, -1);
        break;
    }
}

void lex_incdir(lex_state* const ls, char const* const path) {
    *push(&ls->paths) = path;
}

void lex_entry(lex_state* const ls, FILE* const stream, char const* const file) {
    size_t const file_at = ls->work.len;
    strcpy(grow(&ls->work, ls->work.len, strlen(file)+1), file);
    *push(&ls->sources) = (struct lex_source){.stream= stream, .file= file_at, .line= 1};
    *push(&ls->tokens) = '\0';
    *push(&ls->tokens) = '\0';
}

void lex_rewind(lex_state* const ls, size_t const count) {
    for (size_t k = 0; k < count; ++k) {
        size_t n = ls->tokens.len - ls->ahead - 2;
        if (!ls->tokens.ptr[n]) break;
        while (--n && !ls->tokens.ptr[n]);
    }
}

void lex_free(lex_state* const ls) {
    frry(&ls->macros);
    frry(&ls->paths);
    each (&ls->sources) if (stdin != ls->sources.ptr->stream) fclose(ls->sources.ptr->stream);
    frry(&ls->sources);
    frry(&ls->work);
    frry(&ls->tokens);
}

long lex_eval(lex_state* const ls, char const* const expr) {
    ls->cstream = expr;
    long r = _lex_eval(ls);
    ls->cstream = NULL;
    return r;
}

size_t lext(lex_state* const ls) {
    if (!ls->sources.len || endd) {
        *push(&ls->tokens) = '\0';
        return ls->tokens.len-1;
    }

    if (!ls->ahead) {
        size_t const pline = curr->line;

        char c;
        while (strchr(" \t\n\r", c = _lex_getc(ls)));

        if (endd) {
            fclose(curr->stream);
            if (!--ls->sources.len) {
                *push(&ls->tokens) = '\0';
                return ls->tokens.len-1;
            } else return lext(ls);
        }

        if ('#' == c && ('\0' == ls->tokens.ptr[ls->tokens.len-2] || pline != curr->line)) {
            _lex_directive(ls);
            return lext(ls);
        }

        _lex_ungetc(ls, c);
    }

    size_t const r = ls->ahead ? _lex_ahead(ls) : _lex_simple(ls);

    if (isidh(ls->tokens.ptr[r])) {
        time_t tt;
        char replace[2048];
        size_t replace_len = 0;

#       define nameis(__l) (!strcmp(__l, ls->tokens.ptr+r))
        if (0) ;

        else if (nameis("__FILE__")) replace_len = lex_strquo(replace, sizeof replace-1, ls->work.ptr+curr->file, strlen(ls->work.ptr+curr->file));
        else if (nameis("__LINE__")) replace_len = snprintf(replace, sizeof replace-1, "%zu", curr->line);
        else if (nameis("__DATE__")) replace_len = strftime(replace, sizeof replace-1, "\"%b %e %Y\"", localtime((time(&tt), &tt)));
        else if (nameis("__TIME__")) replace_len = strftime(replace, sizeof replace-1, "\"%T\"", localtime((time(&tt), &tt)));

        else for (size_t k = 0; k < ls->macros.len; ++k) if (!ls->macros.ptr[k].marked && nameis(ls->work.ptr+ls->macros.ptr[k].name)) {
            size_t const len = ls->macros.ptr[k].length;
            ls->ahead+= len+1+strlen(ls->tokens.ptr+r)+1;
            memcpy(grow(&ls->tokens, r, len+1), ls->work.ptr+ls->macros.ptr[k].value, len);
            ls->tokens.ptr[r+len] = 0x1a;
            ls->macros.ptr[k].marked = 1;
            return lext(ls);
        }
#       undef nameis

        if (replace_len) {
            size_t const token_len = strlen(ls->tokens.ptr+r);
            grow(&ls->tokens, r+token_len, replace_len-token_len);
            strcpy(ls->tokens.ptr+r, replace);
        }
    }

    if ('"' == ls->tokens.ptr[r]) {
        size_t const n = lext(ls);
        if ('"' == ls->tokens.ptr[n]) grow(&ls->tokens, n+1, -3);
        else ls->ahead+= strlen(ls->tokens.ptr+n)+1;
    }

    return r;
}

size_t lex_strquo(char* const quoted, size_t const size, char const* const unquoted, size_t const length) {
    if (size <3 +length) return 0;
    size_t d = 1, s = 0;
    quoted[0] = '"';

    while (d < size && s < length) {
        char c = unquoted[s++];

        if (' ' <= c && c <= '~' && !strchr("'\"?\\", c)) quoted[d++] = c;
        else {
            switch(c) {
            case '\'': c ='\''; break;
            case '\"': c = '"'; break;
            case '\?': c = '?'; break;
            case '\\': c ='\\'; break;
            case '\a': c = 'a'; break;
            case '\b': c = 'b'; break;
            case '\f': c = 'f'; break;
            case '\n': c = 'n'; break;
            case '\r': c = 'r'; break;
            case '\t': c = 't'; break;
            case '\v': c = 'v'; break;
            case '\0': c = '0'; break;

            default:
                if (d+4 >= size) return 0;
                quoted[d++] = '\\';
                quoted[d++] = 'x';
                quoted[d++] = "0123456789abcdef"[c>>4 & 15];
                quoted[d++] = "0123456789abcdef"[c & 15];
                continue;
            }

            if (d+2 >= size) return 0;
            quoted[d++] = '\\';
            quoted[d++] = c;
        }
    }

    if (d+2 >= size) return 0;
    quoted[d++] = '"';
    quoted[d] = '\0';
    return d;
}

size_t lex_struqo(char* const unquoted, size_t const size, char const* const quoted) {
    char const c0 = quoted[0];
    size_t d = 0, s = 1;

    while (d < size && c0 != quoted[s]) {
        char c = quoted[s++];
        if (!c) return 0;

        if ('\\' != c) unquoted[d++] = c;
        else switch (c = quoted[s++]) {
        case'\'': unquoted[d++] = '\''; break;
        case '"': unquoted[d++] = '\"'; break;
        case '?': unquoted[d++] = '\?'; break;
        case'\\': unquoted[d++] = '\\'; break;
        case 'a': unquoted[d++] = '\a'; break;
        case 'b': unquoted[d++] = '\b'; break;
        case 'f': unquoted[d++] = '\f'; break;
        case 'n': unquoted[d++] = '\n'; break;
        case 'r': unquoted[d++] = '\r'; break;
        case 't': unquoted[d++] = '\t'; break;
        case 'v': unquoted[d++] = '\v'; break;

            long unsigned r;

        case 'x':
            r = 0;
            static char const dgts[] = "0123456789abcdef";
            char const* v = strchr(dgts, quoted[s]|32);
            do r = (r<<4) + (v-dgts);
            while ((v = strchr(dgts, quoted[++s]|32)));
            unquoted[d++] = r;
            break;

        case 'u':
        case 'U':
            notif("NIY: 'Universal character names' (Unicode)");
            return 0;
            break;

        default:
            if ('0' <= c && c <= '7') {
                r = 0;
                do r = (r<<3) + (c-'0');
                while (c = quoted[s++], '0' <= c && c <= '7');
                --s;
                unquoted[d++] = r;
            } else unquoted[d++] = c;
        }
    }

    return quoted[s+1] ? 0 : d;
}

#undef isid_t
#undef isid_h
#undef isnum

#undef endd
#undef curr

#ifdef LEXER_H_SIMPLE_CPP
int main(int argc, char** argv) {
    char const* const prog = *(--argc, argv++);

    FILE* stream = stdin;
    char const* file = "<stdin>";

    lex_state* const ls = &(lex_state){0};

    for (; argc--; ++argv) {
        if ('-' == **argv) switch ((*argv)[1]) {
        default:  printf("Unknown flag -%c\n", (*argv)[1]);     return EXIT_FAILURE;
        case 'h': printf("Usage: %s [-D..|-U..|-I..]\n", prog); return EXIT_FAILURE;
            char* eq;
        case 'D': lex_define(ls, *argv+2, (eq = strchr(*argv, '=')) ? *eq = '\0', eq+1 : "1"); break;
        case 'U': lex_undef(ls, *argv+2);  break;
        case 'I': lex_incdir(ls, *argv+2); break;
        case'\0': break;
        } else if (!(stream = fopen(file = *argv, "r"))) {
            printf("Could not read file %s\n", *argv);
            return EXIT_FAILURE;
        }
    }

    lex_entry(ls, stream, file);

    size_t k;
    while (k = lext(ls), ls->tokens.ptr[k]) {
        if ('@' == ls->tokens.ptr[k]) {
            ls->tokens.len-= 2;
            _lex_dump(ls, stdout);
        } else printf("[%s:%3zu]\t%s\n",
                ls->work.ptr+ls->sources.ptr[ls->sources.len-1].file,
                ls->sources.ptr[ls->sources.len-1].line,
                ls->tokens.ptr+k);
    }

    lex_free(ls);
    return EXIT_SUCCESS;
}
#endif // LEXER_H_SIMPLE_CPP

#undef notif
#undef HERE
#undef _HERE_XSTR
#undef _HERE_STR

#undef each
#undef last
#undef grow
#undef push
#undef frry

#endif // LEXER_H_IMPLEMENTATION

#undef arry

#endif // LEXER_H
