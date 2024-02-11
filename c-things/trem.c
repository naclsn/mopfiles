#define _XOPEN_SOURCE // for putenv

#include "utils/types.h"
#define FRAME_IMPLEMENTATION
#include "utils/frame.h"
#define TEXT_IMPLEMENTATION
#include "utils/text.h"

#include <pthread.h>
#include <pty.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define _HERE_STR(__ln) #__ln
#define _HERE_XSTR(__ln) _HERE_STR(__ln)
#define HERE __FILE__ ":" _HERE_XSTR(__LINE__)
#define exitf(...) (printf(HERE ": " __VA_ARGS__), putchar('\n'), exit(1))

// TODO: would be neater to have 4x8 font for "half-width" characters
#define FONT_WIDTH 8
#define FONT_HEIGHT 12
#define INIT_COLS 42
#define INIT_ROWS 24

// TODO: change scale with ctrl+wheel?
static float const scale = 2;

int pty = 0;
pid_t pid = 0;
pthread_t thr = 0;
Frame* frm = NULL;
struct screen {
    dyarr(buf) lines;
    sz col, row;
    bool hid;
} shl = {0}, alt = {0}, *cur = &shl;
sz cols = INIT_COLS, rows = INIT_ROWS;
struct kbstate {
    bool shift;
    bool ctrl;
    bool alt;
    bool caplock;
} kbs = {0};

void cleanup(void) {
    for (sz k = 0; k < rows; k++) {
        free(shl.lines.ptr[k].ptr);
        free(alt.lines.ptr[k].ptr);
    }
    free(shl.lines.ptr);
    free(alt.lines.ptr);
    text_free();
    if (frm) frame_destroy(frm);
    if (thr) pthread_join(thr, NULL);
    if (pty) close(pty);
    if (pid) waitpid(pid, NULL, 0);
}

void render(Frame* frame) {
    (void)frame;

    // TODO: on frame resize
    float w = frame->width, h = frame->height;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w/scale, h/scale, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glClearColor(.12f, .15f, .18f, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!cur->hid) {
        glColor4f(.42, .72, .12, 1);
        glRectf(cur->col*FONT_WIDTH, cur->row*FONT_HEIGHT, (cur->col+1)*FONT_WIDTH, (cur->row+1)*FONT_HEIGHT);
    }

    glColor4f(1, .4, .7, 1);
    for (sz i = 0; i < rows; i++) {
        buf const ln = cur->lines.ptr[i];
        text_draw((char*)ln.ptr, ln.len, 0, FONT_HEIGHT*i+3);
    }
}

void resize(Frame* frame) {
    (void)frame;
    // TODO
}

void closing(Frame* frame) {
    if (kill(pid, SIGTERM)) kill(pid, SIGKILL);
    pthread_cancel(thr);
    frame_close(frame);
}

// TODO(frame.h): fix kb layout
u8 key2byte(unsigned key) {
    switch (key) {
        case KEY_BACKSPACE: return 127;
        case KEY_TAB:       return CTRL('I');
        case KEY_RETURN:    return CTRL('M');
        case KEY_ESC:       return CTRL('[');
        default: if (' ' <= key && key <= '~') {
            if (kbs.ctrl && (
                    '2' == key || // '@'
                    ('a' <= key && key <= 'z') ||
                    '[' == key || ']' == key || '\\' == key || '^' == key || '_' == key))
                return CTRL(key);
            if (kbs.caplock && 'a' <= key && key <= 'z')
                return !kbs.shift ? key&~32 : key;
            if (kbs.shift) switch (key) {
                case '`': return '~';
                case '1': return '!';
                case '2': return '@';
                case '3': return '#';
                case '4': return '$';
                case '5': return '%';
                case '6': return '^';
                case '7': return '&';
                case '8': return '*';
                case '9': return '(';
                case '0': return ')';
                case '-': return '_';
                case '=': return '+';
                case '[': return '{';
                case ']': return '}';
                case'\\': return '|';
                case ';': return ':';
                case'\'': return '"';
                case ',': return '<';
                case '.': return '>';
                case '/': return '?';
                default:  return 'a' <= key && key <= 'z' ? key&~32 : key;
            } else return key;
        }
    } // switch key
    printf("unhandled key: 0x%02X\n", key);
    return 0; // TODO: don't return anything!
}

