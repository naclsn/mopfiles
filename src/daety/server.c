#include "server.h"

#define IDX_SOCK 0
#define IDX_TERM 1
#define IDX_CLIS 2
#define IDX_COUNT 8

static char const* filename = NULL; // name of a file to remove at cleanup
static struct pollfd fds[IDX_COUNT];
static int fds_count = 0;
static pid_t cpid = 0;
static struct winsize wss[IDX_COUNT-IDX_CLIS];
static struct winsize curr_ws = {.ws_col= 80, .ws_row= 24};
static bool is_alt = false;
static bool terminate = false;

/// cleanup SIGINT handler
static void cleanup(int sign) {
  #define lastsay(_c) write(STDOUT_FILENO, "server: " _c "\n", strlen("server: " _c "\n"));

  lastsay("cleaning");
  if (filename) unlink(filename);

  if (terminate) {
    // program did not terminate by itself (or from user input)
    lastsay("terminating program (1s)");
    kill(cpid, SIGTERM);
    sleep(1);
  }

  // get exit code
  int wst;
  char code = 0;
  if (0 == waitpid(cpid, &wst, WNOHANG)) {
    lastsay("program is not stopping.. waiting");
    kill(cpid, SIGTERM);
    sleep(3);
    // try again before escalating
    if (0 == waitpid(cpid, &wst, WNOHANG)) {
      lastsay("program still not stopping, killing");
      kill(cpid, SIGKILL);
      waitpid(cpid, &wst, 0);
    }
  }
  if (WIFEXITED(wst)) code = WEXITSTATUS(wst);

  lastsay("closing connections")
  for (int k = 0; k < fds_count; k++) {
    // send exit code to client
    write(fds[k].fd, &code, 1);
    close(fds[k].fd);
  }

  lastsay("done");
  if (sign) _exit(0);
}

/// fork and start program on a new pty
static pid_t fork_program(char** args) {
  pid_t cpid;
  try(cpid, forkpty(&fds[IDX_TERM].fd, NULL, NULL, NULL));
  fds_count++;

  // parent (server)
  if (0 < cpid) return cpid;

  // child (program)
  try(cpid, execvp(args[0], args)); // TODO: catch and report err

finally:
  _die();
  return -1;
}

/// update the term winsize and curr_ws (fails if ioctl does)
static int update_winsize(bool quiet) {
  if (IDX_CLIS < fds_count) {
    // find new smaller size
    curr_ws.ws_col = 0xffff;
    curr_ws.ws_row = 0xffff;
    for (int k = 0; k < fds_count-IDX_CLIS; k++) {
      if (wss[k].ws_col < curr_ws.ws_col) curr_ws.ws_col = wss[k].ws_col;
      if (wss[k].ws_row < curr_ws.ws_row) curr_ws.ws_row = wss[k].ws_row;
    }
  } else {
    // use a 'standard' 80x24
    curr_ws.ws_col = 80;
    curr_ws.ws_row = 24;
  }

  if (!quiet) printf("server: new size %dx%d\n", curr_ws.ws_col, curr_ws.ws_row);
  return ioctl(fds[IDX_TERM].fd, TIOCSWINSZ, &curr_ws);
}

/// output using ^x sequences
static void putesc(char const* buf, int len) {
  do {
    if (*buf < 0x20) {
      putchar('^');
      putchar(*buf | 0x40);
    } else putchar(*buf);
    buf++;
  } while (--len);
}

