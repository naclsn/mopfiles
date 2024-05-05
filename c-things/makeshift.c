//bin/cat <<'END' | ${CC:-c99 -x c -} -o makeshift -Wall -Wextra -Wpedantic ${CFLAGS:--ggdb -O1}
// https://www.gnu.org/software/make/manual/make.html#Reading-Makefiles
// usual/common {{{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // chdir

#define ref * const
#define cref const ref

#define exitf(...) (notif(__VA_ARGS__), exit(EXIT_FAILURE))
#define notif(...) (fprintf(stderr, "makeshift: " __VA_ARGS__), fputc(10, stderr))

#define arry(...) struct { __VA_ARGS__* ptr; size_t len, cap; }
#define frry(__a) ((__a)->cap ? free((__a)->ptr) : (void)0, (__a)->ptr = NULL, (__a)->len = (__a)->cap = 0)
#define push(__a) ((__a)->len < (__a)->cap || ((__a)->ptr = realloc((__a)->ptr, ((__a)->cap+= (__a)->cap+8)*sizeof*(__a)->ptr)) ? (void)0 : exitf("OOM"), (__a)->ptr+(__a)->len++)
#define grow(__a, __k, __n) _grow((char**)&(__a)->ptr, &(__a)->len, &(__a)->cap, sizeof*(__a)->ptr, (__k), (__n))
#define last(__a) ((__a)->ptr+(__a)->len-1)
#define each(__a) for (void ref _ptr = (__a)->ptr, ref _end = _ptr+(__a)->len; _end == (__a)->ptr ? (__a)->ptr = _ptr, 0 : 1; ++(__a)->ptr)
void* _grow(char* ref ptr, size_t ref len, size_t ref cap, size_t const s, size_t const k, size_t const n) {
    size_t const nlen = *len+n;
    if (*cap < nlen && !(*ptr = realloc(*ptr, (*cap = nlen)*s))) exitf("OOM");
    if (k < *len) memmove(*ptr+(k+n)*s, *ptr+k*s, (*len-k)*s);
    return *len = nlen, *ptr+k*s;
}

typedef arry(char) buf;
typedef arry(char*) lns;

#define bufmt(__b) (unsigned)(__b).len, (__b).ptr

buf bufdup(buf cref b) {
    buf r = {0};
    r.ptr = malloc(r.cap = b->len);
    if (!r.ptr) exitf("OOM");
    memcpy(r.ptr, b->ptr, r.len = r.cap);
    return r;
}

buf read_all(FILE ref f) {
    buf r = {0};
    if (!f) return r;
    if (!fseek(f, 0, SEEK_END)) {
        if (!(r.ptr = malloc(r.len = r.cap = ftell(f)))) exitf("OOM");
        fseek(f, 0, SEEK_SET);
        fread(r.ptr, 1, r.len, f);
    } else do {
        size_t const a = r.len ? r.len*2 : 1024;
        fread(grow(&r, r.len, a), 1, a, f);
    } while (!feof(f) && !ferror(f));
    return r;
}

lns word_slices(buf ref text) {
    lns r = {0};
    size_t h, k = 0;
#   define skipw() while ((h=k) < text->len && strchr("\t\n ", text->ptr[k])) k++
    skipw();
    for (; k <= text->len; k++) {
        if (h != k && (text->len == k || strchr("\t\n ", text->ptr[k]))) {
            *push(&r) = text->ptr+h;
            text->ptr[h-1] = '\0';
            skipw();
        } else if ('\\' == text->ptr[k]) k++;
    }
#   undef skipw
    return r;
}
// }}}

// state {{{
struct {
    struct {
        unsigned always_make :1,
                 ignore_errors :1,
                 keep_going :1,
                 dry_run :1,
                 question :1,
                 silent :1,
                 print_directory :1,
                 warn_undefined_variables :1;
    } flags;

    struct {
        char const* target_name;
        lns const* target_prereqs;
    } temps;

    //arry(char*) incdirs;
    struct {
        char* path;
        buf text;
    } file;

    arry(struct {
        char* name;
        lns prereqs;
        arry(buf) recipe;
    }) rules;

