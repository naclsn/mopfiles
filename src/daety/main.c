#include "inc.h"
#include "client.h"
#include "server.h"

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

/// similar to php's `urlencode`, that is only 'A-Za-z_.-' are kept as is and '+' is a space
void make_id(char const** argv, char* dst, int max_len) {
  char const* const start = dst;
  for (char const** a = argv; *a && dst-start < max_len-1; a++) {
    char const* src = *a;
    while ('\0' != *src && dst-start < max_len-1) {
      char c = *src++;
      if ('-' == c || '.' == c || '_' == c
        || ('0' <= c && c <= '9')
        || ('A' <= c && c <= 'Z')
        || ('a' <= c && c <= 'z')
      ) {
        *dst++ = c;
      } else if (' ' == c) {
        *dst++ = '+';
      } else {
        *dst++ = '%';
        if (dst-start < max_len-1) {
          char h = (c>>4) & 0xf, l = c & 0xf;
          *dst++ = h<10 ? h+'0' : h+'A'-10;
          *dst++ = l<10 ? l+'0' : l+'A'-10;
        }
      }
    }
    *dst++ = '+';
  }
  dst[-1] = '\0';
}

/// decode from `make_id` and output it right away, with a trailing '\n'
void puts_id(char const* id) {
  id+= strlen(LOC_ID_PFX);
  while ('\0' != *id) {
    char c = *id++;
    if ('%' == c) {
      char dh = *id++;
      char dl = *id++;
      char h = dh<'A' ? dh-'0' : dh-'A'+10, l = dl<'A' ? dl-'0' : dl-'A'+10;
      putchar((h<<4) | l);
    } else putchar('+' == c ? ' ' : c);
  }
  putchar('\n');
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

  if (argis("--list") || argis("-l")) {
    DIR* d = opendir(TMP_DIR);
    if (!d) return EXIT_FAILURE;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
      if (0 == memcmp(LOC_ID_PFX, dir->d_name, strlen(LOC_ID_PFX)))
        puts_id(dir->d_name);
    }
    closedir(d);
    return EXIT_SUCCESS;
  }

  char const* id = NULL;
  char const* addr = NULL;
  bool is_server = false;
  char const* cwd = NULL;
  bool is_verbose = false;
  bool is_quiet = false;
  bool is_track = false;
  char const* key = "^\\";
  char const** cmd = NULL;
  int cmd_len = 0;
  bool is_cooked = false;
  bool is_kill = false;

  // parse options
  while (argc && *argv) {
    if (NULL != cmd) {
      if (argis("--")) {
        argc--;
        argv++;
        break;
      }
      cmd_len++;
      // indicates that client should stop right after sending the sequence
      if ('\0' == (*argv)[0]) {
        argc--;
        argv++;
        if (!argis("--")) {
          usage(self, "expected a `--` after an empty `--cmd` element");
          return EXIT_FAILURE;
        }
        break;
      }
    }

    else if ('-' == (*argv)[0]) {
      if ('-' == (*argv)[1]) {
        if ('\0' == (*argv)[2]) {
          argc--;
          argv++;
          break;
        }
        else if (argis("--id"))      id = *++argv;
        else if (argis("--addr"))    addr = *++argv;
        else if (argis("--server"))  is_server = true;
        else if (argis("--cwd"))     cwd = *++argv;
        else if (argis("--verbose")) is_verbose = true;
        else if (argis("--quiet"))   is_quiet = true;
        else if (argis("--track"))   is_track = true;
        else if (argis("--key"))     key = *++argv;
        else if (argis("--cmd"))     cmd = argv;
        else if (argis("--cooked"))  is_cooked = true;
        else if (argis("--kill"))    is_kill = true;
        else {
          printf("Unknown option: '%s'\n", *argv);
          return EXIT_FAILURE;
        }
      } // long opts

      else {
        for (char const* c = *argv+1; *c; c++) {
          switch (*c) {
            case 'i': id = *++argv;     break;
            case 'a': addr = *++argv;   break;
            case 's': is_server = true; break;
            case 'C': cwd = *++argv;    break;
            case 'q': is_quiet = true;  break;
            case 'k': key = *++argv;    break;
            case 'c': cmd = argv;       break;
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

  // if an id or addr is given, 'not --server' can operate;
  // in other words, if not a client, it needs program args
  bool is_client = (NULL != id || NULL != addr) && !is_server;
  if (!is_client && (NULL == *argv || '\0' == (*argv)[0])) {
    usage(self, "program argument needed to start a server");
    return EXIT_FAILURE;
  }

  // id built form --addr or --id or argv, whichever is there
  char id_buf[1024] = TMP_DIR "/" LOC_ID_PFX;
  int id_off = strlen(TMP_DIR "/" LOC_ID_PFX);
  if (NULL != addr) {
    char const* argv[2] = {addr, NULL};
    make_id(argv, id_buf+id_off, 1024-id_off);
  } else if (NULL != id) {
    char const* argv[2] = {id, NULL};
    make_id(argv, id_buf+id_off, 1024-id_off);
  } else make_id(argv, id_buf+id_off, 1024-id_off);
  id = id_buf;

  if (is_kill) {
    // empty element indicates client to exit right after sending
    char const* term_cmd[2] = {ESC CUSTOM_TERM_TERM, ""};
    if (-1 == client(id, key, term_cmd, 2, true)) {
      puts("Could not join server");
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  if (is_server) {
    server(id, argv, cwd, false, is_verbose, is_quiet, is_track);
    return EXIT_SUCCESS;
  }

  if (cmd) cmd++; // at this point, `cmd` is still on '--cmd' or '-c'

  if (-1 == client(id, key, cmd, cmd_len, is_cooked)) {
    // server not exists, start it as daemon
    pid_t spid = fork();
    if (spid < 0) return EXIT_FAILURE;

    if (0 == spid) {
      server(id, argv, cwd, true, false, true, is_track);
      return EXIT_SUCCESS;
    }

    waitpid(spid, NULL, 0);

    // TODO: get rid of it for a proper solution (eg. semaphore)
    sleep(1); // ZZZ: just in case

    if (-1 == client(id, key, cmd, cmd_len, is_cooked)) {
      puts("Could not start server");
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
