/**
 * A small `readline`-like with only '\b', in the most unhinged free-form C.
 * ```c
 * char* line;
 * while (printf(">> "), line = line_read()) {
 *     printf("echo: '%s'\n", line);
 * }
 * line_free();
 * ```
 *
 * Notes: ^D when empty returns NULL, but ^C returns an empty line ("").
 *
 * To access the history use `line_histget` and `line_histset`.
 * For completion see `line_compgen`.
 *
 * The size of the history is fixed at compile-time; the default value is 128
 * entries, this can by changed by defining `_hist_ln`.
 *
 * Make sure to only use LINE_IMPLEMENTATION once.
 *
 * Define line_getchar/line_putchar and line_pause/line_resume to retarget.
 *
 * Defaults:
 * ```c
 * #define line_fileno STDIN_FILENO
 * #define line_getchar getchar
 * #define line_putchar putchar
 * #define line_ready() tcsetattr(line_fileno, TCSANOW, term_raw)
 * #define line_reset() tcsetattr(line_fileno, TCSANOW, term_good)
 * #define line_pause() kill(0, SIGTSTP)
 * ```
 *
 * If retagetting by changing these values, it may also be preferable to use
 * line_read_raw instead. line_read is a simple wrapper around it that
 * expects the target to be a terminal in "cooked" state and handles
 * line_fileno not being a terminal (typically a pipe). So it will always call
 * to tcgetattr.
 * Defining line_ready, line_pause and line_reset to no-op will essentially
 * disable ^Z (background process group for shell job control).
 */

/* TODO: see if needed
    #if _WIN32
    / * Raw input mode * /
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_PROCESSED_INPUT);
    / * Enable ANSI/VT sequences on windows * /
    HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output_handle != INVALID_HANDLE_VALUE) {
        DWORD old_mode;
        if (GetConsoleMode(output_handle, &old_mode)) {
            SetConsoleMode(output_handle, old_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    #endif
*/

char* line_read(void);
/* terminal (or other) is already in "raw" mode */
char* line_read_raw(void);
/* this is a direct access to the history (careful with editing it) */
char** line_histget(unsigned* count);
/* the lines are copied using the default allocator */
void line_histset(char** lines, unsigned count);
/* always call this after at least one `line_read` */
void line_free(void);
/* `words` should return a NULL-terminated list of completions insertable as-is */
/* `clean` can be NULL, otherwise it is called with the result afterward */
void line_compgen(char** (*words)(char* line, unsigned point), void (*clean)(char** words));

#if 1 /*def LINE_IMPLEMENTATION */
#include <stdlib.h>
#include <string.h>

#ifndef line_fileno
# include <unistd.h>
# define line_fileno STDIN_FILENO
#endif
#if !defined line_getchar || !defined line_putchar
# include <stdio.h>
# define line_getchar getchar
# define line_putchar putchar
#endif
#if !defined line_ready || !defined line_reset
# include <termios.h>
# define line_ready() tcsetattr(line_fileno, TCSANOW, &term_raw)
# define line_reset() tcsetattr(line_fileno, TCSANOW, &term_good)
# define HAS_TERMIOS
#endif
#ifndef line_pause
# include <signal.h>
# define line_pause() kill(0, SIGTSTP)
#endif

static char** (*_compgen_words)(char* line, unsigned point) = NULL;
static void (*_compgen_clean)(char** words) = NULL;

#ifndef _hist_ln
#define _hist_ln (128)
#endif
static char* _hist_ls[_hist_ln] = {0};
static unsigned _hist_at = 0;

#undef CTRL
#define CTRL(x) (x&31)
#define ESC CTRL('[')
#define DEL 0x7f

#define IS_LETTER(__c) (('A' <= __c && __c <= 'Z') || ('a' <= __c && __c <= 'z'))
#define IS_WORD(__c) (('0' <= __c && __c <= '9') || IS_LETTER(__c))

