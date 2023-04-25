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
static bool is_alt = false;

/// cleanup SIGINT handler
static void cleanup(int sign) {
  close(sock);

  if (is_raw) tcsetattr(STDERR_FILENO, TCSANOW, &prev_tio);
  if (is_alt) {
    write(STDOUT_FILENO, ESC TERM_RMCUP, 2);
    write(STDOUT_FILENO, ESC TERM_RESET, 2);
  }

  if (sign) _exit(0);
}

/// update server on winsize changes
static void winch(int sign) {
  (void)sign;
  struct winsize ws;
  if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) < 0) return;

  char buf[16] = ESC CUSTOM_TERM_WINSIZE;
  char* head = buf + strlen(ESC CUSTOM_TERM_WINSIZE);
  // ^[[=

  do {
    *head++ = ws.ws_row%10 + '0';
    ws.ws_row/= 10;
  } while (0 != ws.ws_row);
  // ^[[={h}

  *head++ = ';';
  // ^[[={h};

  do {
    *head++ = ws.ws_col%10 + '0';
    ws.ws_col/= 10;
  } while (0 != ws.ws_col);
  // ^[[={h};{w}

  // reverse
  char* base = buf + strlen(ESC CUSTOM_TERM_WINSIZE);
  for (int k = 0; k < (head-base)/2; k++) {
    char tmp = base[k];
    base[k] = head[-k-1];
    head[-k-1] = tmp;
  }
  // ^[[={w};{h}

  *head++ = 'w';
  // ^[[={w};{h}w

  write(sock, buf, head-buf);
}

/// connect, set term raw and winsize to server
static void init(char const* name, bool skip_raw) {
  int r;
  sock = conx_local_socket(name);

  // set terminal into raw mode
  struct termios tio;
  try(r, tcgetattr(STDERR_FILENO, &tio));

  if (!skip_raw) {
    prev_tio = tio;
    cfmakeraw(&tio);
    try(r, tcsetattr(STDERR_FILENO, TCSANOW, &tio));
    is_raw = true;
  }

  // send size to server
  winch(0);

  return;
finally:
  _die();
}

static char leader[8];

/// convert ^x sequences (for now thats pretty much it, ^ itself cannot be used)
static int parse_key(char const* ser, char* de, int max_len) {
  int r = 1;
  do { *de++ = '^' == *ser ? CTRL(*++ser) : *ser; }
  while (*++ser && ++r < max_len);
  return r;
}

void client(char const* name, char const* leader_key, char** send_sequence, int sequence_len, bool skip_raw) {
  int r;
  int leader_len = parse_key(leader_key, leader, 8);
  bool leader_found = false;

  sig_handle(SIGINT, cleanup, SA_RESETHAND);
  sig_handle(SIGWINCH, winch, 0);

  init(name, skip_raw);

  if (send_sequence) {
    char buf[BUF_SIZE];
    for (; 0 < sequence_len; sequence_len--, send_sequence++) {
      int len = parse_key(*send_sequence, buf, BUF_SIZE);
      try(r, write(sock, buf, len));
    }
  }

#define IDX_USER 0
#define IDX_SOCK 1
  struct pollfd fds[2] = {
    {.fd= STDIN_FILENO, .events= POLLIN},
    {.fd= 0, .events= POLLIN},
  };
  fds[IDX_SOCK].fd = sock;

  char buf[BUF_SIZE];
  int len;
  while (1) {
    int n;
    try_accept(n, poll(fds, 2, -1), EINTR); // allow being interrupted by SIGWINCH

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

          case CTRL('Z'):
            cleanup(0); // FIXME: does not make it..
            try(r, raise(SIGSTOP));
            init(name, skip_raw);
            leader_found = false;
            continue;

          default:
            // not for me, forward all (re-prefix leader)
            memmove(buf+leader_len, buf, len);
            memcpy(buf, leader, leader_len);
            leader_found = false;
        }
      } else {
        // in raw mode, `buf` will contain exactly the sequence, so this does it for now
        leader_found = is_raw && 0 == memcmp(leader, buf, leader_len);

        // TODO(term): when not raw (if ever)
        // try(r, write(sock, buf, .. up to leader));
        // try(r, write(sock, buf, from after key past leader ..));
      }

      if (!leader_found) try(r, write(sock, buf, len));
    }

    len = 0;
    bool end = POLLHUP & fds[IDX_SOCK].revents;
    // program output
    if (POLLIN & fds[IDX_SOCK].revents) {
      try(len, read(sock, buf, BUF_SIZE));
      if (0 == len) break;

      // scan for enter/leave alt
      char const* found = buf-1;
      int rest = len;
      while (0 < rest && NULL != (found = memchr(found+1, *ESC, rest))) {
        rest-= found-buf;
        if (!is_alt && 0 == memcmp(TERM_SMCUP, found+1, strlen(TERM_SMCUP)))
          is_alt = true;
        else if (is_alt && 0 == memcmp(TERM_RMCUP, found+1, strlen(TERM_RMCUP)))
          is_alt = false;
      }

      if (end) len--; // because in that case the last bytes is the exit code
      try(r, write(STDOUT_FILENO, buf, len));
      if (end) len++;
    }

    // server stopped (eg. program terminated)
    if (end) break;
  } // while (poll)

finally:
  cleanup(0);
  if (errdid) _die();

  // program exit code should be the last byte sent if any
  exit(len ? buf[len-1] : 0);
}
