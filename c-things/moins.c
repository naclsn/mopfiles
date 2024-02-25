/** TODO:
 *  - line wrap (-S)
 *  - fix backward search
 *  - 'v' (open in editor)
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int myfileno(void);
int mygetchar(void);
int myputchar(int c);

#define LINE_IMPLEMENTATION
#define line_fileno myfileno()
#define line_getchar mygetchar
#define line_putchar myputchar
#include "utils/line.h"

FILE* term = NULL;
FILE* file = NULL;
struct termios term_in, term_raw;
unsigned short row = 24, col = 80;
char* name = NULL;

char** lines = NULL;      /* lines[0] is the whole buffer, and thus start of first line, lines[k] is end of line k (ie first line is lines[0]..lines[1]) */
char** line_rends = NULL; /* line_rends[k] is since the last SGR reset ("ESC[0m" or "ESC[m") and so line_rends[0] == lines[0] */
unsigned capacity = 2048; /* capacity of the whole buffer */
unsigned length = 0;      /* length of the whole buffer, so how many bytes were read */
unsigned count = 0;       /* line count (eg if there's 2 lines, then it's lines[0]..lines[1] and lines[1]..lines[2]) */
unsigned top = 0;         /* the index of the start of the first visible line */

struct {
    unsigned alt :1,   /* in alt screen (not toggleable) */
             eof :1,   /* found eof (not toggleable) */
             wrap :1,  /* NIY: wrap long lines */
             raw :1,   /* explicit ctrl chars */
             nums :1,  /* show line number */
             marks :1; /* show marks */
} flags = {0};
unsigned marks[26] = {0}; /* a-z (A-Z fold back to it) */
char* search = NULL;
unsigned searchlen = 0;

#define enter_alt() ((void)(flags.alt || fprintf(term, "\x1b[?1049h\x1b[?1000h\x1b[m")), flags.alt = 1)
#define leave_alt() ((void)(flags.alt && fprintf(term, "\x1b[?1049l\x1b[?1000l\x1b[m")), flags.alt = 0)

#define get_key(_in) (read(fileno(term), (_in), 1) <= 0 ? exit(EXIT_FAILURE), 0 : 1)
#define get_line(_in) (*(_in) = line_read_raw(&term_in, &term_raw))

int myfileno(void) { return fileno(term); }
int mygetchar(void) { char c; get_key(&c); return c; }
int myputchar(int c) { return '\n' == c ? 0 : fputc(c, term); }

unsigned scan_escape(char* ptr, unsigned len) {
    unsigned k = 0;
    while (k < len && 'm' != ptr[++k]);
    return ++k;
}

void fetch_lines(unsigned till) {
    unsigned len, k;

    while (count <= till && !flags.eof) {
        if (capacity-length < 256) {
            char* plines = *lines;
            *lines = realloc(*lines, capacity*= 2);
            for (k = 0; k < count; k++) {
                lines[k+1] = *lines + (lines[k+1]-plines);
                line_rends[k+1] = *lines + (line_rends[k+1]-plines);
            }
        }

        fprintf(term, " waiting...\r");
        fflush(term);
        len = fread(*lines+length, 1, capacity-length-1, file);
        (*lines)[length+len] = '\0';

        for (k = 0; k < len; k++) if ('\n' == (*lines)[length+k]) {
            char* ptr;

            if ((char*)-1 == lines[++count]) {
                lines = realloc(lines, count*2 * sizeof*lines);
                line_rends = realloc(line_rends, count*2 * sizeof*line_rends);
                lines[count*2] = (char*)-1;
            }

            lines[count] = *lines+length+k+1;
            line_rends[count] = line_rends[count-1];

            for (ptr = lines[count]-1; lines[count-1] <= ptr-3; ptr--)
                if (!memcmp(ptr-3, "\x1b[m", 3) || !memcmp(ptr-4, "\x1b[0m", 4)) {
                    line_rends[count] = ptr;
                    break;
                }
        } /* k = 0..len */

        length+= len;
        flags.eof = feof(file);
    } /* !eof */
}