void keydown(Frame* frame, unsigned key) {
    (void)frame;
    switch (key) {
        case KEY_LSHIFT: case KEY_RSHIFT: kbs.shift = true; break;
        case KEY_LCTRL:  case KEY_RCTRL:  kbs.ctrl = true;  break;
        case KEY_LALT:   case KEY_RALT:   kbs.alt = true;   break;
        case KEY_CAPLOCK: kbs.caplock = !kbs.caplock;       break;

        default:
            if (kbs.alt) write(pty, (u8[]){CTRL('['), key2byte(key)}, 2);
            else write(pty, (u8[]){key2byte(key)}, 1);
    }
}

void keyup(Frame* frame, unsigned key) {
    (void)frame;
    switch (key) {
        case KEY_LSHIFT: case KEY_RSHIFT: kbs.shift = false; break;
        case KEY_LCTRL:  case KEY_RCTRL:  kbs.ctrl = false;  break;
        case KEY_LALT:   case KEY_RALT:   kbs.alt = false;   break;
    }
}

void scroll(void) {
    buf const dropped = cur->lines.ptr[0];
    for (sz k = 1; k < rows; k++) cur->lines.ptr[k-1] = cur->lines.ptr[k];
    cur->lines.ptr[rows-1] = dropped;
    memset(cur->lines.ptr[rows-1].ptr, ' ', cols);
}
void unscroll(void) {
    buf const dropped = cur->lines.ptr[rows-1];
    for (sz k = rows-2; 0 != k; k--) cur->lines.ptr[k] = cur->lines.ptr[k-1];
    cur->lines.ptr[0] = dropped;
    memset(cur->lines.ptr[0].ptr, ' ', cols);
}