#define WORD_BKW(__does) do {        \
        while (0 < i) {              \
            if (!IS_WORD(s[i-1])) {  \
                i--;                 \
                __does;              \
            } else break;            \
        }                            \
        while (0 < i) {              \
            if (IS_WORD(s[i-1])) {   \
                i--;                 \
                __does;              \
            } else break;            \
        }                            \
    } while (0)

#define WORD_FWD(__does) do {      \
        while (s[i]) {             \
            if (!IS_WORD(s[i])) {  \
                __does;            \
                i++;               \
            } else break;          \
        }                          \
        while (s[i]) {             \
            if (IS_WORD(s[i])) {   \
                __does;            \
                i++;               \
            } else break;          \
        }                          \
    } while (0)

char* line_read(void) {
    char* s;
    unsigned i = 0;

#ifdef HAS_TERMIOS
    struct termios term_good, term_raw;

    if (tcgetattr(line_fileno, &term_good)) {
        static unsigned const n = 64-12;
        char c;
        s = calloc(n, 1);
        if (!s) return NULL;
        s[n-1] = ESC;

        c = line_getchar();
        if (EOF == c) return NULL;

        while (1) {
            if (EOF == c || '\n' == c) {
                s['\r' == s[i-1] ? i-1 : i] = '\0';
                break;
            }

            if (ESC == s[i]) {
                unsigned n = (i+1)*2+12;
                char* w = realloc(s, n);
                if (!w) break;
                s = w;
                memset(s+i+1, '\0', n-i-2);
                s[n-1] = ESC;
            }

            s[i++] = c;
            c = line_getchar();
        } /* while 1 - breaks on eof/eol and alloc fails */

        return free(_hist_ls[0]), _hist_ls[0] = s;
    } /* if not tty */

    term_raw = term_good;
    term_raw.c_iflag&=~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    term_raw.c_oflag&=~(OPOST);
    term_raw.c_lflag&=~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    term_raw.c_cflag&=~(CSIZE | PARENB);
    term_raw.c_cflag|= (CS8);
#endif /* HAS_TERMIOS */

    line_ready();
    s = line_read_raw();
    line_reset();

    return s;
}

