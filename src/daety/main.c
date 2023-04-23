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
    "Usage: %s [opts] <prog> [<args...>]\n"
    "\n"
    "--help    -h         display this help\n"
    "--version            show build the version\n"
    "\n"
    "shared client/server:\n"
    "--id      -i <id>    the default is from prog args\n"
    "(TODO: socket options)\n"
    "\n"
    "server only:\n"
    "--server  -s         starts only the server\n"
    "--verbose            (only when -s)\n"
    "\n"
    "client only:\n"
    "--key     -k <key>   use the following leader key\n"
    "--cmd     -c <keys..> --\n"
    "                     send the sequence upon connection\n"
    "                     joined with not separator\n"
    "                     must be terminated with --\n"
    "--cooked             do not set raw mode\n"
    , self
  );
}

int main(int argc, char** argv) {
  char const* self = *argv++;
  #define argis(_x) 0 == strcmp(_x, *argv)
  if (0 == --argc || argis("--help") || argis("-h")) {
    usage(self);
    return EXIT_FAILURE;
  } if (argis("--version")) {
    #define xtocstr(x) tocstr(x)
    #define tocstr(x) #x
    #ifndef VERS
    #warning "no version defined (-DVERS, use the build script)"
    #define VERS (unversioned)
    #endif
    puts(xtocstr(VERS));
    #undef xtocstr
    #undef tocstr
    return EXIT_SUCCESS;
  }

  char const* id = NULL;
  bool is_server = false;
  bool is_verbose = false;
  char const* key = "^\\";
  char** cmd = NULL;
  int cmd_len = 0;
  bool is_cooked = false;

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
        else if (argis("--server"))  is_server = true;
        else if (argis("--verbose")) is_verbose = true;
        else if (argis("--key"))     key = *++argv;
        else if (argis("--cmd"))     cmd = argv;
        else if (argis("--cooked"))  is_cooked = true;
        else {
          printf("Unknown option: '%s'\n", *argv);
          return EXIT_FAILURE;
        }
      } // long opts

      else {
        for (char const* c = *argv+1; *c; c++) {
          switch (*c) {
            case 'i': id = *++argv;     break;
            case 's': is_server = true; break;
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

  if (NULL == *argv || '\0' == (*argv)[0]) {
    usage(self);
    return EXIT_FAILURE;
  }

  char id_buf[1024] = "/tmp/daety-";
  if (NULL == id) {
    // NOTE: it uses all of prog and args, maybe just the prog is enough?
    char* dst = id_buf+strlen(id_buf);
    for (char** a = argv; *a && dst-id_buf < 1023; a++) {
      char const* src = *a;
      while ('\0' != (*dst++ = *src++) && dst-id_buf < 1023);
      dst[-1] = ' ';
    }
    dst[-1] = '\0';
  } else strncpy(id_buf+strlen(id_buf), id, 1024-strlen(id_buf));
  id = id_buf;

  if (is_server) {
    server(id, argv, is_verbose);
    return EXIT_SUCCESS;
  }

  // TODO: if server not exists fork and stuff

  if (cmd) cmd++; // at this point, `cmd` is still on '--cmd' or '-c'

  client(id, key, cmd, cmd_len, is_cooked);
  return EXIT_SUCCESS;
}
