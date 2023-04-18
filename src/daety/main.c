#define _XOPEN_SOURCE 501

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static char const* errfile;
static unsigned int errline;
static char const* errmsg;
static void _die() {
  char errbuf[4096];
  sprintf(errbuf, "[%s:%d] %s", errfile, errline, errmsg);
  perror(errbuf);
  exit(errno);
}

#define die(__msg) do {  \
    errfile = __FILE__;  \
    errline = __LINE__;  \
    errmsg = __msg;      \
    _die();              \
} while (1)
#define try(_v, _r) do {      \
    _v = _r;                  \
    if (_v < 0) {             \
        errfile = __FILE__;   \
        errline = __LINE__;   \
        errmsg = #_r;         \
        goto fail;            \
    }                         \
} while (0)

#define BUF_SIZE 65535

/// return full path of the fifo for argv[]
void identify(char* argv[], char* idf) {
    (void)argv;
    // TODO: lol
    idf[0] = '.';
    idf[1] = '/';
    idf[2] = 'm';
    idf[3] = 'e';
    idf[4] = '\0';
}

/// exec the program with pts as terminal
void program(int pts, char* argv[]) {

    if (dup2(pts, STDIN_FILENO) < 0
     || dup2(pts, STDOUT_FILENO) < 0
     || dup2(pts, STDERR_FILENO) < 0) {
        close(pts);
        die("dup2");
    }
    if (STDERR_FILENO < pts) close(pts);

    execvp(argv[0], argv);
    // puts("bidoof");
    // exit(EXIT_SUCCESS);

    die("execvp");
}

/// client loop (tty <-> fifo)
void client(int fifo) {
    // {{{ setup signal
    // TODO: implement proper working signal handlers (ISIG?)
    //signal(SIGTERM, SIG_IGN);
    //signal(SIGCONT, SIG_IGN);
    //signal(SIGSTOP, SIG_IGN);
    //signal(SIGTSTP, SIG_IGN);
    //signal(SIGWINCH, SIG_IGN);
    // }}}

    // {{{ stdin -> fifo -> stdout
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(fifo, &fds);

    while (1) {
        fd_set cpy = fds;
        int r;
        try(r, select(fifo+1, &cpy, NULL, NULL, NULL));

        int in = -1, out = -1;
        if (FD_ISSET(STDIN_FILENO, &cpy)) {
            in = STDIN_FILENO;
            out = fifo;
        } else if (FD_ISSET(fifo, &cpy)) {
            in = fifo;
            out = STDOUT_FILENO;
        } else continue;

        char buf[BUF_SIZE];
        ssize_t len = 0;
        try(len, read(in, buf, BUF_SIZE));
        char* esc = memchr(buf, '\e', len);
        if (0 == len || esc && esc-buf < len && '\0' == esc[1]) {
            break;
        }

        char const* at = buf;
        while (len) {
            ssize_t wrote;
            try(wrote, write(out, buf, len));

            at+= wrote;
            len-= wrote;
        }
    } // while (1)
    // }}}

    close(fifo);
    exit(EXIT_SUCCESS);

    return;
fail:
    close(fifo);
    _die();
}

/// server loop (fifo <-> ptm || watch cpid)
void server(int ptm, int fifo, pid_t cpid) {
    int logfd = -1;
    // umask(0);
    // chdir("/");
    // YYY: (drop other privileges)

    // {{{ put ptm as i/o, and log as e
    int r;
    try(r, dup2(ptm, STDIN_FILENO) || dup2(ptm, STDOUT_FILENO));
    if (STDERR_FILENO < ptm) close(ptm);

#define LOG_FILE "./log.log"
    try(logfd, creat(LOG_FILE, 420));
    try(r, dup2(logfd, STDERR_FILENO));
    if (logfd != STDERR_FILENO) close(logfd);
    // }}}

    // {{{ stdin -> fifo -> stdout
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(fifo, &fds);

    while (1) {
        fd_set cpy = fds;
        try(r, select(fifo+1, &cpy, NULL, NULL, NULL));

        int in = -1, out = -1;
        if (FD_ISSET(STDIN_FILENO, &cpy)) {
            in = STDIN_FILENO;
            out = fifo;
        } else if (FD_ISSET(fifo, &cpy)) {
            in = fifo;
            out = STDOUT_FILENO;
        } else continue;

        char buf[BUF_SIZE];
        ssize_t len = 0;
        try(len, read(in, buf, BUF_SIZE));
        if (0 == len) {
            // YYY: need kill(cpid) here?
            try(r, waitpid(cpid, NULL, 0));
            break;
        }

        char const* at = buf;
        while (len) {
            ssize_t wrote;
            try(wrote, write(out, buf, len));

            at+= wrote;
            len-= wrote;
        }

        int wst = 0;
        try(r, waitpid(cpid, &wst, WNOHANG));
        if (WIFEXITED(wst) || WIFSIGNALED(wst)) {
            break;
        }
        
    } // while (1)
    // }}}

    write(fifo, "\e\0", 2); // ZZZ: signal closing to client
    close(fifo);

    return;
fail:
    close(ptm);
    close(fifo);
    close(logfd);
    _die();
}

