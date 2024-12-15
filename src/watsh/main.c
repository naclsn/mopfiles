#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

char const* errfile = NULL;
unsigned int errline = 0;
char const* errmsg = NULL;
int errdid = 0;
void _die() {
  char errbuf[4096];
  sprintf(errbuf, "[%s:%d] %s", errfile, errline, errmsg);
  errno = errdid;
  perror(errbuf);
  exit(errno);
}

#include <sys/inotify.h>
#include <unistd.h>

void usage(char const* self, char const* may_errmsg) {
  if (may_errmsg) printf("Error: %s\n  see %s --help for\n", may_errmsg, self);
  else printf(
    #ifndef VERS
    "Usage: %s <...>\n"
    #else
    #include "usage.inc"
    #endif // VERS
    , self
  );
}

int main(int argc, char const** argv) {
  char const* self = *argv++;
  #define argis(_x) (0 == strcmp(_x, *argv))

  if (0 == --argc || argis("--help") || argis("-h")) {
    usage(self, NULL);
    return EXIT_FAILURE;
  }

  if (argis("--version")) {
    #define xtocstr(x) tocstr(x)
    #define tocstr(x) #x
    #ifndef VERS
    #warning "no version defined (-DVERS, use the build script)"
    #define VERS (unversioned)
    #endif // VERS
    puts(xtocstr(VERS));
    #undef xtocstr
    #undef tocstr
    return EXIT_SUCCESS;
  }

  //bool is_full = false;

  // parse options
  while (argc && *argv) {
    if ('-' == (*argv)[0]) {
      if ('-' == (*argv)[1]) {
        if ('\0' == (*argv)[2]) {
          argc--;
          argv++;
          break;
        }
        //else if (argis("--full")) is_full = true;
        else {
          printf("Unknown option: '%s'\n", *argv);
          return EXIT_FAILURE;
        }
      } // long opts

      else {
        for (char const* c = *argv+1; *c; c++) {
          switch (*c) {
            //case 'f': is_full = true; break;
            default:
              printf("Unknown option: '-%c'\n", *c);
              return EXIT_FAILURE;
          }
        }
      } // short opts
    } else break; // not '-'

    argc--;
    argv++;
  } // while arg

  int fd, wds[argc]; // FIXME: will need to realloc on IN_CREATE anyway so better off with malloc...
  try(fd, inotify_init());
  for (int k = 0; k < argc; k++)
    try(wds[k], inotify_add_watch(fd, argv[k],
        IN_CREATE |
        IN_DELETE |
        IN_MODIFY |
        IN_MOVE));

  union { struct inotify_event const _align; char b[4096]; } buf;
  struct inotify_event const* ev;
  while (true) {
    ssize_t len;
    try(len, read(fd, buf.b, sizeof buf.b));

    bool first = true;
    for (char* ptr = buf.b; ptr < buf.b+len; ptr+= sizeof(struct inotify_event) + ev->len) {
      ev = (struct inotify_event*)ptr;

      for (int k = 0; k < argc; k++) if (ev->wd == wds[k]) {
        printf(first ? "%s" : ":%s", argv[k]);
        first = false;
        break;
      }
      if (ev->len) printf(":%s", ev->name);
    }
    printf("\n");
    fflush(stdout);
  }

  // unreachable
  close(fd);
  return EXIT_SUCCESS;

finally:
  if (0 <= fd) {
    close(fd);
  }
  return EXIT_FAILURE;
}