bool put(u8 b) {
    static enum {
        NORMAL,
        ESCAPE_INTER,
        ESCAPE_FINAL,
        CTRL_SEQ_PARAM,
        CTRL_SEQ_INTER,
        CTRL_SEQ_FINAL,
    } state = NORMAL;
    static u8 acc[1024];
    static unsigned at = 0;

    switch (state) {
        case NORMAL: switch (b) {
            case CTRL('G'): // \a
                puts("hi :3");
                return false;
            case 127: // ^?
            case CTRL('H'): // \b
                if (cur->col) cur->col--;
                return true;
            case CTRL('I'): // \t
                cur->col = (cur->col/8+1)*8;
                if (cols <= cur->col) cur->col = cols-1;
                return true;
            case CTRL('J'): // \n
            case CTRL('K'): // \v  XXX: what is the "standard" behavior for this one?
                if (rows == ++cur->row) cur->row--, scroll();
                return true;
            case CTRL('L'): // \f
                for (sz k = 0; k < rows; k++) memset(cur->lines.ptr[k].ptr, ' ', cols);
                cur->row = 0;
                return true;
            case CTRL('M'): // \r
                cur->col = 0;
                return true;
            case CTRL('['): // \e
                at = 0;
                return state = ESCAPE_INTER, false;
            default:
                if (' ' <= b && b <= '~') {
                    cur->lines.ptr[cur->row].ptr[cur->col++] = b;
                    if (cols == cur->col) {
                        cur->col = 0;
                        if (rows == ++cur->row) cur->row--, scroll();
                    }
                    return true;
                }
                // TODO: utf8
                printf("unhandled in NORMAL: 0x%02X\n", b);
                return false;
        } // NORMAL

        case ESCAPE_INTER:
            acc[at < countof(acc) ? at++ : at] = b;
            if (0x20 <= b && b <= 0x2f) return false;
            if (0x30 <= b && b < 0x7f) return state = ESCAPE_FINAL, put(b);
            printf("unexpected in ESCAPE_INTER: 0x%02X\n", b);
            return state = NORMAL, false;
        case ESCAPE_FINAL: switch (acc[at-1]) {
            case '[':
                at = 0;
                return state = CTRL_SEQ_PARAM, false;
            default:
                printf("unhandled in ESCAPE_FINAL: 0x%02X\n", b);
                return state = NORMAL, false;
        }

        case CTRL_SEQ_PARAM:
            acc[at < countof(acc) ? at++ : at] = b;
            if (0x30 <= b && b <= 0x3f) return false;
            if (0x20 <= b && b <= 0x2f) return state = CTRL_SEQ_INTER, put(b);
            if (0x40 <= b && b < 0x7f) return state = CTRL_SEQ_FINAL, put(b);
            printf("unexpected in CTRL_SEQ_PARAM: 0x%02X\n", b);
            return state = NORMAL, false;
        case CTRL_SEQ_INTER:
            acc[at < countof(acc) ? at++ : at] = b;
            if (0x20 <= b && b <= 0x2f) return false;
            if (0x40 <= b && b < 0x7f) return state = CTRL_SEQ_FINAL, put(b);
            printf("unexpected in CTRL_SEQ_INTER: 0x%02X\n", b);
            return state = NORMAL, false;
        case CTRL_SEQ_FINAL: switch (acc[at-1]) {
            case 'F': cur->col = 0; // fallthrough
            case 'A': {
                unsigned by = 1 < at ? acc[at-1] = '\0', atoi((char*)acc) : 1;
                cur->row = cur->row < by ? 0 : cur->row-by;
            } return state = NORMAL, true;
            case 'E': cur->col = 0; // fallthrough
            case 'B': {
                unsigned by = 1 < at ? acc[at-1] = '\0', atoi((char*)acc) : 1;
                cur->row = rows-1-cur->row < by ? rows-1 : cur->row+by;
            } return state = NORMAL, true;
            case 'C': {
                unsigned by = 1 < at ? acc[at-1] = '\0', atoi((char*)acc) : 1;
                cur->col = cols-1-cur->col < by ? cols-1 : cur->col+by;
            } return state = NORMAL, true;
            case 'D': {
                unsigned by = 1 < at ? acc[at-1] = '\0', atoi((char*)acc) : 1;
                cur->col = cur->col < by ? 0 : cur->col-by;
            } return state = NORMAL, true;
            case 'G': {
                unsigned to = 1 < at ? acc[at-1] = '\0', atoi((char*)acc) -1 : 0;
                cur->col = to < cols ? to : cols-1;
            } return state = NORMAL, true;
            case 'f': printf("not implemented: formatting\n"); // fallthrough
            case 'H': {
                acc[at-1] = '\0';
                char* end;
                cur->row = ';' != *acc ? strtol((char*)acc, &end, 10) -1 : 0;
                cur->col = ';' != *end && 'H' != *end ? atoi((char*)end+1) -1 : 0;
                if (rows <= cur->row) cur->row = rows-1;
                if (cols <= cur->col) cur->col = cols-1;
            } return state = NORMAL, true;
            case 'J':
                if (1 == at || strchr("023", acc[at-2])) {
                    memset(cur->lines.ptr[cur->row].ptr+cur->col, ' ', cols-1-cur->col);
                    for (sz k = cur->row+1; k < rows; k++) memset(cur->lines.ptr[k].ptr, ' ', cols);
                }
                if (1 != at && strchr("123", acc[at-2])) {
                    for (sz k = 0; k < cur->row; k++) memset(cur->lines.ptr[k].ptr, ' ', cols);
                    memset(cur->lines.ptr[cur->row].ptr, ' ', cur->col);
                }
                return state = NORMAL, true;
            case 'K':
                if (1 == at || strchr("02", acc[at-2]))
                    memset(cur->lines.ptr[cur->row].ptr+cur->col, ' ', cols-1-cur->col);
                if (1 != at && strchr("12", acc[at-2]))
                    memset(cur->lines.ptr[cur->row].ptr, ' ', cur->col);
                return state = NORMAL, true;
            case 'S': {
                unsigned by = 1 < at ? acc[at-1] = '\0', atoi((char*)acc) -1 : 0;
                for (unsigned k = 0; k < by; k++) scroll();
            } return state = NORMAL, true;
            case 'T': {
                unsigned by = 1 < at ? acc[at-1] = '\0', atoi((char*)acc) -1 : 0;
                for (unsigned k = 0; k < by; k++) unscroll();
            } return state = NORMAL, true;
            case 'h':
            case 'l': if ('?' == acc[0]) {
                if (!memcmp("25", acc+1, 2)) {
                    cur->hid = 'h' == acc[at-1];
                    return state = NORMAL, true;
                }
                if (!memcmp("1049", acc+1, 4)) {
                    cur = 'h' == acc[at-1] ? &alt : &shl;
                    return state = NORMAL, true;
                }
                if (!memcmp("1004", acc+1, 4)) {
                    printf("not implemented: focussing\n");
                    return state = NORMAL, true;
                }
                if (!memcmp("2004", acc+1, 4)) {
                    printf("not implemented: pasting\n");
                    return state = NORMAL, true;
                }
            } return acc[at-1] = '\0', put(b); // YYY: trigger unhandled
            case 'm':
                printf("TODO: sgr: <<%.*s>>\n", at, acc);
                return state = NORMAL, false;
            case 'n': {
                char buf[1024];
                write(pty, buf, sprintf(buf, "\x1b[%zu;%zuR", cur->row, cur->col));
            } return state = NORMAL, false;
            default:
                printf("unhandled control sequence: <<%.*s>>\n", at, acc);
                return state = NORMAL, false;
         }
    } // switch state

    // normally unreachable
    return false;
} // put