void show_lines(unsigned st, unsigned nm) {
    unsigned len, k, l, w;
    char* ptr;

    fetch_lines((unsigned long)st+nm <= (unsigned)-1 ? st+nm : (unsigned)-1);
    if ((unsigned long)count+row-1 <= st) st = count-1;

    if (st < count) for (ptr = line_rends[st]; ptr < lines[st]; ptr++) if (CTRL('[') == *ptr) {
        unsigned n = scan_escape(ptr, lines[st]-ptr);
        if (!n) continue;
        if ('m' == ptr[n-1]) fprintf(term, "%.*s", n, ptr);
        ptr+= n;
    }

    for (l = st; l < (unsigned long)st+nm; l++) {
        unsigned found = 0;

        ptr = l < count ? lines[l] : "~";
        len = l < count ? lines[l+1]-lines[l]-1 : 1;

        w = 0;
        fprintf(term, "\x1b[K");
        if (flags.marks && l < count) {
            for (k = 0; k < 26; k++) if (l+1 == marks[k]) {
                fprintf(term, "%c ", 'a'+k);
                break;
            } if (26 == k) fprintf(term, "  ");
            w+= 2;
        }
        if (flags.nums && l < count) w+= fprintf(term, "%4u ", l);

        for (k = 0; k < len && w < col;) {
            if (search) {
                if (!found) {
                    if (!memcmp(ptr+k, search, searchlen)) {
                        found = searchlen;
                        fprintf(term, "\x1b[7m");
                    }
                } else if (!--found) fprintf(term, "\x1b[27m");
            }

            if (' ' <= ptr[k] && ptr[k] <= '~') {
                fputc(ptr[k++], term);
                w++;
            } /* printable ascii */

            else if ((0 <= ptr[k] && ptr[k] < ' ') || 127 == ptr[k]) {
                if (flags.raw) {
                    fputc('^', term);
                    if (++w < col) fputc(127 != ptr[k] ? ptr[k]|64 : '?', term), w++;
                    k++;
                } else {
                    if (CTRL('[') == ptr[k]) {
                        unsigned n = scan_escape(ptr+k, len-k);
                        fprintf(term, "%.*s", n, ptr+k);
                        k+= n;
                    }
                }
            } /* ctrls + del */

            else {
                unsigned u = ptr[k], pk = k;
                if (0 == (32 & u) && k+1 < len) {
                    char x = ptr[++k];
                    u = ((u & 31) << 6) | (x & 63);
                } else if (0 == (16 & u) && k+2 < len) {
                    char x = ptr[++k], y = ptr[++k];
                    u = ((u & 15) << 12) | ((x & 63) << 6) | (y & 63);
                } else if (0 == (8 & u) && k+3 < len) {
                    char x = ptr[++k], y = ptr[++k], z = ptr[++k];
                    u = ((u & 7) << 18) | ((x & 63) << 12) | ((y & 63) << 6) | (z & 63);
                }
                if (pk != k) {
                    (void)u;
                    /* XXX: this surely is the worse hack I've ever written in my life so far */
                    if (2 <= col-w) { /* widest it can be is 2 cells, don't even try if not at least this much available */
                        char c;
                        fprintf(term, "%.*s\x1b[6n", k-pk, ptr+pk);
                        /* ask the terminal to be smart instead, this is as likely to break as it is to work */
                        while (get_key(&c), ';' != c);
                        w = 0;
                        while (get_key(&c), 'R' != c) w = w*10 + c-'0';
                        w--;
                    } else w = col;
                }
            } /* multi byte (utf-8) */
        } /* kk = 0..len */

        if (found) fprintf(term, "\x1b[27m");

        if (!flags.raw) {
            while (k < len) if (CTRL('[') == ptr[k]) {
                unsigned n = scan_escape(ptr+k, len-k);
                fprintf(term, "%.*s", n, ptr+k);
                k+= n;
            } else k++;
        } /* ansi sequences out of screen */

        fprintf(term, "\r\n");
    } /* k = st..st+nb */
}

void scroll_up(unsigned by) {
    if (count-1 < by+top) by = count-1-top;
    if (!by) return;
    fprintf(term, "\x1b[%uS\x1b[%uH", by, row-by);
    top+= by;
    show_lines(top+row-by-1, by);
}

void scroll_dw(unsigned by) {
    if (top < by) by = top;
    if (!by) return;
    fprintf(term, "\x1b[%uT\x1b[H", by);
    top-= by;
    show_lines(top, by);
}