void server(char const* id, char** args, bool daemon, bool verbose, bool quiet) {
  int r;
  enum use_socket use = identify_use(id);
  union any_addr addr;
  try(r, fill_addr(use, &addr, id));

  if (daemon) {
    pid_t dpid;
    try(dpid, fork());
    if (0 < dpid) _exit(0);
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  cpid = fork_program(args);

  if (daemon) {
    try(r, setsid());
    try(r, chdir("/"));
    umask(0);
  }

  sig_handle(SIGINT, cleanup, SA_RESETHAND);
  sig_handle(SIGTERM, cleanup, SA_RESETHAND);

  if (USE_LOCAL == use) filename = id;
  try(fds[IDX_SOCK].fd, bind_sock(use, &addr, IDX_COUNT-IDX_CLIS));

  fds[IDX_TERM].events = POLLIN;
  fds[IDX_SOCK].events = POLLIN;
  fds_count = IDX_CLIS;

  if (!quiet) puts("server: listening");

  char buf[BUF_SIZE];
  int len;
  while (1) {
    int n;
    try(n, poll(fds, fds_count, -1));

    // notification from a client
    for (int i = IDX_CLIS; i < fds_count; i++) {
      bool remove = POLLHUP & fds[i].revents;

      // client got input
      if (!remove && POLLIN & fds[i].revents) {
        try(len, read(fds[i].fd, buf, BUF_SIZE));
        if (!quiet) {
          printf("<%d> (%dB) ", fds[i].fd, len);
          if (verbose) putesc(buf, len);
          putchar('\n');
        }
        remove = 0 == len;

        // input for program
        if (0 != len) {
          char const* found = buf-1;
          int rest = len;
          while (0 < rest && NULL != (found = memchr(found+1, *ESC, rest))) {
            rest-= found-buf;
            if (0 == memcmp(CUSTOM_TERM_TERM, found+1, strlen(CUSTOM_TERM_TERM))) {
              if (!quiet) puts("server: received terminate");
              terminate = true;
              goto finally;
            } else if (0 == memcmp(CUSTOM_TERM_WINSIZE, found+1, strlen(CUSTOM_TERM_WINSIZE))) {
              // client indicating winsize change
              int w = 0, h = 0;
              char const* const start = found;
              if (!quiet) puts("server: received winsize change");

              // ^[[={w};{h}w
              found+= strlen(CUSTOM_TERM_WINSIZE) + 1;
              rest-= strlen(CUSTOM_TERM_WINSIZE) + 1;

              // {w};{h}w
              while ('0' <= *found && *found <= '9' && 0 < rest--)
                w = 10*w + (*found++ - '0');

              // ;{h}w
              if (0 == w || 0 == rest || ';' != *found) continue;
              found++;
              rest--;

              // {h}w
              while ('0' <= *found && *found <= '9' && 0 < rest--)
                h = 10*h + (*found++ - '0');

              // w
              if (0 == h || 0 == rest || 'w' != *found) continue;
              found++;
              rest--;

              wss[i-IDX_CLIS].ws_col = w;
              wss[i-IDX_CLIS].ws_row = h;
              try(r, update_winsize(quiet));

              // splice the sequence out of buf
              len-= found-start;
              memmove((void*)start, found, rest);
              found = start;
            } // if CUSTOM_TERM_WINSIZE
          } // while scan for ESC

          // ultimately send the buffer (client -> program)
          try(r, write(fds[IDX_TERM].fd, buf, len));
        } // if 0 != len
      } // if client got input

      // client was closed
      if (remove) {
        if (!quiet) printf("server: -%d\n", fds[i].fd);
        close(fds[i].fd);

        fds_count--;
        for (int j = i; j < fds_count; j++) {
          fds[j] = fds[j+1];
          wss[j-IDX_CLIS] = wss[j-IDX_CLIS+1];
        }

        try(r, update_winsize(quiet));
      } // if remove
    } // for (clients)

    // progam is done
    if (POLLHUP & fds[IDX_TERM].revents) {
      if (!quiet) puts("server: program done");
      break;
    }

    // program output (only if there are clients listening)
    if (POLLIN & fds[IDX_TERM].revents) {
      try(len, read(fds[IDX_TERM].fd, buf, BUF_SIZE));
      if (!quiet) {
        printf("<prog> (%dB) ", len);
        if (verbose) putesc(buf, len);
        putchar('\n');
      }

      // progam is done (eof)
      if (0 == len) {
        if (!quiet) puts("server: program done (eof)");
        break;
      }

      // echo back to every clients
      for (int j = IDX_CLIS; j < fds_count; j++)
        try(r, write(fds[j].fd, buf, len));

      // scan for enter/leave alt
      char const* found = buf-1;
      int rest = len;
      while (0 < rest && NULL != (found = memchr(found+1, *ESC, rest))) {
        rest-= found-buf;
        if (!is_alt && 0 == memcmp(TERM_SMCUP, found+1, strlen(TERM_SMCUP))) {
          if (!quiet) puts("server: entering alt");
          is_alt = true;
        } else if (is_alt && 0 == memcmp(TERM_RMCUP, found+1, strlen(TERM_RMCUP))) {
          if (!quiet) puts("server: leaving alt");
          is_alt = false;
        }
      } // while scan for ESC
    } // if program output

    // new incoming connection
    if (POLLIN & fds[IDX_SOCK].revents) {
      int cli;
      try(cli, accept(fds[IDX_SOCK].fd, NULL, NULL));
      if (!quiet) printf("server: +%d\n", cli);

      fds[fds_count].fd = cli;
      fds[fds_count].events = POLLIN;

      // init winsize for this client
      wss[fds_count-IDX_CLIS] = curr_ws;

      // enter alt screen if needed (and other term states that i dont know of)
      if (is_alt) try(r, write(cli, ESC TERM_SMCUP, strlen(ESC TERM_SMCUP)));

      fds_count++;
    } // if new connection
  } // while (poll)

finally:
  if (errdid) terminate = true;
  cleanup(0);
  if (errdid) _die();
}
