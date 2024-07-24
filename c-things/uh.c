#define _DEFAULT_SOURCE // sigaction

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h> // read/write

// arry {{{
#define arry(...) struct { __VA_ARGS__* ptr; size_t len, cap; }
#define frry(__a) (free((__a)->cap ? (__a)->ptr : NULL), (__a)->ptr = NULL, (__a)->len = (__a)->cap = 0)
#define push(__a) ((__a)->len < (__a)->cap || (((__a)->cap || !(__a)->ptr) && ((__a)->ptr = realloc((__a)->ptr, ((__a)->cap+= (__a)->cap+8)*sizeof*(__a)->ptr))) ? (__a)->ptr+(__a)->len++ : (exit(EXIT_FAILURE), (__a)->ptr))
#define grow(__a, __k, __n) _arrygrow((char**)&(__a)->ptr, &(__a)->len, &(__a)->cap, sizeof*(__a)->ptr, (__k), (__n))
#define last(__a) ((__a)->ptr+(__a)->len-1)
#define each(__a) for (void* const _ptr = (__a)->ptr,* const _end = (__a)->ptr+(__a)->len; _end == (__a)->ptr ? (__a)->ptr = _ptr, 0 : 1; ++(__a)->ptr)
static char* _arrygrow(char** const ptr, size_t* const len, size_t* const cap, size_t const s, size_t const k, size_t const n) {
    if (!n) return *ptr+k*s;
    size_t const nlen = *len+n;
    if (*cap < nlen && !(cap && (*ptr = realloc(*ptr, (*cap = nlen)*s)))) exit(EXIT_FAILURE);
    if (k < *len) memmove(*ptr+(k+n)*s, *ptr+k*s, (*len-k)*s);
    return *len = nlen, *ptr+k*s;
}
// }}}

#define term stdout

// uses/requires termios {{{
#define TERM STDOUT_FILENO
struct termios ini_tios, raw_tios;
#define set_ini_tios() tcsetattr(TERM, TCSANOW, &ini_tios)
#define set_raw_tios() tcsetattr(TERM, TCSANOW, &raw_tios)

bool setup_tios(void) {
    if (tcgetattr(TERM, &ini_tios)) return false;
    raw_tios = ini_tios;
    raw_tios.c_iflag&=~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw_tios.c_oflag&=~OPOST;
    raw_tios.c_lflag&=~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw_tios.c_cflag&=~(CSIZE | PARENB);
    raw_tios.c_cflag|= CS8;
    return true;
}
// }}}

// uses/requires sigaction {{{
void signaled(int const signum);
#define caught_sigs(__do) __do(SIGINT)    \
                          __do(SIGWINCH)
                        //__do(SIGCONT) ?

bool setup_sigs(void) {
    struct sigaction sa;
    sa.sa_handler = signaled;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
#define __do(__s) sigaction(__s, &sa, NULL);
    caught_sigs(__do)
#undef __do
    return true;
}
// }}}

// state definition bits {{{
struct _IOTA_NALPH_MARK { char _[__LINE__-('z'-'a'+1) +3]; }; // +3: the 3 next lines
size_t char_to_mark(char const c) {
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) return (c|32)-'a'+1;
    switch (c) {
    case '.': return __LINE__-sizeof(struct _IOTA_NALPH_MARK);
    case '(': return __LINE__-sizeof(struct _IOTA_NALPH_MARK);
    case ')': return __LINE__-sizeof(struct _IOTA_NALPH_MARK);
    case'\'': return __LINE__-sizeof(struct _IOTA_NALPH_MARK);
    case '[': return __LINE__-sizeof(struct _IOTA_NALPH_MARK);
    case ']': return __LINE__-sizeof(struct _IOTA_NALPH_MARK);
    }
    return 0;
}
struct _NB_MARKS { char _[__LINE__-sizeof(struct _IOTA_NALPH_MARK) -3];}; // -3: the 3 previous lines
#define NB_MARKS (sizeof(struct _NB_MARKS))

struct state {
    arry(struct buf {
        char const* name;
        arry(arry(char)) text;
        struct mark { size_t ln, ch; } marks[NB_MARKS];

        struct { unsigned
            exists : 1,
            modified : 1,
            readonly : 1;
        } flags;
    }) bufs;

    size_t vis_buf;
    arry(char) typeahead;

    short unsigned rows, cols;
} uh = {0};

#define visible (uh.bufs.ptr+uh.vis_buf)
#define markk(__c) (visible->marks+char_to_mark(__c))
#define cursor markk('.')
// }}}

// term/cursor functions (sig unsafe, 0-based) {{{
void cu_goto(short unsigned row, short unsigned col) {
    fprintf(term, "\x1b[%hu;%huH", row+1, col+1);
}
// }}}

