#define _DEFAULT_SOURCE // sigaction

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h> // read/write

#define countof(__a) (sizeof(__a)/sizeof*(__a))

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

struct {
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
    arry(char) typeahead; // TODO: make it a reg

    char* regs[127]; // TODO: make it tighter

    short unsigned rows, cols;

    arry(struct state {
        enum {
            STARTING,
            QUITTING,            // extra: status
            NEED_RESIZE,
            NEED_REDRAW,
            NEED_RESTATUS,
            NEED_REPOS,
            USER_PENDING,
            USER_READLINE,       // extra: char
        } tag;
        size_t extra;
    }) states;
} uh = {0};

#define visible (uh.bufs.ptr+uh.vis_buf)
#define markk(__c) (visible->marks+char_to_mark(__c))
#define cursor markk('.')

#define cstate last(&uh.states)
#define understate(__s, __x) memcpy(grow(&uh.states, 0, 1), &(struct state){(__s), (__x)}, sizeof(struct state))
#define abovestate(__s, __x) (*push(&uh.states) = (struct state){(__s), (__x)})
#define popstate() (uh.states.ptr[--uh.states.len])
#define replstate(__s, __x) (*cstate = (struct state){(__s), (__x)})
// }}}

// term/cursor functions {{{
#define cu_enter_alt() fprintf(term, "\x1b[?1049h")
#define cu_leave_alt() fprintf(term, "\x1b[?1049l")

void cu_goto(short unsigned row, short unsigned col) {
    fprintf(term, "\x1b[%hu;%huH", row+1, col+1);
}

void repos_cursor(void) {
    static unsigned const gutter_width = 5; // "%4zu "
    cu_goto(cursor->ln, cursor->ch+gutter_width);
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
    for (size_t k = 0; k < countof(uh.regs); ++k) free(uh.regs[k]);
    struct _ { char exhaustive_cleanup_check[1104 == sizeof uh ? 1 : -1]; };

    cu_leave_alt();
    fputc('\r', term);
    fflush(term);
    set_ini_tios();
}