char* line_read_raw() {
    char c, * s;
    unsigned i = 0;
    int reprocess = 0;

    if (_hist_at) for (s = _hist_ls[_hist_at]; s[i]; i++) line_putchar(s[i]);
    else if (!_hist_ls[0] || _hist_ls[0][0]) {
        static unsigned const n = 64-12;
        memmove(_hist_ls+1, _hist_ls, (_hist_ln-1)*sizeof(char*));
        _hist_ls[0] = s = calloc(n, 1);
        if (!s) return NULL;
        s[n-1] = ESC;
    } else s = _hist_ls[0];

    while (1) {
        if (!reprocess) c = line_getchar();
        else reprocess = 0;
        switch (c) {
            case ESC:
                switch (c = line_getchar()) {
                    case 'b':
                        WORD_BKW(line_putchar('\b'));
                        break;

                    case 'c':
                        if (s[i]) {
                            int done = 0;
                            WORD_FWD(line_putchar(IS_LETTER(s[i])
                                ? s[i] = (!done ? done = 1, s[i]&~32 : s[i]|32)
                                : s[i]));
                        }
                        break;

                    case 'd':
                        if (s[i]) {
                            unsigned j = i, k;
                            WORD_FWD((void)0);
                            for (k = j; s[i]; k++, i++) line_putchar(s[k] = s[i]);
                            s[k] = '\0';
                            for (; k != i; k++) line_putchar(' ');
                            for (; i != j; i--) line_putchar('\b');
                        }
                        break;

                    case 'f':
                        WORD_FWD(line_putchar(s[i]));
                        break;

                    case CTRL('H'):
                    case DEL:
                        if (i) {
                            unsigned j = i, k;
                            WORD_BKW(line_putchar('\b'));
                            for (k = i; s[j]; k++, j++) line_putchar(s[k] = s[j]);
                            s[k] = '\0';
                            for (; k != j; k++) line_putchar(' ');
                            for (; k != i; k--) line_putchar('\b');
                        }
                        break;

                    case 'l':
                        WORD_FWD(line_putchar(IS_LETTER(s[i]) ? s[i]|= 32 : s[i]));
                        break;

                    case 't':
                        if (i) {
                            char* cpy;
                            unsigned k, j = i, bst, ben, fst, fen, len;
                            int found = 0;
                            WORD_BKW(if (!found && IS_LETTER(s[i])) { found = 1; ben = i+1; });
                            bst = i;
                            i = j;
                            if (!found) break;
                            found = 0;
                            WORD_FWD(if (!found && IS_LETTER(s[i])) { found = 1; fst = i; });
                            fen = i;
                            if (!found) {
                                fst = bst;
                                fen = ben;
                                i = bst;
                                found = 0;
                                WORD_BKW(if (!found && IS_LETTER(s[i])) { found = 1; ben = i+1; });
                                bst = i;
                                i = j;
                                if (!found) break;
                            }
                            i = j;
                            len = strlen(s);
                            cpy = malloc(len);
                            if (!cpy) break;
                            memcpy(cpy, s, len);
                            for (; i != bst; i--) line_putchar('\b');
                            for (k = fst; k != fen; k++, i++) line_putchar(s[i] = cpy[k]);
                            for (k = ben; k != fst; k++, i++) line_putchar(s[i] = cpy[k]);
                            for (k = bst; k != ben; k++, i++) line_putchar(s[i] = cpy[k]);
                            free(cpy);
                        }
                        break;

                    case 'u':
                        WORD_FWD(line_putchar(IS_LETTER(s[i]) ? s[i]&=~32 : s[i]));
                        break;
                }
                break; /* case ESC */

            case CTRL(']'):
                for (c = line_getchar(); s[i] && c != s[i]; i++) line_putchar(s[i]);
                break;

            case CTRL('A'):
                for (; i; i--) line_putchar('\b');
                break;

            case CTRL('B'):
                if (i) i--, line_putchar('\b');
                break;

            case CTRL('C'):
                line_putchar('^');
                line_putchar('C');
                s[0] = '\0';
                /* fall through */
            case CTRL('M'):
            case CTRL('J'):
                if (s[0] && 1 < _hist_at) strcpy(_hist_ls[0], s);
                _hist_at = 0;
                /* fall through */
            case CTRL('O'):
                if (_hist_at) _hist_at--;
                line_putchar('\r');
                line_putchar('\n');
                return s;

            case CTRL('D'):
                if ('\0' != *s) {
                    if (s[i]) {
                        unsigned j = i++, k;
                        for (k = j; s[i]; k++, i++) line_putchar(s[k] = s[i]);
                        s[k] = '\0';
                        line_putchar(' ');
                        for (; i != j; i--) line_putchar('\b');
                    }
                    break;
                }
                /* fall through */
            case EOF:
                return NULL;

            case CTRL('E'):
                for (; s[i]; i++) line_putchar(s[i]);
                break;

            case CTRL('F'):
                if (s[i]) line_putchar(s[i++]);
                break;

            case CTRL('H'):
            case DEL:
                if (i) {
                    unsigned j = i-1, k;
                    line_putchar('\b');
                    for (k = j; s[i]; k++, i++) line_putchar(s[k] = s[i]);
                    s[k] = '\0';
                    line_putchar(' ');
                    for (; i != j; i--) line_putchar('\b');
                }
                break;

            case CTRL('I'):
                if (_compgen_words) {
                    char** words = _compgen_words(s, i);
                    while (words && words[0]) {
                        char* cpy;
                        unsigned k, j = 0, n;
                        for (k = 0; words[k]; k++) {
                            unsigned l = strlen(words[k]);
                            if (j < l) j = l;
                        }
                        for (k = i; ESC != s[k]; k++);
                        n = (k+1)+12;
                        while (n < k+j) n*= 2;
                        n-= 12;
                        cpy = malloc(n);
                        if (!cpy) break;
                        memcpy(cpy, s, k);
                        if (n-k-1) memset(cpy+k+1, '\0', n-k-2);
                        cpy[n-1] = ESC;
                        s = cpy;
                        cpy = _hist_ls[_hist_at];
                        _hist_ls[_hist_at] = s;
                        reprocess = 1;
                        k = 0;
                        j = i;
                        while (1) {
                            unsigned l = 0, il;
                            int z = 0;
                            for (; j != i; i--) line_putchar('\b');
                            for (; words[k][i-j]; i++) line_putchar(s[i] = words[k][i-j]);
                            for (; cpy[j+l]; l++) {
                                z = z || !s[i+l];
                                line_putchar(s[i+l] = cpy[j+l]);
                            }
                            il = i+l;
                            if (!z) for (; s[i+l]; l++) line_putchar(' ');
                            s[il] = '\0';
                            for (; l; l--) line_putchar('\b');
                            if (CTRL('I') != (c = line_getchar())) break;
                            if (!words[++k]) k = 0;
                        }
                        free(cpy);
                        break;
                    } /* if words (while, so that it can break) */
                    if (_compgen_clean) _compgen_clean(words);
                }
                break;

            case CTRL('K'):
                if (s[i]) {
                    unsigned j = i;
                    for (; s[j]; j++) line_putchar(' ');
                    for (; j != i; j--) line_putchar('\b');
                    s[i] = '\0';
                }
                break;

            case CTRL('N'):
                if (_hist_at) {
                    unsigned p_l = strlen(s), n_l, k;
                    for (k = i; k; k--) line_putchar('\b');
                    s = _hist_ls[--_hist_at];
                    for (k = 0; s[k]; k++) line_putchar(s[k]);
                    n_l = k;
                    if (n_l < p_l) {
                        for (; k < p_l; k++) line_putchar(' ');
                        for (; n_l < k; k--) line_putchar('\b');
                    }
                    i = n_l;
                }
                break;

            case CTRL('P'):
                if (_hist_at+1 < _hist_ln && _hist_ls[_hist_at+1]) {
                    unsigned p_l = strlen(s), n_l, k;
                    for (k = i; k; k--) line_putchar('\b');
                    s = _hist_ls[++_hist_at];
                    for (k = 0; s[k]; k++) line_putchar(s[k]);
                    n_l = k;
                    if (n_l < p_l) {
                        for (; k < p_l; k++) line_putchar(' ');
                        for (; n_l < k; k--) line_putchar('\b');
                    }
                    i = n_l;
                }
                break;

            case CTRL('R'):
            case CTRL('S'):
                {
                    char sr_s[32];
                    unsigned sr_i = 0, k, j;
                    int rev = c == CTRL('R');
                    sr_s[0] = '\0';
                    sr_i = i;
                    for (; s[sr_i]; sr_i++) line_putchar(' ');
                    for (; sr_i; sr_i--) line_putchar('\b');
                    line_putchar('^');
                    line_putchar(rev ? 'R' : 'S');
                    line_putchar(':');
                    if (3 < i) for (; sr_i < i-3; sr_i++) line_putchar(' ');
                    for (; sr_i; sr_i--) line_putchar('\b');
                    sr_s[sr_i = 0] = '\0';
                    while (1) {
                        line_putchar('|');
                        for (k = 0; s[k]; k++) line_putchar(s[k]);
                        k++, line_putchar(' ');
                        for (; k != i; k--) line_putchar('\b');
                        c = line_getchar();
                        if (' ' <= c && c <= '~' && sr_i <31) {
                            for (k = i+1; k != 0; k--) line_putchar('\b');
                            line_putchar(sr_s[sr_i++] = c);
                        } else if ((DEL == c || CTRL('H') == c) && sr_i) {
                            for (k = i+2; k != 0; k--) line_putchar('\b');
                            --sr_i;
                        } else {
                            reprocess = /*ESC != c &&*/ CTRL('G') != c && (rev ? CTRL('R') : CTRL('S')) != c;
                            break;
                        }
                        sr_s[sr_i] = '\0';
                        while (rev ? _hist_at+1 < _hist_ln && _hist_ls[_hist_at] : _hist_at) {
                            char* at = strstr(_hist_ls[_hist_at], sr_s);
                            if (!at) rev ? _hist_at++ : _hist_at--;
                            else {
                                line_putchar(' ');
                                for (k = 1; s[k]; k++) line_putchar(' ');
                                for (; k; k--) line_putchar('\b');
                                i = at - (s = _hist_ls[_hist_at]);
                                break;
                            }
                        } /* while strstr */
                    } /* while 1 - line_getchar (also breaks if not handled) */
                    for (k = i+sr_i+4; k; k--) line_putchar('\b');
                    for (; s[k]; k++) line_putchar(s[k]);
                    for (j = 0; j != sr_i+4; j++, k++) line_putchar(' ');
                    for (; k != i; k--) line_putchar('\b');
                }
                break; /* case ^R/^S */

            case CTRL('T'):
                if (i) {
                    char p;
                    if (!s[i]) i--, line_putchar('\b');
                    line_putchar('\b');
                    p = s[i-1];
                    line_putchar(s[i-1] = s[i]);
                    line_putchar(s[i++] = p);
                }
                break;

            case CTRL('U'):
                if (i) {
                    unsigned j = i, k;
                    for (; i; i--) line_putchar('\b');
                    for (k = i; s[j]; k++, j++) line_putchar(s[k] = s[j]);
                    s[k] = '\0';
                    for (; k != j; k++) line_putchar(' ');
                    for (; j != i; j--) line_putchar('\b');
                }
                break;

            case CTRL('W'):
                if (i) {
                    unsigned j = i, k;
                    while (0 < i && ' ' != s[i-1]) i--, line_putchar('\b');
                    for (k = i; s[j]; k++, j++) line_putchar(s[k] = s[j]);
                    s[k] = '\0';
                    for (; k != j; k++) line_putchar(' ');
                    for (; k != i; k--) line_putchar('\b');
                }
                break;

            case CTRL('Z'):
                line_reset();
                line_pause();
                line_ready();
                break;

            default:
                if (' ' <= c && c <= '~') {
                    char p = s[i];
                    unsigned k = i+1;
                    line_putchar(s[i++] = c);
                    while (p) {
                        char w = s[k];
                        line_putchar(s[k++] = p);
                        p = w;
                    }
                    if (ESC == s[k]) {
                        unsigned n = (k+1)*2+12;
                        if (!(s = realloc(s, n))) return s;
                        _hist_ls[_hist_at] = s;
                        memset(s+k+1, '\0', n-k-2);
                        s[n-1] = ESC;
                    }
                    s[k] = p;
                    for (; k != i; k--) line_putchar('\b');
                }
        } /* switch c */
    } /* while 1 */

    return s;
}

