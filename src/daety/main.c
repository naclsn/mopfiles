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

int main(int argc, char** argv) {
  char const* prog = *argv++;
  if (0 == --argc || 0 == strcmp("--help", *argv) || 0 == strcmp("-h", *argv)) {
    printf("Usage: %s [--client|--server] <prog> <args...>\n", prog);
    exit(EXIT_FAILURE);
  }

  bool is_client = 0 == strcmp("--client", *argv);
  bool is_server = 0 == strcmp("--server", *argv);

  if (is_client || is_server) argv++;

  // TODO(opts): from prog+args or option, also enable non-local sockets
  const char* name = "me";

  if (is_client) {
    client(name, NULL); // TODO(opts): client leader key
  } else if (is_server) {
    server(name, argv);
  } else {
    puts("TODO: fork and stuff");
    return 95;
  }
}
