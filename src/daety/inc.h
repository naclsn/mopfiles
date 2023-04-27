#ifndef _DATY_INC
#define _DATY_INC

//#define _XOPEN_SOURCE 501

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <pty.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define TMP_DIR "/tmp"
#define LOC_ID_PFX "daety-"

// these should be enough most of the time
#define ESC "\033"
#define TERM_RESET "c"
#define TERM_SMCUP "[?1049h"
#define TERM_RMCUP "[?1049l"
// fake CSI seq used by client to indicate SIGWINCH to server:
// "^[[={w};{h}w"; width and height are up to 3 digits
#define CUSTOM_TERM_WINSIZE "[="
// fake CSI seq to terminate program  pqrstuvwxyz
#define CUSTOM_TERM_TERM "[=/*-*/~"

extern char const* errfile;
extern unsigned int errline;
extern char const* errmsg;
extern int errdid;
extern void _die();

#define die(__msg) do {  \
  errfile = __FILE__;    \
  errline = __LINE__;    \
  errmsg = __msg;        \
  errdid = errno;        \
  _die();                \
} while (1)
#define try(_v, _r) do {  \
  _v = _r;                \
  if (_v < 0) {           \
    errfile = __FILE__;   \
    errline = __LINE__;   \
    errmsg = #_r;         \
    errdid = errno;       \
    goto finally;         \
  }                       \
} while (0)
#define try_accept(_v, _r, _e) do {  \
  _v = _r;                           \
  if (_v < 0 && _e != errno) {       \
    errfile = __FILE__;              \
    errline = __LINE__;              \
    errmsg = #_r;                    \
    errdid = errno;                  \
    goto finally;                    \
  }                                  \
} while (0)

#define BUF_SIZE 65535

#define sig_handle(_sign, _fn, _flags) do {  \
  struct sigaction sa;                       \
  sa.sa_handler = _fn;                       \
  sigemptyset(&sa.sa_mask);                  \
  sa.sa_flags = _flags;                      \
  int r;                                     \
  try(r, sigaction(_sign, &sa, NULL));       \
} while (0)

enum use_socket {
  USE_LOCAL,
  USE_IPV4,
  //USE_IPV6,
};

/// "%d.%d.%d.%d:%d" -> USE_IPV4
/// default -> USE_LOCAL
static enum use_socket identify_use(char const* id) {
  char const* head;

  // test ipv4
  head = id;
  for (int i = 0; i < 4; i++, head++) {
    for (int j = 0; j <3; j++, head++) {
      if ('.' == *head || ':' == *head) break;
      if ('0' > *head || *head > '9') goto not_ipv4;
    }
    if ((i <3 ? '.' : ':') != *head) goto not_ipv4;
  }
  return USE_IPV4;
not_ipv4:

  // default
  return USE_LOCAL;
}

#endif // _DATY_INC
