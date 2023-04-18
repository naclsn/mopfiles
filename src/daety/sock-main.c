#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

static void _die(char const* file, unsigned int line, char const* msg) {
  char errbuf[4096];
  sprintf(errbuf, "[%s:%d] %s", file, line, msg);
  perror(errbuf);
  exit(errno);
}
static inline int _try(int r, char const* file, unsigned int line, char const* msg) {
  if (r < 0) _die(file, line, msg);
  return r;
}

#define die(__msg) _die(__FILE__, __LINE__, __msg)
#define try(_f, ...) _try(_f(__VA_ARGS__), __FILE__, __LINE__, #_f)

/// create and bind a local socket; exits on failure
static int bind_local_socket(char const* filename, int listen_n) {
  int sock = try(socket, PF_LOCAL, SOCK_STREAM, 0);

  struct sockaddr_un addr = {.sun_family= AF_LOCAL};
  strncpy(addr.sun_path, filename, sizeof(addr.sun_path));

  try(bind, sock, (struct sockaddr*)&addr, SUN_LEN(&addr));

  if (listen_n) try(listen, sock, listen_n);

  return sock;
}

/// connect to a local socket; exits on failure
static int conx_local_socket(char const* filename) {
  int sock = try(socket, PF_LOCAL, SOCK_STREAM, 0);

  struct sockaddr_un addr = {.sun_family= AF_LOCAL};
  strncpy(addr.sun_path, filename, sizeof(addr.sun_path));

  try(connect, sock, (struct sockaddr*)&addr, SUN_LEN(&addr));

  return sock;
}

int main(int argc, char* argv[]) {
  if (1 == argc) {
    printf("Usage: %s <prog> <args>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char const* name = "./me-socket";

  pid_t cpid = try(fork);
  if (0 < cpid) {
    puts("parent: bind_local_socket");
    int sock = bind_local_socket(name, 10);

    puts("parent: now accepting a connection...");
    int cli = try(accept, sock, NULL, NULL);
    puts("parent: got one");

    char buf[64];
    int len = try(read, cli, buf, 64);
    printf("parent: received '%.*s'\n", len, buf);

    close(sock);
    unlink(name);
    try(waitpid, cpid, NULL, 0);
  }

  else {
    puts("child: (waiting a bit)");
    sleep(7);

    puts("child: conx_local_socket");
    int sock = conx_local_socket(name);

    char buf[64] = "coucou";
    try(write, sock, buf, strlen(buf));

    close(sock);
  }

  return EXIT_SUCCESS;
}