void scroll_to(unsigned to) {
    fprintf(term, "\x1b[H");
    show_lines(to, row-1);
    top = count-1 < to ? count-1 : to;
}

void status(char* msg) {
    unsigned len;
    char tmp[1024] = {0};
    len = sprintf(tmp, " file: %s  lines %d to %d of %d%s", name, top+1, count < top+row-1 ? count : top+row-1, flags.eof ? count : 0, msg);
    if (len+1 < col) memset(tmp+len, ' ', col-1-len);
    else tmp[col-1] = '\0';
    fprintf(term, "\x1b[m\x1b[%dH\x1b[7m%s\x1b[m", row, tmp);
    fflush(term);
}

void resize(int sig) {
    struct winsize winsz;
    if (SIGWINCH == sig && !ioctl(fileno(term), TIOCGWINSZ, &winsz) && winsz.ws_row && winsz.ws_col) {
        row = winsz.ws_row;
        col = winsz.ws_col;
        if (lines) {
            scroll_to(top);
            status("");
        }
    }
}

void cleanup(void) {
    line_free();

    leave_alt();
    tcsetattr(fileno(term), TCSANOW, &term_in);

    if (stdin != file) fclose(file);
    if (lines) free(*lines);
    free(lines);
    fflush(term);
}

void setup(void) {
    term = stderr;
    if (tcgetattr(fileno(term), &term_in)) perror(NULL), exit(EXIT_FAILURE);
    atexit(cleanup);

    resize(SIGWINCH);
    signal(SIGWINCH, resize);

    lines = malloc(row*2 * sizeof*lines);
    line_rends = malloc(row*2 * sizeof*line_rends);
    lines[row*2] = (char*)-1;
    *lines = *line_rends = malloc(capacity);
    (*lines)[capacity-1] = '\0';

    term_raw = term_in;
    term_raw.c_iflag&=~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    term_raw.c_oflag&=~(OPOST);
    term_raw.c_lflag&=~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    term_raw.c_cflag&=~(CSIZE | PARENB);
    term_raw.c_cflag|= (CS8);

    tcsetattr(fileno(term), TCSANOW, &term_raw);
    enter_alt();

    /* could: line_compgen(compgen_words, compgen_clean); */
}

