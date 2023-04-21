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
  // YYY: reset the terminal, this should be enough most of the time
  // NOTE: could track at least if it is in alt screen, ie. sometime we don't want to reset
  write(STDOUT_FILENO, "\033c", 2);
  if (sign) _exit(0);
}

static char leader[8];

/// convert ^x sequences (for now thats pretty much it, ^ itself cannot be used)
int parse_key(char const* ser, char* de) {
  int r = 1;
  do { *de++ = '^' == *ser ? CTRL(*++ser) : *ser; }
  while (*++ser && ++r < 8);
  return r;
}

void client(char const* name, char const* leader_key) {
  int leader_len = parse_key(leader_key ? leader_key : "^[^[", leader);
  bool leader_found = false;

  // TODO(winsize): capture window size change signal, update server on it

  setup_cleanup();
  
  // sleep(3); // ZZZ
  sock = conx_local_socket(name);

  // set terminal into raw mode
  struct termios tio;
  try(r, tcgetattr(STDERR_FILENO, &tio));

  // TODO(term/opts): option to have it not raw (plays better with buffering and thus
  //                  could be preferable in some situations) ((will still have -echo))
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

  char buf[BUF_SIZE];
  int len;
  while (1) {
    int n;
    try(n, poll(fds, 2, -1));

    // server stopped (eg. program terminated)
    if (POLLHUP & fds[IDX_SOCK].revents) break;

    // user input
    if (POLLIN & fds[IDX_USER].revents) {
      try(len, read(STDIN_FILENO, buf, BUF_SIZE));
      if (0 == len) break;

      // handle leader key
      if (leader_found) {
        switch (buf[0]) {
          case CTRL('C'): // ^C also kill server?
            try(r, raise(SIGINT));

          case CTRL('D'):
            goto finally;

          // case CTRL('Z'):
          //   // something to prevent socket over buffering?
          //   // TODO(winsize): have server update winsize
          //   // (both of these can be solved by closing the socket and re-opening it later...)
          //   try(r, raise(SIGTSTP));
          //   break;

          default:
            // not for me, forward all (re-prefix leader)
            memmove(buf+leader_len, buf, len);
            memcpy(buf, leader, leader_len);
            leader_found = false;
        }
      } else {
        // YYY: in raw mode, `buf` will contain exactly the sequence, so this does it for now
        leader_found = is_raw && 0 == memcmp(leader, buf, leader_len);

        // TODO(term): when not raw (if ever)
        // try(r, write(sock, buf, .. up to leader));
        // try(r, write(sock, buf, from after key past leader ..));
      }

      if (!leader_found) try(r, write(sock, buf, len));
    }

    // program output
    if (POLLIN & fds[IDX_SOCK].revents) {
      try(len, read(sock, buf, BUF_SIZE));
      if (0 == len) break;
      try(r, write(STDOUT_FILENO, buf, len));
    }
  } // while (poll)

finally:
  cleanup(0);
  if (errdid) _die();
}
