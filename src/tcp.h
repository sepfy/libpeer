#ifndef TCP_SOCKET_H_
#define TCP_SOCKET_H_

#include "address.h"

typedef struct TcpSocket TcpSocket;

struct TcpSocket {

  int fd;
  Address bind_addr;
  long long int timeout_sec;
  long int timeout_usec;
};

int tcp_socket_open(TcpSocket *tcp_socket);

int tcp_socket_connect(TcpSocket *tcp_socket, Address *addr);

void tcp_socket_close(TcpSocket *tcp_socket);

int tcp_socket_send(TcpSocket *tcp_socket, const uint8_t *buf, int len);

int tcp_socket_recv(TcpSocket *tcp_socket, uint8_t *buf, int len);

void tcp_blocking_timeout(TcpSocket *tcp_socket, long long int ms);

#endif // TCP_SOCKET_H_
