#include "socket.h"

/// "%d.%d.%d.%d:%d" -> USE_IPV4
/// default -> USE_LOCAL
enum use_socket identify_use(char const* id) {
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

int fill_addr_local(union any_addr* addr, char const* filename) {
  addr->un.sun_family = AF_LOCAL;
  strncpy(addr->un.sun_path, filename, sizeof addr->un.sun_path);
  return 0;
}
int fill_addr_ipv4(union any_addr* addr, char const* ipv4) {
  char* port = strchr(ipv4, ':');
  if (NULL == port) return -1;
  addr->in.sin_family= AF_INET;
  addr->in.sin_port= htons(atoi(port+1));
  *port = '\0';
  if (0 == inet_aton(ipv4, &addr->in.sin_addr)) return -1;
  *port = ':';
  return 0;
}
int fill_addr(enum use_socket use, union any_addr* addr, void const* data) {
  switch (use) {
    case USE_LOCAL: return fill_addr_local(addr, data);
    case USE_IPV4: return fill_addr_ipv4(addr, data);
  }
  return -1;
}

int bind_sock_local(union any_addr const* addr, int listen_n) {
  int sock, r;
  if ((sock = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) return sock;
  if ( (r = bind(sock, &addr->any, SUN_LEN(&addr->un))) < 0
    || (r = listen(sock, listen_n)) < 0 ) {
    close(sock);
    return r;
  }
  return sock;
}
int bind_sock_ipv4(union any_addr const* addr, int listen_n) {
  int sock, r;
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) return sock;
  if ( (r = bind(sock, &addr->any, sizeof addr->in)) < 0
    || (r = listen(sock, listen_n)) < 0 ) {
    close(sock);
    return r;
  }
  return sock;
}
int bind_sock(enum use_socket use, union any_addr const* addr, int listen_n) {
  switch (use) {
    case USE_LOCAL: return bind_sock_local(addr, listen_n);
    case USE_IPV4: return bind_sock_ipv4(addr, listen_n);
  }
  return -1;
}

int conx_sock_local(union any_addr const* addr) {
  int sock, r;
  if ((sock = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) return sock;
  if ((r = connect(sock, &addr->any, SUN_LEN(&addr->un))) < 0) {
    close(sock);
    return r;
  }
  return sock;
}
int conx_sock_ipv4(union any_addr const* addr) {
  int sock, r;
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) return sock;
  if ((r = connect(sock, &addr->any, sizeof addr->in)) < 0) {
    close(sock);
    return r;
  }
  return sock;
}
int conx_sock(enum use_socket use, union any_addr const* addr) {
  switch (use) {
    case USE_LOCAL: return conx_sock_local(addr);
    case USE_IPV4: return conx_sock_ipv4(addr);
  }
  return -1;
}
