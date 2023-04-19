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
  if (1 == argc) {
    printf("Usage: %s <prog> <args...>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  const char* name = "me";

  if (0 == strcmp("client", argv[1])) {
    puts("starting client");
    client(name);
  } else if (0 == strcmp("server", argv[1])) {
    puts("starting server");
    server(name);
  }

  puts("done");
}
