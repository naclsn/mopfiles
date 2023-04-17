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

static char errbuf[4096];
#define die(__msg) {                                           \
    sprintf(errbuf, "[%s:%d] %s", __FILE__, __LINE__, __msg);  \
    perror(errbuf);                                            \
    exit(errno);                                               \
}
#define dieof(__eno, __msg) {  \
    errno = __eno;             \
    die(__msg);                \
}
#define log(__fmt, ...) {                                                          \
    ssize_t l = sprintf(errbuf, "[%s:%-3d] %s\n", __FILE__, __LINE__, __fmt);      \
    write(STDERR_FILENO, errbuf+l+1, sprintf(errbuf+l+1, errbuf, ##__VA_ARGS__));  \
}

#define BUF_SIZE 65535

/// return full path of the fifo for argv[]
void identify(char* argv[], char* idf) {
    log("both: identify");
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
    log("program: program('%s'..)", argv[0]);
    log("program: last message before dup2");

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
        log("client: select(stdin, fifo)");
        fd_set cpy = fds;
        if (select(fifo+1, &cpy, NULL, NULL, NULL) < 0) {
            close(fifo);
            die("select");
        }

        int in = -1, out = -1;
        if (FD_ISSET(STDIN_FILENO, &cpy)) {
            log("client: found input on stdin");
            in = STDIN_FILENO;
            out = fifo;
        } else if (FD_ISSET(fifo, &cpy)) {
            log("client: found input on fifo");
            in = fifo;
            out = STDOUT_FILENO;
        } else continue;

        char buf[BUF_SIZE];
        ssize_t len = 0;
        len = read(in, buf, BUF_SIZE);
        if (len < 0) {
            close(fifo);
            die("read");
        }
        char* esc = memchr(buf, '\e', len);
        if (0 == len || esc && esc-buf < len && '\0' == esc[1]) {
            log(esc ? "client: server done" : "client: read eof");
            break;
        }
        log("client: transferring %d byte(s)", len);

        char const* at = buf;
        while (len) {
            ssize_t wrote = write(out, buf, len);
            if (wrote < 0) {
                close(fifo);
                die("write");
            }

            at+= wrote;
            len-= wrote;
        }
    } // while (1)
    // }}}

    log("client: done");
    close(fifo);
    exit(EXIT_SUCCESS);
}

/// server loop (fifo <-> ptm || watch cpid)
void server(int ptm, int fifo, pid_t cpid) {
    // umask(0);
    // chdir("/");
    // YYY: (drop other privileges)

    // {{{ put ptm as i/o, and log as e
    if (dup2(ptm, STDIN_FILENO) < 0
     || dup2(ptm, STDOUT_FILENO) < 0) {
        close(ptm);
        die("dup2");
    }
    if (STDERR_FILENO < ptm) close(ptm);

#define LOG_FILE "./log.log"
    int log = creat(LOG_FILE, 420);
    log("server: see you in '%s'", LOG_FILE);
    if (log < 0) {
        close(ptm);
        close(fifo);
        die("open(log)");
    }
    if (dup2(log, STDERR_FILENO) < 0) {
        close(ptm);
        close(fifo);
        close(log);
        die("dup2");
    }
    if (log != STDERR_FILENO) close(log);
    // }}}

    // {{{ stdin -> fifo -> stdout
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(fifo, &fds);

    while (1) {
        log("server: select(stdin, fifo)");
        fd_set cpy = fds;
        if (select(fifo+1, &cpy, NULL, NULL, NULL) < 0) {
            close(fifo);
            die("select");
        }

        int in = -1, out = -1;
        if (FD_ISSET(STDIN_FILENO, &cpy)) {
            log("server: found input on stdin");
            in = STDIN_FILENO;
            out = fifo;
        } else if (FD_ISSET(fifo, &cpy)) {
            log("server: found input on fifo");
            in = fifo;
            out = STDOUT_FILENO;
        } else continue;

        char buf[BUF_SIZE];
        ssize_t len = 0;
        len = read(in, buf, BUF_SIZE);
        if (len < 0) {
            close(fifo);
            die("read");
        }
        if (0 == len) {
            log("server: read eof");
            log("server: last waiting");
            // YYY: need kill(cpid) here?
            if (waitpid(cpid, NULL, 0) < 0) {
                close(fifo);
                die("waitpid");
            }
            break;
        }
        log("server: transferring %d byte(s)", len);

        char const* at = buf;
        while (len) {
            ssize_t wrote = write(out, buf, len);
            if (wrote < 0) {
                close(fifo);
                die("write");
            }

            at+= wrote;
            len-= wrote;
        }

        int wst = 0;
        log("server: try waiting on program");
        if (waitpid(cpid, &wst, WNOHANG) < 0) {
            close(fifo);
            die("waitpid");
        }
        if (WIFEXITED(wst) || WIFSIGNALED(wst)) {
            log("server: program done");
            break;
        }
    } // while (1)
    // }}}

    log("server: done");
    write(fifo, "\e\0", 2); // ZZZ: signal closing to client
    close(fifo);
}

