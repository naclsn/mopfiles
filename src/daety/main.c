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

void usage(char const* self) {
  printf(
    #ifndef VERS
    "Usage: %s <...>\n"
    #else
    #include "usage.inc"
    #endif // VERS
    , self
  );
}

void make_id(char** argv, char* dst, int max_len) {
  // NOTE: it uses all of prog and args, maybe just the prog is enough?
  // TODO: proper (eg. do not use special charaters (maybe hash it?))
  char* start = dst;
  for (char** a = argv; *a && dst-start < max_len-1; a++) {
    char const* src = *a;
    while ('\0' != (*dst++ = *src++) && dst-start < max_len-1);
    dst[-1] = ' ';
  }
  dst[-1] = '\0';
}

void put_id(char* id) {
  // TODO: will need to reverse-id here (ie get command from socket filename)
  puts(id+strlen(LOC_ID_PFX));
}

int main(int argc, char** argv) {
  char const* self = *argv++;
  #define argis(_x) 0 == strcmp(_x, *argv)

  if (0 == --argc || argis("--help") || argis("-h")) {
    usage(self);
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
        put_id(dir->d_name);
    }
    closedir(d);
    return EXIT_SUCCESS;
  }

  char const* id = NULL;
  char const* addr = NULL;
  bool is_server = false;
  bool is_verbose = false;
  bool is_quiet = false;
  char const* key = "^\\";
  char** cmd = NULL;
  int cmd_len = 0;
  bool is_cooked = false;
  bool is_kill = false;

  // parse options
  while (argc && *argv) {
    if (NULL != cmd) {
      if (argis("--")) {
        argv++;
        break;
      }
      cmd_len++;
    }

    else if ('-' == (*argv)[0]) {
      if ('-' == (*argv)[1]) {
        if (argis("--")) {
          argv++;
          break;
        }
        else if (argis("--id"))      id = *++argv;
        else if (argis("--addr"))    addr = *++argv;
        else if (argis("--server"))  is_server = true;
        else if (argis("--verbose")) is_verbose = true;
        else if (argis("--quiet"))   is_quiet = true;
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

  // if an id or addr is given, 'not --server' can operate, same with '--kill'
  bool is_client = (NULL != id || NULL != addr) && (!is_server || is_kill);
  if (!is_client && (NULL == *argv || '\0' == (*argv)[0])) {
    usage(self);
    return EXIT_FAILURE;
  }

  char id_buf[1024] = TMP_DIR "/" LOC_ID_PFX;
  if (NULL == addr) {
    if (NULL == id) make_id(argv, id_buf+strlen(id_buf), 1024);
    else strncpy(id_buf+strlen(id_buf), id, 1024-strlen(id_buf));
  } else strcpy(id_buf, addr); // --addr
  id = id_buf;

  if (is_kill) {
    // NULL indicates client to exit right after sending
    char* term_cmd[2] = {ESC CUSTOM_TERM_TERM, NULL};
    if (-1 == client(id, key, term_cmd, 1, true)) {
      puts("Could not join server");
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  if (is_server) {
    server(id, argv, false, is_verbose, is_quiet);
    return EXIT_SUCCESS;
  }

  if (cmd) cmd++; // at this point, `cmd` is still on '--cmd' or '-c'

  if (-1 == client(id, key, cmd, cmd_len, is_cooked)) {
    // server not exists, start it as daemon
    pid_t spid = fork();
    if (spid < 0) return EXIT_FAILURE;

    if (0 == spid) {
      server(id, argv, true, false, true);
      return EXIT_SUCCESS;
    }

    waitpid(spid, NULL, 0);

    sleep(1); // ZZZ: just in case

    if (-1 == client(id, key, cmd, cmd_len, is_cooked)) {
      puts("Could not start server");
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