/// returns the fifo fd or -1 if it does not exist
int existing(char const* idf) {
    int fd = open(idf, O_RDWR|O_NOCTTY);
    if (fd < 0) {
        if (ENOENT == errno) {
            return -1;
        }
        die("open(idf)");
    }
    return fd;
}

/// open both ends and setup tio/ws
void multiplex(int* ptm, int* pts, struct termios const* tio, struct winsize const* ws) {
    // {{{ open both ends
    try(*ptm, open("/dev/ptmx", O_RDWR|O_NOCTTY));

    int r;
    try(r, grantpt(*ptm) || unlockpt(*ptm));

    char* ptname = ptsname(*ptm);

    try(*pts, open(ptname, O_RDWR|O_NOCTTY));
    // }}}

    // {{{ apply tio/ws
    if (tio) try(r, tcsetattr(*ptm, TCSAFLUSH, tio));
    if (ws) try(r, ioctl(*ptm, TIOCSWINSZ, ws));
    // }}}

    return;
fail:
    close(*ptm);
    close(*pts);
    _die();
}

/// returns the fifo fd; daemon does not return
int spawn(char* argv[], char const* idf, struct termios const* tio, struct winsize const* ws) {
    int ptm = -1, pts = -1, fifo = -1;
    multiplex(&ptm, &pts, tio, ws);

    // {{{ open fifo here
    int r;
    try(r, mkfifo(idf, 420));

    try(fifo, open(idf, O_RDWR|O_NOCTTY));
    // }}}

    // {{{ fork once, child will be server, parent resumes as client
    pid_t cpid;
    try(cpid, fork());
    if (0 < cpid) {
        close(ptm);
        close(pts);
        return fifo;
    }
    // }}}

    // {{{ setup for a daemon fork, with pts as controlling
    signal(SIGHUP, SIG_IGN); // ignore being detached from tty

    try(r, setsid());

#ifdef TIOCSCTTY
    // YYY: this needed?
    try(r, ioctl(pts, TIOCSCTTY, NULL));
#endif
    // }}}

    // {{{ here only daemon, last fork for the program itself
    try(r, fork());
    // }}}

    if (0 == cpid) {
        close(ptm);
        close(fifo);
        program(pts, argv);
    }

    close(pts);

    server(ptm, fifo, cpid);

    unlink(idf);
    exit(EXIT_SUCCESS);

    return -1;
fail:
    close(ptm);
    close(pts);
    close(fifo);
    _die();
    return -1;
}

void usage(char const* prog) {
    printf("Usage: %s <prog> <args...>\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    char const* prog = *argv++;
    if (0 == --argc) usage(prog);


    // {{{ copy from terminal on stderr
    int r;
    struct termios tio;
    try(r, tcgetattr(STDERR_FILENO, &tio));
    struct winsize ws;
    try(r, ioctl(STDERR_FILENO, TIOCGWINSZ, &ws));
    // }}}

    // {{{ look for existing daemon, or spawn
    char idf[256]; // YYY: buffer size
    identify(argv, idf);

    int fifo = existing(idf);
    if (fifo < 0) fifo = spawn(argv, idf, &tio, &ws);
    // }}}

    client(fifo);

    return EXIT_SUCCESS;
fail:
    _die();
    return EXIT_FAILURE;
}
