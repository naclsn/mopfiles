#define _XOPEN_SOURCE 543

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static char errbuf[4096];
#define die(__fmt, ...) {                   \
    sprintf(errbuf, __fmt, ##__VA_ARGS__);  \
    perror(errbuf);                         \
    exit(errno);                            \
}
#define dieof(__eno, __fmt, ...) {  \
    errno = __eno;                  \
    die(__fmt, ##__VA_ARGS__);      \
}

pid_t forkpty(int* master, char* name, struct termios* tio, struct winsize* ws) {
    *master = open("/dev/ptmx", O_RDWR|O_NOCTTY);
    if (master < 0) die("open(ptmx)");

    if (grantpt(*master) < 0 || unlockpt(*master) < 0) {
        close(*master);
        *master = -1;
        die("grantpt/unlockpt");
    }

    if (name) strcpy(name, ptsname(*master));

    int slave = open(name ? name : ptsname(*master), O_RDWR|O_NOCTTY);
    if (slave < 0) {
        close(*master);
        *master = -1;
        die("open(pts)");
    }

    pid_t r = fork();
    if (0 == r) {
        close(*master);

        setsid();
#ifdef TIOCSCTTY
        if (ioctl(slave, TIOCSCTTY, NULL) < 0) die("ioctl(TIOCSCTTY)");
#endif

        if (tio && tcsetattr(slave, TCSAFLUSH, tio) < 0) die("tcsetattr");
        if (ws && ioctl(slave, TIOCSWINSZ, ws) < 0) die("ioctl(TIOCSWINSZ)");

        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        if (STDERR_FILENO < slave) close(slave);
        return r;
    }

    close(slave);
    return r;
}

void usage(char const* prog) {
    printf("Usage: %s <prog> <args...>\n", prog);
    exit(1);
}

int main(int argc, char* argv[]) {
    char const* prog = *argv++;
    if (0 == --argc) usage(prog);

    int ma = -1;

    struct winsize ws;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) < 0) die("ioctl(TIOCGWINSZ)");

    pid_t cpid = forkpty(&ma, NULL, NULL, &ws);
    if (cpid < 0) die("forkpty");

    if (0 == cpid) {
        execvp(argv[0], argv);
        die("execvp");
    }

    struct termios tio;
    if (tcgetattr(ma, &tio) < 0) die("tcgetattr");
    // tio.c_lflag&= ~(ECHO | ECHONL);
    if (tcsetattr(ma, TCSAFLUSH, &tio) < 0) die("tcsetattr");

    bool done = false;
    do {
        char buf[1024];
        // char* cur;
        ssize_t len;

        while ((len = read(STDIN_FILENO, buf, 1024))) {
            if (len < 0) die("read(STDIN_FILENO)");
            if (write(ma, buf, len) < 0) die("write(ma)");
        }

        while ((len = read(ma, buf, 1024))) {
            int wst = 0;
            if (waitpid(cpid, &wst, WNOHANG) < 0) die("waitpid");
            if (WIFEXITED(wst) || WIFSIGNALED(wst)) done = true;

            if (len < 0) die("read(ma)");
            if (write(STDOUT_FILENO, buf, len) < 0) die("write(STDOUT_FILENO)");
        }
    } while (!done);

    close(ma);
}
