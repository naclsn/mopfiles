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

void identify(char* argv[], char* idf) {
    (void)argv;
    // TODO: lol
    idf[0] = '.';
    idf[1] = '/';
    idf[2] = 'm';
    idf[3] = 'e';
    idf[4] = '\0';
}

// stdin -> fifo -> stdout
void loop(int fifo) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(fifo, &fds);

    bool done;
    do {
        fd_set cpy = fds;
        if (select(fifo+1, &cpy, NULL, NULL, NULL) < 0) {
            close(fifo);
            die("select");
        }

        int in = -1, out = -1;
        if (FD_ISSET(STDIN_FILENO, &cpy)) {
            in = STDIN_FILENO;
            out = fifo;
        } else if (FD_ISSET(fifo, &cpy)) {
            in = fifo;
            out = STDOUT_FILENO;
        } else continue;

#define BUF_SIZE 65535
        char buf[BUF_SIZE];
        ssize_t len = read(in, buf, BUF_SIZE);
        if (len < 0) {
            close(fifo);
            die("read");
        }
        if (0 == len) done = true;

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

    } while (!done);
}

/// returns the pts fd or -1 if it does not exist
int existing(char const* idf) {
    int fd = open(idf, O_RDWR|O_NOCTTY);
    if (fd < 0) {
        if (ENOENT == errno) return -1;
        die("open(idf)");
    }
    return fd;
}

/// returns the fifo fd; daemon does not return
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

    // {{{ apply tio/ws
    if (tio && tcsetattr(ptm, TCSAFLUSH, tio) < 0) {
        close(ptm);
        close(pts);
        die("tcsetattr");
    }
    if (ws && ioctl(ptm, TIOCSWINSZ, ws) < 0) {
        close(ptm);
        close(pts);
        die("ioctl(TIOCSWINSZ)");
    }
    // }}}

    // {{{ open fifo here (fail early or something)
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
        return fifo;
    }
    // }}}

    // {{{ setup for a daemon fork, with pts as controlling (hence double fork not needed)
    signal(SIGHUP, SIG_IGN); // ignore being detached from tty

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
    cpid = fork();
    if (cpid < 0) {
        close(ptm);
        close(pts);
        close(fifo);
        die("fork");
    }
    // }}}

    if (0 == cpid) {
        // {{{ keeps only pts: setup i/o/e and exec
        close(ptm);
        close(fifo);

        if (dup2(pts, STDIN_FILENO) < 0
         || dup2(pts, STDOUT_FILENO) < 0
         || dup2(pts, STDERR_FILENO) < 0) {
            close(pts);
            die("dup2");
        }
        if (STDERR_FILENO < pts) close(pts);

        execvp(argv[0], argv);

        die("execvp");
        // }}}
    }

    // YYY: not sure I want any of that...
    //umask(0);
    //chdir("/");
    // (drop other privileges)

    close(pts);

    if (dup2(ptm, STDIN_FILENO) < 0
     || dup2(ptm, STDOUT_FILENO) < 0) {
        close(ptm);
        die("dup2");
    }
    if (STDERR_FILENO < ptm) close(ptm);

    int log = creat("./log.log", 420);
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

    loop(fifo); // YYY: also has to monitor child if it finished
    close(fifo);

    // YYY: need kill(cpid) here?
    waitpid(cpid, NULL, 0);

    exit(EXIT_SUCCESS);
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

    struct winsize ws;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) < 0) die("ioctl(TIOCGWINSZ)");
    // }}}

    // {{{ look for existing daemon, or spawn
    char idf[256]; // YYY: buffer size
    identify(argv, idf);

    int fifo = existing(idf);
    if (fifo < 0) fifo = spawn(argv, idf, &tio, &ws);
    // }}}

    // {{{ setup signal
    // TODO: implement proper working signal handlers (ISIG?)
    //signal(SIGTERM, SIG_IGN);
    //signal(SIGCONT, SIG_IGN);
    //signal(SIGSTOP, SIG_IGN);
    //signal(SIGTSTP, SIG_IGN);
    //signal(SIGWINCH, SIG_IGN);
    // }}}

    loop(fifo);
    close(fifo);

    return EXIT_SUCCESS;
}
