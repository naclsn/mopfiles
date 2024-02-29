#define _POSIX_SOURCE /* kill, fileno */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

FILE* term = NULL;
struct termios term_good, term_raw;
int mygetchar(void);
int myputchar(int c);
void mylinepause(void);
#define LINE_IMPLEMENTATION
#define line_fileno fileno(term)
#define line_getchar mygetchar
#define line_putchar myputchar
#define line_pause mylinepause
#include "utils/line.h"

FILE* file = NULL;
unsigned short row = 24, col = 80;
char* name = NULL;
/* temporary file for commands like 'v' */
#define TMP_PAGER_STDIN "/tmp/pager-stdin"

char** lines = NULL;      /* lines[0] is the whole buffer, and thus start of first line, lines[k] is end of line k (ie first line is lines[0]..lines[1]) */
char** line_rends = NULL; /* line_rends[k] is since the last SGR reset ("ESC[0m" or "ESC[m") and so line_rends[0] == lines[0] */
unsigned capacity = 2048; /* capacity of the whole buffer */
unsigned length = 0;      /* length of the whole buffer, so how many bytes were read */
unsigned count = 0;       /* line count (eg if there's 2 lines, then it's lines[0]..lines[1] and lines[1]..lines[2]) */
unsigned top = 0;         /* the index of the start of the first visible line */

struct {
    unsigned alt      :1, /* in alt screen (not toggleable) */
             eof      :1, /* found eof (not toggleable) */
             wrap     :1, /* wrap long lines */
             cutwords :1, /* when wrapping, do cut words */
             raw      :1, /* show ctrl chars */
             nums     :1, /* show line numbers */
             marks    :1; /* show marks */
} flags = {0};
unsigned marks[26] = {0}; /* a-z (A-Z fold back to it) */
char* search = NULL;
unsigned searchlen = 0;

#define enter_alt() ((void)(flags.alt || fprintf(term, "\x1b[?1049h\x1b[?1000h\x1b[m")), flags.alt = 1)
#define leave_alt() ((void)(flags.alt && fprintf(term, "\x1b[?1049l\x1b[?1000l\x1b[m")), flags.alt = 0)

#define get_key(_in) (read(fileno(term), (_in), 1) <= 0 ? exit(EXIT_FAILURE), 0 : 1)
#define get_line(_in) ((*(_in) = line_read_raw()) && (*(_in))[0])

void scroll_to(unsigned);

int mygetchar(void) { char c; get_key(&c); return c; }
int myputchar(int c) { return '\n' == c ? 0 : fputc(c, term); }
void mylinepause(void) {
    leave_alt();
    kill(0, SIGTSTP);
    enter_alt();
    scroll_to(top);
    /* TODO: line.h doesn't redraw it's content, and the prompt text is lost
     * anyways, so that leaves the user in a somewhat awkward situation... */
}

unsigned scan_escape(char* ptr, unsigned len) {
    unsigned k = 0;
    while (k < len && 'm' != ptr[++k]);
    return ++k;
}

unsigned cell_count(unsigned cp) {
    (void)cp; /* TODO: properly estimate display width for codepoint */
    return 1;
}