    arry(struct {
        char* name;
        buf val;
    }) vars;

    arry(char*) goals;
} _st = {0}, ref st = &_st;

void cleanup(void) {
    //each (&st->rules) {
    //    free(st->rules.ptr->name);
    //    free(st->rules.ptr->prereqs.ptr);
    //    each (&st->rules.ptr->recipe)
    //        frry(st->rules.ptr->recipe.ptr);
    //    free(st->rules.ptr->recipe.ptr);
    //}
    for (size_t k = 0; k < st->rules.len; k++) {
        free(st->rules.ptr[k].name);
        free(st->rules.ptr[k].prereqs.ptr);
        for (size_t k = 0; k < st->rules.ptr[k].recipe.len; k++)
            frry(&st->rules.ptr[k].recipe.ptr[k]);
        free(st->rules.ptr[k].recipe.ptr);
    }
    free(st->rules.ptr);

    //each (&st->vars) {
    //    free(st->vars.ptr->name);
    //    frry(&st->vars.ptr->val);
    //}
    for (size_t k = 0; k < st->vars.len; k++) {
        free(st->vars.ptr[k].name);
        frry(&st->vars.ptr[k].val);
    }
    free(st->vars.ptr);

    free(st->goals.ptr);
}
// }}}

buf next_line(buf ref slice, size_t ref lnum) {
    buf r = {0};
    size_t c;
    do {
#       define nx() (--slice->len ? *lnum+= '\n' == *++slice->ptr, slice->ptr : "")
        char const* h;
        while (slice->len && strchr(" \n", *slice->ptr)) nx();
        r.len = 0;
        c = -1;
        h = slice->ptr;
        for (; slice->len; nx()) {
            size_t const d = slice->ptr-h;
            char const p = *slice->ptr;
            if ('\\' == p && strchr("\n#", *nx())) {
                memcpy(grow(&r, r.len, d), h, d);
                h = slice->ptr + ('\n'==*slice->ptr);
            } else if ('\n' == p) {
                memcpy(grow(&r, r.len, d), h, d);
                break;
            } else if ('#' == p) c = r.len+d;
        }
#       undef nx
    } while (!c && slice->len);
    if (c < r.len) r.ptr[r.len = c] = '\0';
    else *push(&r) = '\0', r.len--;
    return r;
}

enum directive {
    dir_none,
    dir_export=    0<<4|6,
    dir_unexport=  1<<4|8,
    dir_override=  2<<4|8,
    dir_private=   3<<4|7,
    dir_ifeq=      4<<4|4,
    dir_ifneq=     5<<4|5,
    dir_ifdef=     6<<4|5,
    dir_ifndef=    7<<4|6,
    dir_endif=     8<<4|5,
    dir_include=   9<<4|7,
    dir_tinclude= 10<<4|8,
    dir_define=   11<<4|6,
    dir_endef=    12<<4|5,
    dir_undefine= 13<<4|8,
} scan_directive(buf cref line) {
    int const dash = '-' == *line->ptr;
    if (line->len < 4) return dir_none;
    if (dash) *line->ptr = 't';
#   define start(a,b,c,d) (a|b<<8|c<<16|d<<24)
    switch (start(line->ptr[0], line->ptr[1], line->ptr[2], line->ptr[3])) {
#       define returnif(__d) return strlen(#__d) < line->len && !memcmp(#__d, line->ptr, strlen(#__d)) && strchr("\t\n #('\"$", line->ptr[strlen(#__d)]) ? dir_##__d : dir_none
    case start('e','x','p','o'): returnif(export);
    case start('u','n','e','x'): returnif(unexport);
    case start('o','v','e','r'): returnif(override);
    case start('p','r','i','v'): returnif(private);
    case start('i','f','e','q'): returnif(ifeq);
    case start('i','f','n','e'): returnif(ifneq);
    case start('i','f','d','e'): returnif(ifdef);
    case start('i','f','n','d'): returnif(ifndef);
    case start('e','n','d','i'): returnif(endif);
    case start('i','n','c','l'): returnif(include);
    case start('t','i','n','c'): returnif(tinclude);
    case start('d','e','f','i'): returnif(define);
    case start('e','n','d','e'): returnif(endef);
    case start('u','n','d','e'): returnif(undefine);
#       undef returnif
    }
#   undef start
    if (dash) *line->ptr = '-';
    return dir_none;
}

