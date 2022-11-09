#define _XOPEN_SOURCE 501

/* TODOs:
 - fork before exec, parent will clean up idf
 - catch and forward signals from client
 - synchronised winsize:
    - at `existing`
    - on client `SIGWINCH`
    - on server `SIGWINCH`
 - stop client without stopping server
 - (other feats if y nat)
 - C things:
    - local buffers sizes
    - sprintf/sscanf
*/

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
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

void identify(char* argv[], char* idf) {
    (void)argv;
    // TODO: lol
    idf[0] = '.';
    idf[1] = '/';
    idf[2] = 'm';
    idf[3] = 'e';
    idf[4] = '\0';
}

/// returns the pts fd or -1 if it does not exist
int existing(char const* idf) {
    int fd = open(idf, O_RDONLY|O_NOCTTY);
    if (fd < 0) {
        if (ENOENT == errno) return -1;
        die("open(idf)");
    }

    char buf[1024]; // YYY: buffer size
    char* at;
    ssize_t off = 0;
    do {
        off = read(fd, at, 1024-(at-buf));
        if (off < 0) {
            close(fd);
            return -1;
        }
        at+= off;
    } while (off);
    *at = '\0';

    char ptname[256]; // YYY: buffer size
    pid_t dpid;
    sscanf(buf, "%s\n%d\n", ptname, &dpid);
    close(fd);

    fd = open(ptname, O_RDWR|O_NOCTTY); // TODO: may have things to do (eg send SIGWINCH?)
    if (fd < 0) die("open(pts)")
    return fd;
}

/// returns the pts fd; daemon does not return
int spawn(char* argv[], char const* idf, struct termios const* tio, struct winsize const* ws) {
    // {{{ open both ends
    int ptm = open("/dev/ptmx", O_RDWR|O_NOCTTY);
    if (ptm < 0) die("open(ptmx)");

    if (grantpt(ptm) < 0 || unlockpt(ptm) < 0) {
        close(ptm);
        die("grantpt/unlockpt");
    }

    char ptname[256]; // YYY: buffer size
    strcpy(ptname, ptsname(ptm));

    int pts = open(ptname, O_RDWR|O_NOCTTY);
    if (pts < 0) {
        close(ptm);
        die("open(pts)");
    }
    // }}}

    // {{{ fork once, child only keeps ptm, parent resumes as client
    pid_t cpid = fork();
    if (cpid < 0) {
        close(ptm);
        close(pts);
        die("fork");
    }
    if (0 < cpid) {
        close(ptm);
        return pts;
    }
    close(pts);
    // }}}

    // {{{ setup for a daemon f-fork, with ptm as controlling
    signal(SIGHUP, SIG_IGN); // ignore being detached from tty
    // XXX: what about an already set handler? can probably re-install it

    if (setsid() < 0) {
        close(ptm);
        die("setsid");
    }

#ifdef TIOCSCTTY
    // YYY: this needed?
    if (ioctl(ptm, TIOCSCTTY, NULL) < 0) {
        close(ptm);
        die("ioctl(TIOCSCTTY)");
    }
#endif

    cpid = fork();
    if (cpid < 0) {
        close(ptm);
        die("fork");
    }
    if (0 < cpid) {
        close(ptm);
        exit(EXIT_SUCCESS);
    }
    // }}}

    // {{{ post a letter with its address on it
    {
        int me = creat(idf, 420);
        if (me < 0) {
            close(ptm);
            die("creat");
        }
        char buf[1024]; // YYY: buffer size
        int len = sprintf(buf, "%s\n%d\n", ptname, getpid());
        char const* at = buf;
        do {
            ssize_t wrote = write(me, at, len);
            if (wrote < 0) {
                close(me);
                close(ptm);
                die("write");
            }
            at+= wrote;
            len-= wrote;
        } while (len);
        fsync(me);
        close(me);
    }
    // }}}

    // {{{ here only daemon, setup i/o/e and exec
    if (tio && tcsetattr(ptm, TCSAFLUSH, tio) < 0) {
        close(ptm);
        die("tcsetattr");
    }
    if (ws && ioctl(ptm, TIOCSWINSZ, ws) < 0) {
        close(ptm);
        die("ioctl(TIOCSWINSZ)");
    }

    if (dup2(ptm, STDIN_FILENO) < 0
     || dup2(ptm, STDOUT_FILENO) < 0
     || dup2(ptm, STDERR_FILENO) < 0) {
        close(ptm);
        die("dup2");
    }
    if (STDERR_FILENO < ptm) close(ptm);

    // YYY: not sure I want any of that...
    //umask(0);
    //chdir("/");
    // (drop other privileges)

    // TODO: fork one last time: child execs;
    // parent waits on child and clean ifd
    execvp(argv[0], argv);

    close(ptm);
    die("execvp");
    // }}}
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
    struct termios tio;
    if (tcgetattr(STDERR_FILENO, &tio) < 0) die("tcgetattr");
    // cfmakeraw: 1960 magic shit
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tio.c_oflag &= ~(OPOST);
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN); // YYY: ISIG? (generate signals for INTR, QUIT, SUSP and DSUSP)
    tio.c_cflag &= ~(CSIZE | PARENB);
    tio.c_cflag |= CS8;

    struct winsize ws;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) < 0) die("ioctl(TIOCGWINSZ)");
    // }}}

    // {{{ look for existing daemon, or spawn
    char idf[256]; // YYY: buffer size
    identify(argv, idf);

    int pts = existing(idf);
    if (pts < 0) pts = spawn(argv, idf, &tio, &ws);
    // }}}

    // {{{ setup signal
    // TODO: implement proper working signal handlers (ISIG?)
    //signal(SIGTERM, SIG_IGN);
    //signal(SIGCONT, SIG_IGN);
    //signal(SIGSTOP, SIG_IGN);
    //signal(SIGTSTP, SIG_IGN);
    //signal(SIGWINCH, SIG_IGN);
    // }}}

    // {{{ stdin -> pts -> stdout
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(pts, &fds);

    bool done;
    do {
        fd_set cpy = fds;
        if (select(pts+1, &cpy, NULL, NULL, NULL) < 0) {
            close(pts);
            die("select");
        }

        int in = -1, out = -1;
        if (FD_ISSET(STDIN_FILENO, &cpy)) {
            in = STDIN_FILENO;
            out = pts;
        } else if (FD_ISSET(pts, &cpy)) {
            in = pts;
            out = STDOUT_FILENO;
        } else continue;

#define BUF_SIZE 65535
        char buf[BUF_SIZE];
        ssize_t len = read(in, buf, BUF_SIZE);
        if (len < 0) {
            close(pts);
            die("read");
        }
        if (0 == len) done = true;

        char const* at = buf;
        while (len) {
            ssize_t wrote = write(out, buf, len);
            if (wrote < 0) {
                close(pts);
                die("write");
            }

            at+= wrote;
            len-= wrote;
        }

    } while (!done);
    // }}}

    close(pts);
    return EXIT_SUCCESS;
}
