#ifndef _DATY_SCK
#define _DATY_SCK

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

enum use_socket {
  USE_LOCAL,
  USE_IPV4,
  //USE_IPV6,
};

union any_addr {
  struct sockaddr_un un;
  struct sockaddr_in in;
  struct sockaddr any;
};

enum use_socket identify_use(char const* id);

int fill_addr(enum use_socket use, union any_addr* addr, void const* data);
int bind_sock(enum use_socket use, union any_addr const* addr, int listen_n);
int conx_sock(enum use_socket use, union any_addr const* addr);

#endif // _DATY_SCK