int main(int argc, char** argv) {
    char const* prog = (argc--, *argv++);
    char* msg = "", c = 'g';

    file = stdin;
    name = "<stdin>";
    if (argc && !strcmp("-h", argv[0])) return printf("Usage: %s [<file>]\n", prog), EXIT_SUCCESS;
    if (argc && strcmp("-", argv[0])) {
        file = fopen(argv[0], "rb");
        name = strrchr(argv[0], '/');
        if (!name++) name = argv[0];
    }
    if (!file) return perror(argv[0]), EXIT_FAILURE;

    setup();

    do switch (c) {
        case 'q': case 'Q': case CTRL('Q'): return EXIT_SUCCESS;

        case 'j': case CTRL('N'): case CTRL('E'): scroll_up(1); break;
        case 'k': case CTRL('P'): case CTRL('Y'): scroll_dw(1); break;
        case 'd': case 'D': case CTRL('D'): scroll_up(row/2); break;
        case 'u': case 'U': case CTRL('U'): scroll_dw(row/2); break;
        case 'f': case 'F': case CTRL('F'): scroll_up(row-1); break;
        case 'b': case 'B': case CTRL('B'): scroll_dw(row-1); break;
        case 'g': scroll_to(0); break;
        case 'G': fetch_lines(-1); scroll_to(count-row+1); break;

        case CTRL('['): switch (get_key(&c), c) {
            case '[': switch (get_key(&c), c) {
                case 'A': scroll_dw(1); break;
                case 'B': scroll_up(1); break;
                case 'F': fetch_lines(-1); scroll_to(count-row+1); break;
                case 'H': scroll_to(0); break;
                case 'M':
                    get_key(&c); /* btn */
                    if (c & 64) /* wheel */ c & 3 ? scroll_up(3) : scroll_dw(3);
                    get_key(&c); /* x */
                    get_key(&c); /* y */
                    break;
            } break;
        } break;

        case 'm':
            get_key(&c); c|= 32;
            if ('a' <= c && c <= 'z') { marks[c-'a'] = top+1; scroll_to(top); }
            break;
        case 'M':
            get_key(&c); c|= 32;
            if ('a' <= c && c <= 'z') { marks[c-'a'] = top+row-1; scroll_to(top); }
            break;
        case '\'': case '`':
            get_key(&c); c|= 32;
            if ('a' <= c && c <= 'z' && marks[c-'a']) scroll_to(marks[c-'a']-1);
            break;

        case '/': {
            fprintf(term, "\r\x1b[Kforward search: ");
            if (get_line(&search)) {
                searchlen = strlen(search);
        case 'n':
                if (search) {
                    char* at;
                    while (!(at = strstr(lines[top+1], search)) && !flags.eof) fetch_lines(count*2);
                    if (!at) msg = " -- no occurence forward";
                    else {
                        unsigned k;
                        for (k = top+1; k+1 < count; k++) if (lines[k] <= at && at <= lines[k+1]) {
                            top = k;
                            break;
                        }
                        scroll_to(top);
                    }
                }
            } /* get line */
        } break;

        case '?': {
            fprintf(term, "\r\x1b[Kbackward search: ");
            if (get_line(&search)) {
                searchlen = strlen(search);
        case 'N':
                if (search && count) {
                    /* FIXME: is broken */
                    unsigned bot = top+row < count ? top+row : count;
                    char* at = lines[bot-1];
                    while (--at-searchlen <= lines[0] && memcmp(at-searchlen, search, searchlen));
                    if (at-searchlen < lines[0]) msg = " -- no occurence backward";
                    else {
                        unsigned k;
                        for (k = bot; k; k--) if (lines[k-1] <= at && at <= lines[k]) {
                            top = k-1;
                            break;
                        }
                        scroll_to(top);
                    }
                }
            } /* get line */
        } break;

        case CTRL('L'):
            scroll_to(top);
            break;

        case CTRL('Z'):
            leave_alt();
            tcsetattr(fileno(term), TCSANOW, &term_in);
            raise(SIGTSTP);
            tcsetattr(fileno(term), TCSANOW, &term_raw);
            get_key(&c);
            enter_alt();
            scroll_to(top);
            break;

        case 'v': if(0) {
            /* TODO: todo */
            if (stdin != file) (void)0;
            cleanup();
            system("vi /tmp/moins-tmp");
        } break;

        case 'r':
        case 'e': {
            char* line;
            fprintf(term, "\r\x1b[Kread file: ");
            if (get_line(&line)) {
                FILE* f = fopen(line, "rb");
                if (!f) msg = " -- could not open file for reading";
                else {
                    fclose(file);
                    file = f;
                    name = strrchr(line, '/');
                    if (!name++) name = line;
                    length = 0;
                    count = 0;
                    flags.eof = 0;
                    scroll_to(0);
                }
            }
        } break;

        case 'w':
        case 's': {
            char* line;
            fprintf(term, "\r\x1b[Kwrite file: ");
            if (get_line(&line)) {
                FILE* f = fopen(line, "wb");
                if (!f) msg = " -- could not open file for writing";
                else {
                    fetch_lines(-1);
                    fwrite(*lines, 1, length, f);
                    fclose(f);
                }
            }
        } break;

        case '!': {
            char* line;
            fprintf(term, "\r\x1b[Krun shell: ");
            if (get_line(&line)) {
                leave_alt();
                tcsetattr(fileno(term), TCSANOW, &term_in);
                system(line);
                tcsetattr(fileno(term), TCSANOW, &term_raw);
                enter_alt();
                scroll_to(top);
            }
        } break;

        case ':': {
            char* line;
            fprintf(term, "\r\x1b[Kgoto line: ");
            if (get_line(&line)) {
                long l = atol(line);
                if (0 < l) scroll_to(row/2 < l ? l-row/2 : l-1);
            }
        } break;

        case '-': switch (fprintf(term, "\r\x1b[Ktoggle option: "), get_key(&c), c) {
            case 'S': flags.wrap++; break;
            case 'R': flags.raw++; scroll_to(top); break;
            case 'N': flags.nums++; scroll_to(top); break;
            case 'J': flags.marks++; scroll_to(top); break;
        } break;
    } while (status(msg), get_key(&c));

    return EXIT_SUCCESS;
}
