#include "client.h"

/// connect to a local socket; exits on failure
static int conx_local_socket(char const* filename) {
  int sock = -1;
  struct sockaddr_un addr = {.sun_family= AF_LOCAL};

  try(sock, socket(PF_LOCAL, SOCK_STREAM, 0));

  strncpy(addr.sun_path, filename, sizeof(addr.sun_path));

  int r;
  try(r, connect(sock, (struct sockaddr*)&addr, SUN_LEN(&addr)));

  return sock;
finally:
  close(sock);
  _die();
  return -1;
}

static int sock = -1;
/// cleanup SIGINT handler
static void cleanup(int sign) {
  close(sock);
  if (sign) _exit(0);
}

void client(char const* name) {
  setup_cleanup();
  
  // sleep(3); // ZZZ
  sock = conx_local_socket(name);

#define IDX_USER_FD 0
#define IDX_SOCK_FD 1
  static struct pollfd fds[2] = {
    {.fd= STDIN_FILENO, .events= POLLIN},
    {.fd= 0, .events= POLLIN},
  };
  fds[IDX_SOCK_FD].fd = sock;

  while (1) {
    int n;
    try(n, poll(fds, 2, -1));

    // server stopped (eg. program terminated)
    if (POLLHUP & fds[IDX_SOCK_FD].revents) break;

    char buf[BUF_SIZE];
    int len;

    // user input
    if (POLLIN & fds[IDX_USER_FD].revents) {
      try(len, read(fds[IDX_USER_FD].fd, buf, BUF_SIZE));
      if (0 == len) break;
      try(r, write(fds[IDX_SOCK_FD].fd, buf, len));
    }

    // program output
    if (POLLIN & fds[IDX_SOCK_FD].revents) {
      try(len, read(fds[IDX_SOCK_FD].fd, buf, BUF_SIZE));
      if (0 == len) break;
      try(r, write(fds[IDX_USER_FD].fd, buf, len));
    }
  } // while (poll)

finally:
  cleanup(0);
  if (errdid) _die();
}
