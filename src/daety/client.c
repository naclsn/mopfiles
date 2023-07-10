#include "client.h"

static int sock = -1;
static bool is_raw = false;
static bool not_tty = false;
static struct termios prev_tio;
static bool is_alt = false;

/// cleanup SIGINT handler
static void cleanup(int sign) {
  // terminate program if called as signal handler
  if (sign) write(sock, ESC CUSTOM_TERM_TERM, strlen(ESC CUSTOM_TERM_TERM));
  close(sock);

  if (!not_tty) tcsetattr(STDERR_FILENO, TCSANOW, &prev_tio);
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
static int init(enum use_socket use, union any_addr const* addr, bool skip_raw) {
  int r;
  sock = conx_sock(use, addr);
  if (-1 == sock) return -1;

  if (!skip_raw) {
    // set terminal into raw mode
    try(r, tcgetattr(STDERR_FILENO, &prev_tio));
    struct termios raw_tio = prev_tio;
    cfmakeraw(&raw_tio);
    try(r, tcsetattr(STDERR_FILENO, TCSANOW, &raw_tio));
    is_raw = true;
  } else if (!not_tty) {
    // still tries to disable echo
    r = tcgetattr(STDERR_FILENO, &prev_tio);
    if (-1 != r) {
      struct termios noecho_tio = prev_tio;
      noecho_tio.c_lflag&= ~(ECHO | ECHONL);
      try(r, tcsetattr(STDERR_FILENO, TCSANOW, &noecho_tio));
    } else not_tty = true;
  }

  // send size to server
  winch(0);

  return 0;
finally:
  _die();
  return 0;
}

static char leader[8];

/// convert ^x sequences (for now thats pretty much it, ^ itself cannot be used)
static int parse_key(char const* ser, char* de, int max_len) {
  int r = 1;
  do { *de++ = '^' == *ser ? CTRL(*++ser) : *ser; }
  while (*++ser && ++r < max_len);
  return r;
}

int client(char const* id, char const* leader_key, char const** send_sequence, int sequence_len, bool skip_raw) {
  int r;
  enum use_socket use = identify_use(id);
  union any_addr addr;
  try(r, fill_addr(use, &addr, id));

  int leader_len = parse_key(leader_key, leader, 8);
  bool leader_found = false;

  r = init(use, &addr, skip_raw);
  if (-1 == r) return -1;

  sig_handle(SIGINT, cleanup, SA_RESETHAND);
  sig_handle(SIGWINCH, winch, 0);

  if (send_sequence) {
    char buf[BUF_SIZE];
    for (; 0 < sequence_len; sequence_len--, send_sequence++) {
      // NULL element indicates end of programmatic input (used by eg. '--kill')
      if (NULL == *send_sequence) goto finally;
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

      if (!skip_raw) {
        // handle leader key
        if (leader_found) {
          switch (buf[0]) {
            case CTRL('C'):
              try(r, raise(SIGINT));

            case CTRL('D'):
              goto finally;

            case CTRL('Z'):
              cleanup(0);
              try(r, raise(SIGSTOP));
              try(r, init(use, &addr, skip_raw));
              leader_found = false;
              continue;

            default:
              // not for me, forward all (re-prefix leader)
              memmove(buf+leader_len, buf, len);
              memcpy(buf, leader, leader_len);
              leader_found = false;
          }
        } else leader_found = is_raw && 0 == memcmp(leader, buf, leader_len);
      } // if raw mode

      if (!leader_found) try(r, write(sock, buf, len));
    } // user input

    if (POLLHUP & fds[IDX_USER].revents) {
      len = 0;
      break;
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
  return 0;
}