void fetch_lines(unsigned till) {
    unsigned len, k;

    while (count <= till && !flags.eof) {
        if (capacity-length < 256) {
            char* plines = *lines;
            *lines = *line_rends = realloc(*lines, capacity*= 2);
            for (k = 0; k < count; k++) {
                lines[k+1] = *lines + (lines[k+1]-plines);
                line_rends[k+1] = *lines + (line_rends[k+1]-plines);
            }
        }

        fprintf(term, " waiting...\r");
        fflush(term);
        tcsetattr(fileno(term), TCSANOW, &term_good);
        len = fread(*lines+length, 1, capacity-length-1, file);
        tcsetattr(fileno(term), TCSANOW, &term_raw);
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
    if ((unsigned long)count+row-1 <= st) st = count ? count-1 : 0;

    if (st < count) for (ptr = line_rends[st]; ptr < lines[st]; ptr++) if (CTRL('[') == *ptr) {
        unsigned n = scan_escape(ptr, lines[st]-ptr);
        if (!n) continue;
        if ('m' == ptr[n-1]) fprintf(term, "%.*s", n, ptr);
        ptr+= n;
    }

    for (l = st; l < (unsigned long)st+nm; l++) {
        unsigned initw, found = 0;
        unsigned lastwrap_k = 0, lastwrap_w = 0;

        /* current line OR trailling chars after last '\n' OR "~" */
        ptr = l < count ? lines[l]              : count == l && lines[count] < *lines+length ? lines[count]               : "~";
        len = l < count ? lines[l+1]-lines[l]-1 : count == l && lines[count] < *lines+length ? *lines+length-lines[count] : 1;

        w = 0;
        fprintf(term, "\x1b[K");
        if (flags.marks) {
            if (l < count) {
                for (k = 0; k < 26; k++) if (l+1 == marks[k]) {
                    fprintf(term, "%c ", 'a'+k);
                    break;
                } if (26 == k) fprintf(term, "  ");
            } else fprintf(term, "  ");
            w+= 2;
        }
        if (flags.nums) {
            if (l < count) w+= fprintf(term, "%4u ", l+1);
            else if (count == l) w+= fprintf(term, "%4s ", "+");
        }
        initw = w;

        for (k = 0; k < len && (flags.wrap || w < col);) {
            if (flags.wrap && col <= w) {
                if (!flags.cutwords && lastwrap_k) {
                  /* weird logic below is because a putc on last column doesn't
                   * move the cursor to the next line _yet_ (only the following
                   * putc would), so we need to put one less '\b' except if we
                   * only need to put one (in which case we still need to put
                   * it) */
                  if (--w == lastwrap_w) fputc('\b', term);
                  else while (lastwrap_w < w--) fputc('\b', term);
                  k = lastwrap_k;
                  fprintf(term, "\x1b[K");
                } /* !cutwords */

                fprintf(term, "\r\n%*s", w = initw, "");
                nm--; /* TODO: do something */
            } /* wrap */

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
                /* can wrap if not 0-9A-Za-z */
                if ( ((ptr[k-1]|32) < '0' || '9' < (ptr[k-1]|32)) &&
                        ((ptr[k-1]|32) < 'a' || 'z' < (ptr[k-1]|32)) )
                    lastwrap_k = k, lastwrap_w = w;
            } /* printable ascii */
            else if ('\t' == ptr[k]) {
                unsigned nw = (w&(unsigned)-8) +8;
                fprintf(term, "%*s", nw-w, "");
                k++;
                w = nw;
                lastwrap_k = k, lastwrap_w = w;
            } /* .. and horizontal tab */

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
                lastwrap_k = k, lastwrap_w = w;
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
                    fprintf(term, "%.*s\x1b[6n", ++k-pk, ptr+pk);
                    w+= cell_count(u);
                    /* TODO: check is word break (eg cp=0xe9 shouln't) */
                    lastwrap_k = k, lastwrap_w = w;
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
    if (!count) return;
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
    if (!count) to = 0;
    fprintf(term, "\x1b[H\x1b[J");
    show_lines(to, row-1);
    top = count-1 < to ? count-1 : to;
}

void status(char* msg) {
    unsigned len;
    char tmp[1024] = {0};

    len = sprintf(tmp, " %s  lines %u-%u (of %s%u"
            , name
            , count ? top+1 : 0
            , count < top+row-1 ? count : top+row-1
            , flags.eof ? "" : ">"
            , count);
    if (lines[count] < *lines+length) len+= sprintf(tmp+len, " +%lub", *lines+length-lines[count]);
    tmp[len++] = ')';
    if (msg) len+= sprintf(tmp+len, "  %s", msg);

    if (len+1 < col) memset(tmp+len, ' ', col-1-len);
    else tmp[col-1] = '\0';

    fprintf(term, msg
            ? "\x1b[m\x1b[%dH\x1b[36m\x1b[7m%s\x1b[m"
            : "\x1b[m\x1b[%dH\x1b[7m%s\x1b[m"
            , row, tmp);
    fflush(term);
}

void sigwinch(int sig) {
    struct winsize winsz;
    if (SIGWINCH == sig && !ioctl(fileno(term), TIOCGWINSZ, &winsz) && winsz.ws_row && winsz.ws_col) {
        row = winsz.ws_row;
        col = winsz.ws_col;
        if (lines) {
            scroll_to(top);
            status(NULL);
        }
        signal(SIGWINCH, sigwinch);
    }
}

void sigterm(int sig) {
    (void)sig;
    write(STDERR_FILENO, "\x1b[?1049l\x1b[?1000l\x1b[m", strlen("\x1b[?1049l\x1b[?1000l\x1b[m"));
    tcsetattr(STDERR_FILENO, TCSANOW, &term_good);
    _exit(0);
}

void cleanup(void) {
    line_free();

    leave_alt();
    tcsetattr(fileno(term), TCSANOW, &term_good);

    if (stdin != file) fclose(file);
    if (lines) free(*lines);
    free(lines);

    fflush(term);
}

void setup(void) {
    term = stderr;
    if (tcgetattr(fileno(term), &term_good)) perror(NULL), exit(EXIT_FAILURE);
    atexit(cleanup);

    sigwinch(SIGWINCH);
    signal(SIGTERM, sigterm);
    signal(SIGINT, sigterm);

    lines = malloc(row*2 * sizeof*lines);
    line_rends = malloc(row*2 * sizeof*line_rends);
    lines[row*2] = (char*)-1;
    *lines = *line_rends = malloc(capacity);
    (*lines)[capacity-1] = '\0';

    term_raw = term_good;
    term_raw.c_iflag&=~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    term_raw.c_oflag&=~(OPOST);
    term_raw.c_lflag&=~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    term_raw.c_cflag&=~(CSIZE | PARENB);
    term_raw.c_cflag|= (CS8);

    tcsetattr(fileno(term), TCSANOW, &term_raw);
    enter_alt();

    /* TODO: line_compgen(compgen_words, compgen_clean); */
}

int main(int argc, char** argv) {
    char const* prog = (argc--, *argv++);
    char* msg = NULL, c = CTRL('L'); /* TODO: less' -F */

    file = stdin;
    name = "<stdin>";
    if (argc && !strcmp("-h", argv[0])) {
        printf("Usage: %s [<file> [+<line>]]\n"
               "\n", prog);
        printf("   ^L             force redraw\n"
               "    q  Q ^Q       quit\n"
               "   ^Z             background (job control)\n"
               "\n");
        printf("    j ^N ^E down  next line\n"
               "    k ^P ^Y up    previous line\n"
               "    d  D ^D       next half-screen\n"
               "    u  U ^U       previous half-screen\n"
               "    f  F ^F       next screen\n"
               "    b  B ^B       previous screen\n"
               "    g       home  first line\n"
               "    G       end   last line\n"
               "    wheel down    next     3 lines\n"
               "    wheel up      previous 3 lines\n"
               "\n");
        printf("    m  (M)        put mark at top (respectively bottom)\n"
               "    '   `         go to mark\n"
               "    /  (?)        search forward (backward)\n"
               "    n  (N)        next occurence forward (backward)\n"
               "\n");
        printf("    v             open in $EDITOR (default `vi`)\n"
               "                   (saves stdin to '" TMP_PAGER_STDIN "' if needed,\n"
               "                   it will not be automatically deleted!)\n"
               "    r  e          read from file\n"
               "    w  s          write to file\n"
               "    !             run shell command\n"
               "    :             go to line number\n");
        printf("    -  _          toggle option flag:\n"
               "                   -S  wrap long lines\n"
               "                   -w  when wrapping, do cut words\n"
               "                   -R  show control characters\n"
               "                   -N  show line numbers\n"
               "                   -J  show marks\n"
               "\n");
        return EXIT_SUCCESS;
    }
    if (argc && strcmp("-", argv[0])) {
        file = fopen(argv[0], "rb");
        name = strrchr(argv[0], '/');
        if (!name++) name = argv[0];
    }
    if (!file) return perror(argv[0]), EXIT_FAILURE;
    if (stdin != file && 1 < argc && '+' == argv[1][0]) {
        long l = atol(argv[1]+1);
        if (0 < l) top = l-1;
        else msg = "invalid line number";
    }

    setup();

    do switch (c) {
        case CTRL('L'): scroll_to(top); break;

        case 'q': case 'Q': case CTRL('Q'): return EXIT_SUCCESS;

        case CTRL('Z'):
            leave_alt();
            tcsetattr(fileno(term), TCSANOW, &term_good);
            kill(0, SIGTSTP);
            tcsetattr(fileno(term), TCSANOW, &term_raw);
            enter_alt();
            scroll_to(top);
            break;

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
            fprintf(term, "\r\x1b[Kset mark top: ");
            get_key(&c); c|= 32;
            if ('a' <= c && c <= 'z') { marks[c-'a'] = top+1; scroll_to(top); }
            else msg = "invalid mark (should be a-z)";
            break;
        case 'M':
            fprintf(term, "\r\x1b[Kset mark bot: ");
            get_key(&c); c|= 32;
            if ('a' <= c && c <= 'z') { marks[c-'a'] = top+row-1 < count ? top+row-1 : count; scroll_to(top); }
            else msg = "invalid mark (should be a-z)";
            break;
        case '\'': case '`':
            fprintf(term, "\r\x1b[Kgo to mark: ");
            get_key(&c); c|= 32;
            if ('a' <= c && c <= 'z' && marks[c-'a']) scroll_to(marks[c-'a']-1);
            else msg = marks[c-'a'] ? "invalid mark (should be a-z)" : "mark not set";
            break;

        case '/': {
            fprintf(term, "\r\x1b[Kforward search: ");
            if (get_line(&search)) {
                searchlen = strlen(search);
        case 'n':
                if (search) {
                    char* at;
                    while (!(at = strstr(lines[top+1], search)) && !flags.eof) fetch_lines(count*2);
                    if (!at) msg = "no occurence forward";
                    else {
                        unsigned k;
                        for (k = top+1; k+1 < count; k++) if (lines[k] <= at && at < lines[k+1]) {
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
                if (search) {
                    char* at = lines[top]-searchlen;
                    while (lines[0] <= --at && memcmp(at, search, searchlen));
                    if (at < lines[0]) msg = "no occurence backward";
                    else {
                        unsigned k;
                        for (k = top; k; k--) if (lines[k-1] <= at && at < lines[k]) {
                            top = k-1;
                            break;
                        }
                        scroll_to(top);
                    }
                }
            } /* get line */
        } break;

        case 'v': {
            char* ed = getenv("EDITOR"), at[8] = {0}, * args[4] = {0};
            if (!ed || !*ed) ed = "vi";
            if (stdin == file) {
                FILE* f = fopen(TMP_PAGER_STDIN, "wb");
                if (!f) {
                    msg = "could not open '" TMP_PAGER_STDIN "' file for writing";
                    break;
                }
                fetch_lines(-1);
                fwrite(*lines, 1, length, f);
                fclose(f);
                name = TMP_PAGER_STDIN;
            }
            cleanup();
            args[0] = ed;
            args[1] = name;
            if (strstr(ed, "vi")) args[2] = (sprintf(at, "+%u", top+1), at);
            execvp(ed, args);
            exit(1);
        } break;

        case 'r':
        case 'e': {
            char* line;
            fprintf(term, "\r\x1b[Kread file: ");
            if (get_line(&line)) {
                FILE* f = fopen(line, "rb");
                if (!f) msg = "could not open file for reading";
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
                if (!f) msg = "could not open file for writing";
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
                tcsetattr(fileno(term), TCSANOW, &term_good);
                system(line);
                tcsetattr(fileno(term), TCSANOW, &term_raw);
                get_key(&c);
                enter_alt();
                scroll_to(top);
            }
        } break;

        case ':': {
            char* line;
            fprintf(term, "\r\x1b[Kgoto line: ");
            if (get_line(&line)) {
                long l = atol(line);
                if (0 < l) scroll_to(l-1);
                else msg = "invalid line number";
                if (count < l) msg = "given line number outside file";
            }
        } break;

        case '-': case '_': switch (fprintf(term, "\r\x1b[Ktoggle option: "), get_key(&c), c) {
            case 'S': flags.wrap++;     scroll_to(top); break;
            case 'w': flags.cutwords++; scroll_to(top); break;
            case 'R': flags.raw++;      scroll_to(top); break;
            case 'N': flags.nums++;     scroll_to(top); break;
            case 'J': flags.marks++;    scroll_to(top); break;
        } break;
    } while (status(msg), msg = NULL, get_key(&c));

    return EXIT_SUCCESS;
}
