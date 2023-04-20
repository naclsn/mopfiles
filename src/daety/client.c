#include "client.h"

/// connect to a local socket; exits on failure
static int conx_local_socket(char const* filename) {
  int sock = -1;
  struct sockaddr_un addr = {.sun_family= AF_LOCAL};

  try(sock, socket(PF_LOCAL, SOCK_STREAM, 0));

  strncpy(addr.sun_path, filename, sizeof addr.sun_path);

  int r;
  try(r, connect(sock, (struct sockaddr*)&addr, SUN_LEN(&addr)));

  return sock;
finally:
  close(sock);
  _die();
  return -1;
}

static int sock = -1;
static bool is_raw = false;
static struct termios prev_tio;
/// cleanup SIGINT handler
static void cleanup(int sign) {
  close(sock);
  if (is_raw) tcsetattr(STDERR_FILENO, TCSANOW, &prev_tio);
  // YYY: do i need to restore the winsize too? i had some awkward results...
  if (sign) _exit(0);
}

void client(char const* name) {
  setup_cleanup();
  
  // sleep(3); // ZZZ
  sock = conx_local_socket(name);

  // set terminal into raw mode
  struct termios tio;
  try(r, tcgetattr(STDERR_FILENO, &tio));

  // TODO: option to have it not raw (plays better with buffering
  //       and thus could be preferable in some situations)
  // NOTE: it will still have -echo
  // NOTE: because of raw, ^D/^C/^Z.. are sent to the program, not the client
  //       this could be approached the classic way of "leader key", but i don't like it much..
  prev_tio = tio;
  cfmakeraw(&tio);
  try(r, tcsetattr(STDERR_FILENO, TCSANOW, &tio));
  is_raw = true;

  // send size to server
  struct winsize ws;
  try(r, ioctl(STDERR_FILENO, TIOCGWINSZ, &ws));
  try(r, write(sock, &ws, sizeof ws));

#define IDX_USER 0
#define IDX_SOCK 1
  static struct pollfd fds[2] = {
    {.fd= STDIN_FILENO, .events= POLLIN},
    {.fd= 0, .events= POLLIN},
  };
  fds[IDX_SOCK].fd = sock;

  while (1) {
    int n;
    try(n, poll(fds, 2, -1));

    // server stopped (eg. program terminated)
    if (POLLHUP & fds[IDX_SOCK].revents) break;

    char buf[BUF_SIZE];
    int len;

    // user input
    if (POLLIN & fds[IDX_USER].revents) {
      try(len, read(fds[IDX_USER].fd, buf, BUF_SIZE));
      if (0 == len) break;
      try(r, write(fds[IDX_SOCK].fd, buf, len));
    }

    // program output
    if (POLLIN & fds[IDX_SOCK].revents) {
      try(len, read(fds[IDX_SOCK].fd, buf, BUF_SIZE));
      if (0 == len) break;
      try(r, write(fds[IDX_USER].fd, buf, len));
    }
  } // while (poll)

finally:
  cleanup(0);
  if (errdid) _die();
}
