#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int mygetchar(void);
int myputchar(int c);

#define LINE_IMPLEMENTATION
#define line_getchar mygetchar
#define line_putchar myputchar
#include "utils/line.h"

FILE* term = NULL;
FILE* file = NULL;
struct termios term_in, term_raw;
unsigned short row = 24, col = 80;

char** lines = NULL; // lines[0] is the whole buffer, and thus start of first line, lines[k] is end of line k (ie first line is lines[0]..lines[1])
unsigned capacity = 2048; // capacity of the whole buffer
unsigned length = 0; // length of the whole buffer, so how many bytes were read
unsigned count = 0; // line count (eg if there's 2 lines, then it's lines[0]..lines[1] and lines[1]..lines[2])
unsigned top = 0; // the index of the start of the first visible line

struct {
    unsigned alt :1,
             eof :1,
             wrap :1;
} flags = {0};

#define enter_alt() ((void)(flags.alt || fprintf(term, "\x1b[?1049h\x1b[?1000h\x1b[m")), flags.alt = 1)
#define leave_alt() ((void)(flags.alt && fprintf(term, "\x1b[?1049l\x1b[?1000l\x1b[m")), flags.alt = 0)
#define move_to(_ro, _co) fprintf(term, "\x1b[%u;%uH", (_co), (_ro))

#define line_ptr(_k) (0 <= (_k) && (_k) < count ? lines[_k] : "~")
#define line_len(_k) (0 <= (_k) && (_k) < count ? lines[(_k)+1]-lines[_k]-1 : 1)

#define get_key(_in) (read(fileno(term), (_in), 1) <= 0 ? exit(EXIT_FAILURE), 0 : 1)
#define get_line(_in) (*(_in) = line_read_raw(&term_in, &term_raw))

int mygetchar(void) { char c; get_key(&c); return c; }
int myputchar(int c) { return '\n' == c ? 0 : fputc(c, term); }

void draw_lines(unsigned st, unsigned ed) {
    unsigned len, k, kk;
    for (k = st; k < ed; k++) {
        if (!flags.eof && count <= k) {
            if (capacity-length < 256) {
                char* plines = *lines;
                *lines = realloc(*lines, capacity*= 2);
                for (kk = 0; kk < count; kk++) lines[kk+1] = *lines + (lines[kk+1]-plines);
            }
            fprintf(term, " waiting...\r");
            len = fread(*lines+length, 1, capacity-length, file);
            for (kk = 0; kk < len; kk++) if ('\n' == (*lines)[length+kk]) {
                if ((char*)-1 == lines[++count]) {
                    lines = realloc(lines, count*2 * sizeof*lines);
                    lines[count*2] = (char*)-1;
                }
                lines[count] = *lines+length+kk+1;
            }
            length+= len;
            if ((flags.eof = feof(file)) && count <= k) {
                k = count-1;
                ed = k+row-1;
            }
            k--;
        } else {
            len = line_len(k);
            fprintf(term, "\x1b[K%.*s\r\n", len < col ? len : col, line_ptr(k));
        }
    }
}

void scroll_up(unsigned by) {
    if (count-1 < by+top) by = count-1-top;
    if (!by) return;
    fprintf(term, "\x1b[%uS", by);
    top+= by;
    move_to(1, row-by);
    draw_lines(top+row-by-1, top+row-1);
}

void scroll_dw(unsigned by) {
    if (top < by) by = top;
    if (!by) return;
    fprintf(term, "\x1b[%uT", by);
    top-= by;
    move_to(1, 1);
    draw_lines(top, top+by);
}

void scroll_to(unsigned to) {
    move_to(0, 0);
    draw_lines(to, to+row-1);
    top = count-1 < to ? count-1 : to;
}

void cleanup(void) {
    leave_alt();
    tcsetattr(fileno(term), TCSANOW, &term_in);

    if (stdin != file) fclose(file);
    if (lines) free(*lines);
    free(lines);
    fflush(term);
}