Frame frame = {
    .width= scale*FONT_WIDTH*INIT_COLS,
    .height= scale*FONT_HEIGHT*INIT_ROWS,
    .events= {
        .render= render,
        .closing= closing,
        .keydown= keydown,
        .keyup= keyup,
        // TODO: mouse
    },
};

void* readloop(void* _) {
    (void)_;
    u8 buf[1024];
    ssize_t n;
    while (0 < (n = read(pty, buf, countof(buf)))) {
        bool up = false;
        for (ssize_t k = 0; k < n; k++) up|= put(buf[k]);
        if (up) frame_redraw(&frame);
    }
    frame_close(&frame);
    return NULL;
}

int main(int argc, char** argv) {
    if (argv++, !--argc) {
        static char* defaultargs[2] = {"/bin/sh", NULL};
        argc = 1;
        argv = defaultargs;
    }

    putenv("TERM=ansi");
    pid = forkpty(&pty, NULL, NULL, &(struct winsize){.ws_row= rows, .ws_col= cols});
    if (pid < 0) exitf("forkpty failed");
    if (!pid) return execvp(argv[0], argv);

    atexit(cleanup);

    // TODO: on frame resize
    if (!dyarr_resize(&shl.lines, shl.lines.len=rows)) exitf("OOM");
    for (sz k = 0; k < rows; k++) {
        if (!dyarr_resize(&shl.lines.ptr[k], (shl.lines.ptr[k].len=cols) *1.5)) exitf("OOM");
        memset(shl.lines.ptr[k].ptr, ' ', cols);
    }
    if (!dyarr_resize(&alt.lines, alt.lines.len=rows)) exitf("OOM");
    for (sz k = 0; k < rows; k++) {
        if (!dyarr_resize(&alt.lines.ptr[k], (alt.lines.ptr[k].len=cols) *1.5)) exitf("OOM");
        memset(alt.lines.ptr[k].ptr, ' ', cols);
    }

    char title[1024];
    sz len = strlen(argv[0]);
    memcpy(title, argv[0], len);
    title[len++] = ':';
    getcwd(title+len, countof(title)-len);
    frame.title = title;

    XInitThreads();
    if (!frame_create(&frame)) exitf("could not create window");
    frm = &frame;

    if (pthread_create(&thr, NULL, readloop, NULL)) exitf("could not create thread");

    text_init();
    frame_loop(&frame);

    return 0;
}