// somewhat misc {{{
void cleanup(void) {
    each (&uh.bufs) {
        each (&uh.bufs.ptr->text) frry(uh.bufs.ptr->text.ptr);
        frry(&uh.bufs.ptr->text);
    }
    frry(&uh.bufs);

    frry(&uh.typeahead);
    struct _ { char exhaustive_cleanup_check[64 == sizeof uh ? 1 : -1]; };

    set_ini_tios();
}

void signaled(int const signum) {
#define __do(__s) _##__s= __s,
    switch ((enum { caught_sigs(__do) })signum) {
#undef __do
    case _SIGINT:
        set_ini_tios();
        exit(EXIT_FAILURE);
        break;

    case _SIGWINCH:
        uh.rows = uh.cols = 0;
        write(TERM, "\x1b[9999;9999H\x1b[6n", 16);
        char csi[2], c;
        read(TERM, &csi, 2);
        while (read(TERM, &c, 1), ';' != c) uh.rows = uh.rows*10 + c-'0';
        while (read(TERM, &c, 1), 'R' != c) uh.cols = uh.cols*10 + c-'0';
        break;
    }
}

char getk(void) {
    char r;
    if (uh.typeahead.len) {
        r = *uh.typeahead.ptr;
        grow(&uh.typeahead, 1, -1);
    } else read(TERM, &r, 1);
    return r;
}

bool loadbuf(char const* const path, struct buf* const res) {
    *res = (struct buf){.name= path};
    FILE* const f = fopen(path, "rb");
    if (!f) return 0;
    size_t alen;
    char* aptr;
    if (0 == fseek(f, 0, SEEK_END)) {
        aptr = malloc(alen = ftell(f));
        if (!aptr) return fclose(f), false;
        fseek(f, 0, SEEK_SET);
        fread(aptr, 1, alen, f);
        fclose(f);
    } else {
        // TODO: eg pipe input
        return fclose(f), false;
    }

    size_t last = 0;
    for (size_t k = 0; k < alen; k++) {
        if ('\n' == aptr[k]) {
            size_t const llen = k-last;
            if (!llen) {
                memset(push(&res->text), 0, sizeof*res->text.ptr);
                last++;
                continue;
            }

            char* const lptr = push(&res->text)->ptr = malloc(llen);
            if (!lptr) {
                last(&res->text)->len = last(&res->text)->cap = 0;
                break;
            }

            memcpy(lptr, aptr+last, last(&res->text)->len = last(&res->text)->cap = llen);
            last = k+1;
        }
    }

    return free(aptr), true;
}
// }}}

int main(int argc, char** argv) {
    (void)(argc--, argv++);

    if (!setup_sigs() || !setup_tios()) return EXIT_FAILURE;
    set_raw_tios();
    atexit(cleanup);

    signaled(SIGWINCH);

    struct buf* const land = push(&uh.bufs);
    if (argc && loadbuf((argc--, *argv++), land))
        land->flags.exists = true;

    {
        cu_goto(0, 0);
        unsigned gutter_width = 5; // "%4zu "

        size_t const first = markk('(')->ln, last = visible->text.len;
        size_t k_scr, k_txt;
        for (k_scr = k_txt = 0; k_scr+2 < uh.rows; k_scr++) {
            if (last <= ++k_txt) break;
            size_t const full = visible->text.ptr[k_txt].len;
            unsigned const fits = full < uh.cols-gutter_width ? full : uh.cols-gutter_width;
            fprintf(term, "%4zu %.*s\r\n", k_scr+1, fits, visible->text.ptr[k_txt].ptr);
        }
        markk(')')->ln = first+k_scr;

        fprintf(term, "%s%s %s%s -- %zu/%zu,%zu (%hux%hu)\r\n",
                visible->flags.exists ? "" : "[NEW]",
                visible->flags.readonly ? "[RO]" : "",
                visible->flags.modified ? "+" : "",
                visible->name ? visible->name : "[Scratch]",
                cursor->ln, visible->text.len, cursor->ch,
                uh.rows, uh.cols);

        cu_goto(cursor->ln, cursor->ch+gutter_width);
        //cu_goto(0, 0);
    }

    {
        bool done = false;
        while (!done) switch (fflush(term), getk()) {
            case 'h': --cursor->ch; fprintf(term, "\x1b[D"); break;
            case 'j': ++cursor->ln; fprintf(term, "\x1b[B"); break;
            case 'k': --cursor->ln; fprintf(term, "\x1b[A"); break;
            case 'l': ++cursor->ch; fprintf(term, "\x1b[C"); break;
            case 'q': done = true; break;

            default: fputc('\a', term);
        }
    }

    return EXIT_SUCCESS;
}