void setup(void) {
    struct winsize winsz;

    term = stderr;
    if (tcgetattr(fileno(term), &term_in)) perror(NULL), exit(EXIT_FAILURE);
    atexit(cleanup);

    if (!ioctl(fileno(term), TIOCGWINSZ, &winsz) && winsz.ws_row && winsz.ws_col) {
        row = winsz.ws_row;
        col = winsz.ws_col;
    }

    lines = malloc(row*2 * sizeof*lines);
    lines[row*2] = (char*)-1;
    *lines = malloc(capacity);

    term_raw = term_in;
    term_raw.c_iflag&=~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    term_raw.c_oflag&=~(OPOST);
    term_raw.c_lflag&=~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    term_raw.c_cflag&=~(CSIZE | PARENB);
    term_raw.c_cflag|= (CS8);

    tcsetattr(fileno(term), TCSANOW, &term_raw);
    enter_alt();
}

int main(int argc, char** argv) {
    unsigned k;
    char const* prog = (argc--, *argv++);
    file = stdin;
    if (argc && !strcmp("-h", argv[0])) return printf("Usage: %s [<file>]\n", prog), EXIT_SUCCESS;
    if (argc && strcmp("-", argv[0])) file = fopen(argv[0], "rb");
    if (!file) return perror(argv[0]), EXIT_FAILURE;

    setup();

    { char* msg = "", c = 'g';
        do {
            switch (c) {
                case 'q': case 'Q': case CTRL('Q'): return EXIT_SUCCESS;

                case 'j': case CTRL('N'): case CTRL('E'): scroll_up(1); break;
                case 'k': case CTRL('P'): case CTRL('Y'): scroll_dw(1); break;
                case 'd': case 'D': case CTRL('D'): scroll_up(row/2); break;
                case 'u': case 'U': case CTRL('U'): scroll_dw(row/2); break;
                case 'f': case 'F': case CTRL('F'): scroll_up(row-1); break;
                case 'b': case 'B': case CTRL('B'): scroll_dw(row-1); break;
                case 'g': scroll_to(0); break;
                case 'G': scroll_to(-1-row+1); break;

                case CTRL('['): switch (get_key(&c), c) {
                    case '[': switch (get_key(&c), c) {
                        case 'M':
                            get_key(&c); /* btn */
                            if (c & 64) /* wheel */ c & 3 ? scroll_up(3) : scroll_dw(3);
                            get_key(&c); /* x */
                            get_key(&c); /* y */
                            break;
                    } break;
                } break;

                case 'v': if(0) {
                    if (stdin != file) (void)0;
                    cleanup();
                    system("vi /tmp/moins-tmp");
                } break;

                case 'e': {
                    char* line;
                    if (get_line(&line)) {
                        FILE* f = fopen(line, "rb");
                        if (!f) msg = " -- could not open file for reading";
                        else {
                            length = 0;
                            count = 0;
                            flags.eof = 0;
                            scroll_to(0);
                        }
                    }
                } break;

                case 's': {
                    char* line;
                    fprintf(term, "\r\x1b[Ksave to: ");
                    if (get_line(&line)) {
                        unsigned ptop = top;
                        FILE* f = fopen(line, "wb");
                        if (!f) msg = " -- could not open file for writing";
                        else {
                            scroll_to(-1-row+1); /* force load */
                            fwrite(*lines, 1, length, f);
                            fclose(f);
                            scroll_to(ptop);
                        }
                    }
                } break;

                case '!': {
                    char* line;
                    fprintf(term, "\r\x1b[Kshell: ");
                    if (get_line(&line)) {
                        leave_alt();
                        tcsetattr(fileno(term), TCSANOW, &term_in);
                        system(line);
                        tcsetattr(fileno(term), TCSANOW, &term_raw);
                        get_key(&c);
                        enter_alt();
                        scroll_to(top);
                    }
                } break;

                case '-': switch (fprintf(term, "\r\x1b[Ktoggle flag: "), get_key(&c), c) {
                    case 'S': flags.wrap++;
                } break;
            }

            move_to(1, row);
            fprintf(term, "%*.s\x1b[m", col-fprintf(term, "\x1b[7m"
                        " file: %s  lines %d to %d of %d%s",
                            stdin == file ? "<stdin>" : argv[0],
                            top+1,
                            count < top+row-1 ? count : top+row-1,
                            flags.eof ? count : 0,
                            msg
                    )+3, "");

            fflush(term);
        } while (get_key(&c));
    }

    return EXIT_SUCCESS;
}
