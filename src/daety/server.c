#include "server.h"

/// create and bind a local socket; exits on failure
static int bind_local_socket(char const* filename, int listen_n) {
  int sock = -1;
  struct sockaddr_un addr = {.sun_family= AF_LOCAL};

  try(sock, socket(PF_LOCAL, SOCK_STREAM, 0));

  strncpy(addr.sun_path, filename, sizeof(addr.sun_path));

  int r;
  try(r, bind(sock, (struct sockaddr*)&addr, SUN_LEN(&addr)));
  try(r, listen(sock, listen_n));

  return sock;
finally:
  close(sock);
  _die();
  return -1;
}

#define IDX_SOCK_FD 0
#define IDX_TERM_FD 1
#define IDX_CLI_FDS 2

static char const* crap_name; // ZZZ: crap
static struct pollfd fds[8];
static int fds_count = 0;
/// cleanup SIGINT handler
static void cleanup(int sign) {
  for (int k = 0; k < fds_count; k++)
    close(fds[k].fd);
  unlink(crap_name);
  if (sign) _exit(0);
}

void server(char const* name) {
  pid_t cpid;
  try(cpid, forkpty(&fds[IDX_TERM_FD].fd, NULL, NULL, NULL)); // TODO: tio and ws
  if (0 == cpid) {
    // child (program)
    char buf[BUF_SIZE];
    int len;
    while (1) {
      try(len, read(STDIN_FILENO, buf, BUF_SIZE));
      if (0 == len--) break;
      for (int k = 0; k < len/2; k++) {
        char tmp = buf[k];
        buf[k] = buf[len-1-k];
        buf[len-1-k] = tmp;
      }
      try(len, write(STDIN_FILENO, buf, len+1));
    }
    _exit(0);
    return;
  }
  // parent (server)

  setup_cleanup();

  fds[IDX_TERM_FD].events = POLLIN;

  crap_name = name;
  fds[IDX_SOCK_FD].fd = bind_local_socket(name, 6);
  fds[IDX_SOCK_FD].events = POLLIN;
  fds_count = IDX_CLI_FDS;

  char buf[BUF_SIZE];
  int len;
  while (1) {
    int n;
    try(n, poll(fds, fds_count, -1));

    // notification from a client
    for (int i = IDX_CLI_FDS; i < fds_count; i++) {
      bool remove = POLLHUP & fds[i].revents;

      // client got input
      if (!remove && POLLIN & fds[i].revents) {
        try(len, read(fds[i].fd, buf, BUF_SIZE));
        printf("<%d> '%.*s'\n", fds[i].fd, len, buf);
        remove = 0 == len;

        // program input
        if (0 != len) {
          try(r, write(fds[IDX_TERM_FD].fd, buf, len));

          // echo back to every other clients
          for (int j = IDX_CLI_FDS; j < fds_count; j++)
            if (i != j) try(r, write(fds[j].fd, buf, len));
        }
      }

      // client was closed
      if (remove) {
        printf("server: -%d\n", fds[i].fd);
        close(fds[i].fd);
        fds_count--;
        for (int j = i; j < fds_count; j++)
          fds[j] = fds[j+1];
      }
    } // for (clients)

    // progam is done
    if (POLLHUP & fds[IDX_TERM_FD].revents) {
      puts("server: program done");
      try(r, waitpid(cpid, NULL, 0)); // XXX: dupped
      break;
    }

    // program output
    if (POLLIN & fds[IDX_TERM_FD].revents) {
      try(len, read(fds[IDX_TERM_FD].fd, buf, BUF_SIZE));
      printf("<prog> '%.*s'\n", len, buf);

      // progam is done (eof)
      if (0 == len) {
        puts("server: program done (eof)");
        try(r, waitpid(cpid, NULL, 0)); // XXX: dupped
        break;
      }

      // echo back to every clients
      for (int j = IDX_CLI_FDS; j < fds_count; j++)
        try(r, write(fds[j].fd, buf, len));
    }

    // new incoming connection
    if (POLLIN & fds[IDX_SOCK_FD].revents) {
      try(fds[fds_count].fd, accept(fds[IDX_SOCK_FD].fd, NULL, NULL));
      printf("server: +%d\n", fds[fds_count].fd);
      fds[fds_count].events = POLLIN;
      fds_count++;
    }
  } // while (poll)

finally:
  cleanup(0);
  if (errdid) _die();
}