void execute_command(char cref com, buf ref optout) {
    if (!optout) fprintf(stderr, "%s\n", com);
    // TODO :/
    system(com);
    if (optout) *optout = (buf){0};
}

// makefile functions {{{
void fun_abspath(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_addprefix(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_addsuffix(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_and(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_basename(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_call(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_dir(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_error(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_eval(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_file(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_filter(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

//void fun_filter-out(buf ref ret, buf cref arg) {
//    (void)ret; (void)arg;
//}

void fun_findstring(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_firstword(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_flavor(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_foreach(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_if(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_intcmp(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_join(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_lastword(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_let(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_notdir(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_or(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_origin(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_patsubst(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_realpath(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_shell(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_sort(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_strip(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_subst(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_suffix(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_value(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_warning(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_wildcard(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_word(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_wordlist(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}

void fun_words(buf ref ret, buf cref arg) {
    (void)ret; (void)arg;
}
// }}}

size_t perform_expansions(buf ref inplace, size_t const start, size_t stop) {
    size_t j;
    if (stop) for (size_t i = start; i < stop-1; '\\' == inplace->ptr[++i] ? i++ : 0) if ('$' == inplace->ptr[i]) switch (inplace->ptr[j = i+1]) {
        char c;
    case '(': c = ')'; if (0)
    case '{': c = '}';

        unsigned depth = 0;
        for (; j < stop; '\\' == inplace->ptr[++j] ? j++ : 0) {
            depth+= (inplace->ptr[i+1] == inplace->ptr[j]) - (c == inplace->ptr[j]);
            if (c == inplace->ptr[j] && !depth) break; // (yyy: approximatif, doesn't match pairs)
        }
        if (depth) exitf("Unmatched '%c%c' pair", inplace->ptr[i+1], c);
        j--;

        // fall through
    default:
        if (stop < ++j) return stop-start;

        if (inplace->len && !inplace->cap) *inplace = bufdup(inplace);

        if (3 < j-i) {
            grow(inplace, i+2, -2);
            grow(inplace, j-1, -1);
            size_t l = perform_expansions(inplace, i, j-2);
            stop+= l - (j-i);
            j = i+l;
        } else {
            grow(inplace, i+1, -1);
            stop--;
            j--;
        }

        buf repl = {0};

        if (j-i <3) switch (inplace->ptr[i]) {
        case '@': repl.len = strlen(repl.ptr = (char*)st->temps.target_name); break; // +D/F
        case '%': repl.len = strlen(repl.ptr = "archive-member"); break; // +D/F
        case '<': repl.len = strlen(repl.ptr = "first-prerequisite"); break; // +D/F
        case '?': repl.len = strlen(repl.ptr = "newer-prerequisites"); break; // +D/F
        case '^': repl.len = strlen(repl.ptr = "dedup-prerequisites"); break; // +D/F
        case '+': repl.len = strlen(repl.ptr = "asis-prerequisites"); break; // +D/F
        case '|': repl.len = strlen(repl.ptr = "order-prerequisites"); break; // ?
        case '*': repl.len = strlen(repl.ptr = "pattern-stem"); break; // +D/F

        case '$': repl.ptr = "$", repl.len = 1; break;
        default: goto var_name;
        }
#       define start(a,b) (a|b<<8)
        else switch (start(inplace->ptr[i], inplace->ptr[i+1])) {
#           define callif(__d) if (strlen(#__d) < stop-j            \
                && !memcmp(#__d, inplace->ptr+i, strlen(#__d))      \
                && (strchr("\t\n ", inplace->ptr[i+strlen(#__d)]))  \
                    ) fun_##__d(&repl, &(buf){inplace->ptr+i+strlen(#__d), j-i-strlen(#__d), 0})
        case start('a','b'): callif(abspath);    break;
        case start('a','d'): callif(addprefix);  else
                             callif(addsuffix);  else goto var_name;
                                                 break;
        case start('a','n'): callif(and);        break;
        case start('b','a'): callif(basename);   break;
        case start('c','a'): callif(call);       break;
        case start('d','i'): callif(dir);        break;
        case start('e','r'): callif(error);      break;
        case start('e','v'): callif(eval);       break;
        case start('f','i'): callif(file);       else
                             callif(filter);     else
                             //callif(filter-out); else
                             callif(findstring); else
                             callif(firstword);  else goto var_name;
                                                 break;
        case start('f','l'): callif(flavor);     break;
        case start('f','o'): callif(foreach);    break;
        case start('i','f'): callif(if);         break;
        case start('i','n'): callif(intcmp);     break;
        case start('j','o'): callif(join);       break;
        case start('l','a'): callif(lastword);   break;
        case start('l','e'): callif(let);        break;
        case start('n','o'): callif(notdir);     break;
        case start('o','r'): callif(or);         else
                             callif(origin);     else goto var_name;
                                                 break;
        case start('p','a'): callif(patsubst);   break;
        case start('r','e'): callif(realpath);   break;
        case start('s','h'): callif(shell);      break;
        case start('s','o'): callif(sort);       break;
        case start('s','t'): callif(strip);      break;
        case start('s','u'): callif(subst);      else
                             callif(suffix);     else goto var_name;
                                                 break;
        case start('v','a'): callif(value);      break;
        case start('w','a'): callif(warning);    break;
        case start('w','i'): callif(wildcard);   break;
        case start('w','o'): callif(word);       else
                             callif(wordlist);   else
                             callif(words);      else goto var_name;
                                                 break;
#           undef callif

        default:
        var_name:
            for (size_t k = 0; k < st->vars.len; k++) if (!memcmp(inplace->ptr+i, st->vars.ptr[k].name, j-i)) {
                repl.ptr = st->vars.ptr[k].val.ptr;
                repl.len = st->vars.ptr[k].val.len;
                perform_expansions(&repl, 0, repl.len);
                break;
            }
        }
#       undef start

        size_t const d = repl.len - (j-i);
        grow(inplace, j, d);
        memcpy(inplace->ptr+i, repl.ptr, repl.len);
        frry(&repl);
        *push(inplace) = '\0', inplace->len--;
        stop+= d;
        i+= repl.len-1;
    }

    return stop-start;
}

// top level: (<name> is all above ' ' but ":#=")
// <rule> ::= <target> ':' {<prerequisites>|<assignment>} [';' <recipe-line>] {<ln> <tab> <recipe-line>}
// <assignment> ::= ['export'|'unexport'|'override'|'private'] <name>
//                          ( '='         # recursive (verbatim)
//                          | ':='|'::='  # simple/immediate
//                          | '+='        # appending
//                          | '?='        # conditional
//                          ) <value>
// <conditional> ::
//    = ('ifeq'|'ifneq') ('('<lhs>','<rhs>')'|<sameideabutwithquotes>)
//    | ('ifdef'|'ifndef') <name>
// <other-directives> ::
//    = ('include'|'-include') <names>
//    | ['override'] 'undefine' <name>
//    | ['export'|'unexport'] <names>
// <define> ::= 'define' <name> <blabla> 'endef'

void update_variable(buf line, size_t eq, enum directive flag) {
    while (flag) {
        size_t const l = strcspn(line.ptr+(flag&0xf), "\n\t ")+(flag&0xf);
        grow(&line, l, -l);
        eq-= l;
        flag = scan_directive(&line);
    }

    if ('=' == line.ptr[eq]) {
        char const c = line.ptr[eq-1];
        if (':' == c) perform_expansions(&line, eq, line.len);

        for (size_t l = line.len-1; eq < l; l--) if (!strchr("\t\n ", line.ptr[l])) {
            l+= '\\' == line.ptr[l];
            line.ptr[line.len = l+1] = '\0';
            break;
        }

        for (size_t l = eq-2; l; l--) if (!strchr("\t\n ", line.ptr[l])) {
            l+= '\\' == line.ptr[l];
            line.ptr[l+1] = '\0';
            break;
        }

        size_t const n = strspn(line.ptr+eq+1, "\t\n ")+1;
        buf val = {line.ptr+eq+n, line.len-eq-n, 0};

        size_t k;
        for (k = 0; k < st->vars.len; k++) if (!strcmp(line.ptr, st->vars.ptr[k].name)) break;

        if (k < st->vars.len) switch (c) {
        case '+':
            val = bufdup(&val);
            buf cref pval = &st->vars.ptr[k].val;
            memcpy(grow(&val, 0, pval->len), pval->ptr, pval->len);
            // fall through
        default:
            frry(&st->vars.ptr[k].val);
            st->vars.ptr[k].val = val;
            // fall through
        case '?':
            return;
        }

        push(&st->vars)->name = line.ptr;
        last(&st->vars)->val = val;
    } // TODO: else export/unexport
}

void process_makefile(buf slice) {
    size_t lnum = 0;
    buf line;
    while ((line = next_line(&slice, &lnum)).len) {
        if ('\t' == *line.ptr) {
            if (!st->rules.len) exitf("No rule defined at this point line %zu", lnum);
            *push(&last(&st->rules)->recipe) = line;
            continue;
        }

        enum directive dir;
        switch (dir = scan_directive(&line)) {
        case dir_none:
        case dir_export:
        case dir_unexport:
        case dir_override:
        case dir_private:
            {
                size_t const sep = strcspn(line.ptr, ":=");
                if (line.len == sep && !(
                            dir_export == dir ||
                            dir_unexport == dir ))
                    exitf("Missing separator at line %zu", lnum);
                perform_expansions(&line, 0, sep);

                if (':' == line.ptr[sep] && '=' != line.ptr[sep+1]) {
                    perform_expansions(&line, sep+1, line.len);

                    for (size_t l = sep-1; l; l--) if (!strchr("\t\n ", line.ptr[l])) {
                        l+= '\\' == line.ptr[l];
                        line.ptr[l+1] = '\0';
                        break;
                    }

                    memset(push(&st->rules), 0, sizeof*st->rules.ptr);
                    last(&st->rules)->name = line.ptr;

                    char ref semi = memchr(line.ptr+sep+1, ';', line.len-sep-1);
                    if (semi) {
                        size_t const d = semi-line.ptr-sep-1;
                        *push(&last(&st->rules)->recipe) = (buf){semi+1, line.len-sep-1-d-1, 0};
                        line.len = d+sep+1;
                        *semi = '\0';
                    }

                    line.ptr+= sep+1, line.len-= sep+1;
                    lns deps = word_slices(&line);
                    // TODO: local assignments
                    last(&st->rules)->prereqs = deps;
                } else update_variable(line, sep + (':' == line.ptr[sep]), dir);
            } break;

        case dir_ifeq:
        case dir_ifneq:
        case dir_ifdef:
        case dir_ifndef:
        case dir_endif:

        case dir_include:
        case dir_tinclude:

        case dir_define:
        case dir_endef:
        case dir_undefine:
            //notif("NIY: directive '%.*s'", dir&0xf, line.ptr);
            free(line.ptr);
            break;
        }
    } // while line
}

void apply_rule(char cref name) {
    // TODO: patterns
    // TODO: most fitting rule is not always the first fit
    for (size_t k = 0; k < st->rules.len; k++) if (!strcmp(name, st->rules.ptr[k].name)) {
        lns cref deps = &st->rules.ptr[k].prereqs;
        for (size_t l = 0; l < deps->len; l++) apply_rule(deps->ptr[l]);
        for (size_t l = 0; l < st->rules.ptr[k].recipe.len; l++) {
            st->temps.target_name = name;
            st->temps.target_prereqs = deps;
            // TODO: gather local assignments
            buf cref it = st->rules.ptr[k].recipe.ptr+l;
            buf cow = (buf){it->ptr+1, it->len-1, 0};
            perform_expansions(&cow, 0, cow.len);
            execute_command(cow.ptr, NULL);
            frry(&cow);
            st->temps.target_name = NULL;
            st->temps.target_prereqs = NULL;
        }
        return;
    }
    exitf("No rule for target '%s'", name);
}

int main(int argc, char** argv) {
    for (char cref prog = *argv; --argc; ) if ('-' == **++argv) switch ((*argv)[1]) {
#       define argis(__s) (!memcmp(__s, *argv, strlen(__s)))
    case '-':
        if (argis("--help"))
    case 'h':
            exitf("Usage: %s [targets...]\n"
                  "   -B, --always-make\n"
                  "   -C DIRECTORY, --directory=DIRECTORY\n"
                  "   -f FILE, --file=FILE, --makefile=FILE\n"
                  "   -h, --help\n"
                  "   -i, --ignore-errors\n"
                  "   -I DIRECTORY, --include-dir=DIRECTORY\n"
                  "   -k, --keep-going\n"
                  "   -n, --just-print, --dry-run, --recon\n"
                  "   -q, --question\n"
                  "   -s, --silent, --quiet\n"
                  "   --no-silent\n"
                  "   -S, --no-keep-going, --stop\n"
                  "   -w, --print-directory\n"
                  "   --no-print-directory\n"
                  "   --warn-undefined-variables\n"
                  , prog);
        else if (argis("--directory=")) {
            *argv+= 12;
            if (0) { case 'C': if ((*argv)[2]) *argv+= 2; else if (argv++, !--argc) exitf("Missing directory path argument"); }
            if (chdir(*argv)) exitf("Could not change directory");
        }
        else if (argis("--file=") || argis("--makefile=")) {
            *argv+= 'm' == (*argv)[2] ? 11 : 7;
            if (0) { case 'f': if ((*argv)[2]) *argv+= 2; else if (argv++, !--argc) exitf("Missing file name argument"); }
            st->file.path = *argv;
        }
        else if (argis("--include-dir=")) {
            *argv+= 14;
            if (0) { case 'I': if ((*argv)[2]) *argv+= 2; else if (argv++, !--argc) exitf("Missing include directory argument"); }
            //*push(&st->incdirs) = *argv;
        }
        else if (argis("--always-make"))                                          { case 'B': st->flags.always_make = 1;              break; }
        else if (argis("--ignore-errors"))                                        { case 'i': st->flags.ignore_errors = 1;            break; }
        else if (argis("--keep-going"))                                           { case 'k': st->flags.keep_going = 1;               break; }
        else if (argis("--no-keep-going"))                                        { case 'S': st->flags.keep_going = 0;               break; }
        else if (argis("--just-print") || argis("--dry-run") || argis("--recon")) { case 'n': st->flags.dry_run = 1;                  break; }
        else if (argis("--question"))                                             { case 'q': st->flags.question = 1;                 break; }
        else if (argis("--silent") || argis("--quiet"))                           { case 's': st->flags.silent = 1;                   break; }
        else if (argis("--no-silent"))                                            {           st->flags.silent = 0;                   break; }
        else if (argis("--print-directory"))                                      { case 'w': st->flags.print_directory = 1;          break; }
        else if (argis("--no-print-directory"))                                   {           st->flags.print_directory = 0;          break; }
        else if (argis("--warn-undefined-variables"))                             {           st->flags.warn_undefined_variables = 1; break; }
        else // fall through
    default:
            exitf("Unknown argument '%s'", *argv);
#       undef argis
    } else {
        char* eq = strchr(*argv, '=');
        if (eq) {
            push(&st->vars)->name = *argv;
            last(&st->vars)->val = (*eq++ = '\0', (buf){eq, strlen(eq), 0});
        } else *push(&st->goals) = *argv;
    }

    FILE* f = NULL;
    if (st->file.path) {
        if (!(f = strcmp("-", st->file.path) ? fopen(st->file.path, "r") : stdin))
            exitf("Could not open file '%s'", st->file.path);
    } else {
        f = fopen(st->file.path = "makefile", "r");
        if (!f) f = fopen(st->file.path = "Makefile", "r");
    }
    if (!f) exitf("No makefile");
    process_makefile(st->file.text = read_all(f));

    if (!st->rules.len) exitf("No rule to make");
    if (!st->goals.len) *push(&st->goals) = st->rules.ptr[0].name;
    for (size_t k = 0; k < st->goals.len; k++) apply_rule(st->goals.ptr[k]);

    return EXIT_SUCCESS;
}

#define END
END