void signaled(int const signum) {
#define __do(__s) _##__s= __s,
    switch ((enum { caught_sigs(__do) })signum) {
#undef __do
    case _SIGINT:   replstate(QUITTING, 2);     break;
    case _SIGWINCH: abovestate(NEED_RESIZE, 0); break;
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
    FILE* const f = fopen(path, "r");
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

bool move_prev_char(bool* const nl) {
    size_t *const ln = &cursor->ln, *const ch = &cursor->ch;
    *nl = false;
    if (!(*ch)--) {
        if (!(*ln)--) {
            *ch = *ln = 0;
            return false;
        }
        *ch = visible->text.ptr[*ln].len-1;
        *nl = true;
    }
    return true;
}

bool move_next_char(bool* const nl) {
    size_t *const ln = &cursor->ln, *const ch = &cursor->ch;
    *nl = false;
    if (visible->text.ptr[*ln].len <= ++*ch) {
        if (visible->text.len <= ++*ln) {
            --*ch, --*ln;
            return false;
        }
        *ch = 0;
        *nl = true;
    }
    return true;
}
// }}}

// line editing {{{
void readline(char const r) {
    fprintf(term, "%c%s", r, uh.regs[(int)r] ? uh.regs[(int)r] : "");

    size_t at = 0;
    arry(char) edit = {0};
    if (uh.regs[(int)r]) {
        edit.len = edit.cap = (at = strlen(edit.ptr = uh.regs[(int)r]))+1;
        uh.regs[(int)r] = NULL;
    } else *push(&edit) = '\0';

    char c;
    while (1) switch (fflush(term), c = getk()) default: {
        if (' ' <= c && c <= '~') {
            if (edit.len-1 == at++) fputc(push(&edit)[-1] = c, term), *last(&edit) = '\0';
            else fprintf(term, "\x1b[@%c", *grow(&edit, at-1, 1) = c);
        }

        if (0) case CTRL('A'): fprintf(term, "\r%c", r), at = 0;
        if (0) case CTRL('B'): if (at) --at, fputc('\b', term);
        if (0) case CTRL('C'): { frry(&edit); return; }
        if (0) case CTRL('D'): if (1 < edit.len && at < edit.len-2) grow(&edit, at+1, -1), fprintf(term, "\x1b[P");
        if (0) case CTRL('E'): if (at < edit.len-1) fprintf(term, "\x1b[%dC", (unsigned)(edit.len-1-at)), at = edit.len-1;
        if (0) case CTRL('F'): if (at < edit.len-1) fputc(edit.ptr[at++], term);

        if (0) case CTRL('H'): case 127: if (at) {
            if (edit.len-1 == at--) edit.ptr[--edit.len-1] = '\0';
            else grow(&edit, at+1, -1);
            fprintf(term, "\b\x1b[P");
        }

        //if (0) case CTRL('I'): { } // compl?

        if (0) case CTRL('J'): case CTRL('M'): { uh.regs[(int)r] = edit.ptr; return; }

        if (0) case CTRL('K'): if (at < edit.len-1) fprintf(term, "\x1b[K"), edit.ptr[edit.len = at] = '\0';
        //if (0) case CTRL('L'): { } // dig? if not on ^K...

        //if (0) case CTRL('N'): { }
        //if (0) case CTRL('O'): { }
        //if (0) case CTRL('P'): { }

        if (0) case CTRL('R'): {
            switch (c = getk()) default: {
                if (uh.regs[(int)c]) {
                    size_t const len = strlen(uh.regs[(int)c]);
                    memcpy(grow(&edit, at, len), uh.regs[(int)c], len);
                }

                //if (0) case '=': {
                //    fprintf(term, "\r\x1b[K");
                //    readline(rr);
                //    // exec
                //    // reprint
                //    // reposition
                //    // insert as below
                //}
            }
        }

        if (0) case CTRL('U'): if (at) grow(&edit, at, -at), fprintf(term, "\x1b[%uD\x1b[%uP", (unsigned)at, (unsigned)at), at = 0;

        if (0) case CTRL('['): switch (c = getk()) default: {

            if (0) case CTRL('['): { frry(&edit); return; }
        }
    }
}
// }}}

// switch key functions {{{
bool movment_key(char const key) {
    size_t *const ln = &cursor->ln, *const ch = &cursor->ch;
#   define mln (visible->text.len)
#   define mch (visible->text.ptr[*ln].len)
#   define atc (visible->text.ptr[*ln].ptr[*ch])

    switch (key) default: {
        return false;

        if (0) {
            if (0) case 'h': if (*ch)         --*ch;
            if (0) case 'j': if (*ln < mln-1) ++*ln;
            if (0) case 'k': if (*ln)         --*ln;
            if (0) case 'l': if (*ch < mch-1) ++*ch;

            if (mch <= *ch)
                *ch = mch ? mch-1 : 0;
        }

#       define wordchar(__c) ('_' == (__c) ||  \
            ('0' <= (__c) && (__c) <= '9') ||  \
            ('A' <= (__c) && (__c) <= 'Z') ||  \
            ('a' <= (__c) && (__c) <= 'z') )
#       define blankchar(__c) (' ' == (__c) || '\t' == (__c))

        if (0) {
            bool nl;

            if (0) case 'w': case 'W': {
                if (mch && !blankchar(atc)) {
                    bool const firstwc = wordchar(atc);
                    while (move_next_char(&nl) && !nl && (!(key&32) || firstwc == wordchar(atc)) && !blankchar(atc));
                } else nl = !mch;
                bool atend = false;
                if (nl)
                    do if ((atend = mln-1 == *ln)) break;
                    while (++*ln, !mch);
                if (!atend) while (blankchar(atc) && move_next_char(&nl) && nl);
            }

            if (0) case 'e': case 'E': {
                nl = false;
                while ((nl ? nl = false, true : move_next_char(&nl)) && (nl || blankchar(atc)));
                bool const firstwc = wordchar(atc);
                bool outnotmv;
                while ((outnotmv = move_next_char(&nl)) && !nl && (key&32 ? firstwc == wordchar(atc) : !blankchar(atc)));
                if (outnotmv) move_prev_char(&nl);
            }

            if (0) case 'b': case 'B': {
                nl = false;
                while ((nl ? nl = false, true : move_prev_char(&nl)) && (nl || blankchar(atc)));
                if (nl) move_next_char(&nl);
                bool const firstwc = wordchar(atc);
                bool outnotmv;
                while ((outnotmv = move_prev_char(&nl)) && !nl && (key&32 ? firstwc == wordchar(atc) : !blankchar(atc)));
                if (outnotmv) move_next_char(&nl);
            }
        }

        if (0) {
            bool nl;
            if (0) case ' ':                 move_next_char(&nl);
            if (0) case 127: case CTRL('H'): move_prev_char(&nl);
        }

        if (0) case '0': *ch = 0;
        if (0) case '$': *ch = visible->text.ptr[*ln].len-1;
        if (0) case '^': case '_': for (*ch = 0; *ch < mch && blankchar(atc); ++*ch);

#       undef wordchar
#       undef blankchar

    }

#   undef mch
#   undef mln
#   undef atc

    return true;
}

bool normal_key(char const key) {
    switch (key) default: {
        if (movment_key(key)) {
            abovestate(NEED_RESTATUS, 0);
            return true;
        }
        return false;

        if (0) {
            if (0) case 'c': ;
            if (0) case 'd': ;
            if (0) case 'y': ;

            if (!movment_key(getk())) return false;
            // TODO: resolve or smth

            switch (key) {
            case 'c':
                cu_goto(uh.rows-1, 0);
                readline('.');
                break; // TODO
            case 'd':
                break;
            case 'y':
                break; // TODO
            }
        }

        if (0) {
        case CTRL('Z'): kill(0, SIGTSTP);
        case CTRL('L'): abovestate(NEED_REDRAW, 0);
        }

        if (0) case CTRL('C'): replstate(QUITTING, 0);

        if (0) {
            if (0) case CTRL('E'): if (markk('(')->ln < visible->text.len-1) {
                ++markk('(')->ln;
                fprintf(term, "\x1b[S\x1b[A");
            }
            if (0) case CTRL('Y'): if (markk('(')->ln) {
                --markk('(')->ln;
                fprintf(term, "\x1b[T\n");
            }
            abovestate(NEED_REPOS, 0);
        }

        if (0) {
        case ':': case '/': case '?': abovestate(USER_READLINE, key);
        }
    }

    return true;
}
// }}}

// redraw functions {{{
void redraw_status(void) {
    cu_goto(uh.rows-2, 0);

    char posinfo[64];
    int const posinfo_len = sprintf(posinfo, "%zu/%zu,%zu",
            cursor->ln+1, visible->text.len, cursor->ch+1);
    int const nameinfo_len = fprintf(term, "%s%s%s%s",
            visible->name ? visible->name : "[Scratch]",
            visible->flags.modified ? "+" : "",
            visible->flags.exists ? "" : "[NEW]",
            visible->flags.readonly ? "[RO]" : "");
    fprintf(term, "%*s%s\r\n",
            uh.cols-nameinfo_len-posinfo_len, "",
            posinfo);
}

void redraw_screen(void) {
    static unsigned const gutter_width = 5; // "%4zu "

    cu_goto(0, 0);

    size_t const top = markk('(')->ln, last = visible->text.len;
    size_t k_scr, k_txt;
    for (k_scr = k_txt = 0; k_scr+2 < uh.rows; k_scr++) {
        if (last <= k_txt) break;
        size_t const full = visible->text.ptr[k_txt].len;
        unsigned const fits = full < uh.cols-gutter_width ? full : uh.cols-gutter_width;
        fprintf(term, "%4zu %.*s\r\n", k_scr+1, fits, visible->text.ptr[k_txt].ptr);
        ++k_txt;
    }
    size_t const bot = markk(')')->ln = top+k_scr;

    if (cursor->ln < top) cursor->ln = top+2;
    if (bot < cursor->ln) cursor->ln = bot-2;

    while (k_scr+2 < uh.rows) fprintf(term, "%4zu~\r\n", ++k_scr);
}
// }}}

int main(int argc, char** argv) {
    (void)(argc--, argv++);

    if (!setup_sigs() || !setup_tios()) return EXIT_FAILURE;
    set_raw_tios();
    cu_enter_alt();
    atexit(cleanup);

    understate(STARTING, 0);

    while (1) switch (cstate->tag) default: {
        replstate(QUITTING, 3);

        if (0) case STARTING: {
            struct buf* const land = push(&uh.bufs);
            if (argc && loadbuf((argc--, *argv++), land)) land->flags.exists = true;

            popstate();
            abovestate(USER_PENDING, 0);
            abovestate(NEED_RESIZE, 0);
        }

        if (0) case QUITTING: {
            exit(cstate->extra);
        }

        if (0) case NEED_RESIZE: {
            uh.rows = uh.cols = 0;
            write(TERM, "\x1b[9999;9999H\x1b[6n", 16);
            char csi[2], c;
            read(TERM, &csi, 2);
            while (read(TERM, &c, 1), ';' != c) uh.rows = uh.rows*10 + c-'0';
            while (read(TERM, &c, 1), 'R' != c) uh.cols = uh.cols*10 + c-'0';

        case NEED_REDRAW:   redraw_screen();
        case NEED_RESTATUS: redraw_status();
        case NEED_REPOS:    repos_cursor();
            popstate();
        }

        if (0) case USER_PENDING: {
            fflush(term);
            normal_key(getk());
        }

        if (0) case USER_READLINE: {
            cu_goto(uh.rows-1, 0);
            readline(cstate->extra);
            fprintf(term, "[[!!%s!!]]", uh.regs[(int)cstate->extra]);
            popstate();
        }
    }

    // (unreachable)
    return EXIT_SUCCESS;
}