/// returns the fifo fd or -1 if it does not exist
int existing(char const* idf) {
    log("both: existing('%s')", idf);
    int fd = open(idf, O_RDWR|O_NOCTTY);
    if (fd < 0) {
        if (ENOENT == errno) {
            log("both: idf not found");
            return -1;
        }
        die("open(idf)");
    }
    log("both: idf found");
    return fd;
}

/// open both ends and setup tio/ws
void multiplex(int* ptm, int* pts, struct termios const* tio, struct winsize const* ws) {
    // {{{ open both ends
    *ptm = open("/dev/ptmx", O_RDWR|O_NOCTTY);
    if (*ptm < 0) die("open(ptmx)");

    if (grantpt(*ptm) < 0 || unlockpt(*ptm) < 0) {
        close(*ptm);
        die("grantpt/unlockpt");
    }

    char* ptname = ptsname(*ptm);

    *pts = open(ptname, O_RDWR|O_NOCTTY);
    if (*pts < 0) {
        close(*ptm);
        die("open(pts)");
    }
    log("both: pty pair open (%s)", ptname);
    // }}}

    // {{{ apply tio/ws
    if (tio && tcsetattr(*ptm, TCSAFLUSH, tio) < 0) {
        close(*ptm);
        close(*pts);
        die("tcsetattr");
    }
    if (ws && ioctl(*ptm, TIOCSWINSZ, ws) < 0) {
        close(*ptm);
        close(*pts);
        die("ioctl(TIOCSWINSZ)");
    }
    // }}}
}

/// returns the fifo fd; daemon does not return
int spawn(char* argv[], char const* idf, struct termios const* tio, struct winsize const* ws) {
    log("both: spawn('%s'.., '%s')", argv[0], idf);

    int ptm, pts;
    multiplex(&ptm, &pts, tio, ws);

    // {{{ open fifo here
    if (mkfifo(idf, 420) < 0) {
        close(ptm);
        close(pts);
        die("mkfifo");
    }

    int fifo = open(idf, O_RDWR|O_NOCTTY);
    if (fifo < 0) {
        close(ptm);
        close(pts);
        die("open(fifo)");
    }
    log("both: fifo created and opened");
    // }}}

    // {{{ fork once, child will be server, parent resumes as client
    pid_t cpid = fork();
    if (cpid < 0) {
        close(ptm);
        close(pts);
        close(fifo);
        die("fork");
    }
    if (0 < cpid) {
        close(ptm);
        close(pts);
        log("client: return from spawn with fifo");
        return fifo;
    }
    // }}}

    // {{{ setup for a daemon fork, with pts as controlling
    signal(SIGHUP, SIG_IGN); // ignore being detached from tty

    log("server: new session");
    if (setsid() < 0) {
        close(ptm);
        close(pts);
        close(fifo);
        die("setsid");
    }

#ifdef TIOCSCTTY
    // YYY: this needed?
    if (ioctl(pts, TIOCSCTTY, NULL) < 0) {
        close(ptm);
        close(pts);
        close(fifo);
        die("ioctl(TIOCSCTTY)");
    }
#endif
    // }}}

    // {{{ here only daemon, last fork for the program itself
    log("server: fork program");
    cpid = fork();
    if (cpid < 0) {
        close(ptm);
        close(pts);
        close(fifo);
        die("fork");
    }
    // }}}

    if (0 == cpid) {
        close(ptm);
        close(fifo);
        program(pts, argv);
    }

    log("server: continuing");
    close(pts);

    server(ptm, fifo, cpid);

    unlink(idf);
    exit(EXIT_SUCCESS);

    return -1; // unreachable
}

void usage(char const* prog) {
    printf("Usage: %s <prog> <args...>\n", prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    char const* prog = *argv++;
    if (0 == --argc) usage(prog);

    log("both: entered main");

    // {{{ copy from terminal on stderr
    struct termios tio;
    if (tcgetattr(STDERR_FILENO, &tio) < 0) die("tcgetattr");

    struct winsize ws;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) < 0) die("ioctl(TIOCGWINSZ)");
    // }}}

    // {{{ look for existing daemon, or spawn
    char idf[256]; // YYY: buffer size
    identify(argv, idf);

    int fifo = existing(idf);
    if (fifo < 0) fifo = spawn(argv, idf, &tio, &ws);
    // }}}

    client(fifo);
}
