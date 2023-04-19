#include "inc.h"
#include "client.c"
#include "server.c"

char const* errfile;
unsigned int errline;
char const* errmsg;
void _die() {
  char errbuf[4096];
  sprintf(errbuf, "[%s:%d] %s", errfile, errline, errmsg);
  perror(errbuf);
  exit(errno);
}

/// return full path of the sock for argv[]
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

    // execvp(argv[0], argv);
    puts("bidoof");
    exit(EXIT_SUCCESS);

    die("execvp");
}

/// returns the sock fd or -1 if it does not exist
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

/// returns the sock fd; daemon does not return
int spawn(char* argv[], char const* idf, struct termios const* tio, struct winsize const* ws) {
    int ptm = -1, pts = -1, sock = -1;
    multiplex(&ptm, &pts, tio, ws);

    // {{{ open sock here
    int r;
    try(r, mkfifo(idf, 420));

    try(sock, open(idf, O_RDWR|O_NOCTTY));
    // }}}

    // {{{ fork once, child will be server, parent resumes as client
    pid_t cpid;
    try(cpid, fork());
    if (0 < cpid) {
        close(ptm);
        close(pts);
        return sock;
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
        close(sock);
        program(pts, argv);
    }

    close(pts);

    server(ptm, sock, cpid);

    unlink(idf);
    exit(EXIT_SUCCESS);

    return -1;
fail:
    close(ptm);
    close(pts);
    close(sock);
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

    int sock = existing(idf);
    if (sock < 0) sock = spawn(argv, idf, &tio, &ws);
    // }}}

    client(sock);

    return EXIT_SUCCESS;
fail:
    _die();
    return EXIT_FAILURE;
}