char** line_histget(unsigned* count) {
    for (*count = 0; *count < _hist_ln && _hist_ls[*count]; (*count)++);
    return _hist_ls[0] && !_hist_ls[0][0] ? (*count)--, _hist_ls+1 : _hist_ls;
}

void line_histset(char** lines, unsigned count) {
    unsigned k;
    line_free();
    if (lines && count) for (k = 0; count && k < _hist_ln; count--, k++) {
        unsigned l = strlen(lines[k]), n = 64;
        while (n < l) n*= 2;
        n-= 12;
        if (!(_hist_ls[count-1] = calloc(n, 1))) break;
        memcpy(_hist_ls[count-1], lines[k], l+1);
        _hist_ls[count-1][n-1] = ESC;
    }
}

void line_free(void) {
    unsigned k;
    for (k = _hist_at = 0; k < _hist_ln; k++) _hist_ls[k] = (free(_hist_ls[k]), NULL);
}

void line_compgen(char** (*words)(char* line, unsigned point), void (*clean)(char** words)) {
    _compgen_words = words;
    _compgen_clean = clean;
}

#undef ESC
#undef DEL
#undef IS_LETTER
#undef IS_WORD
#undef WORD_BKW
#undef WORD_FWD
#undef HAS_TERMIOS
#undef line_fileno
#undef line_getchar
#undef line_putchar
#undef line_ready
#undef line_reset
#undef line_pause
#endif /* LINE_IMPLEMENTATION */
