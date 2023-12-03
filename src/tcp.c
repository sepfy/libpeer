#include "platform/address.h"
#include "platform/socket.h"

#include "utils.h"
#include "tcp.h"

void tcp_blocking_timeout(TcpSocket *tcp_socket, long long int ms) {

  tcp_socket->timeout_sec = ms/1000;
  tcp_socket->timeout_usec = ms%1000*1000;
}

int tcp_socket_open(TcpSocket *tcp_socket) {

  tcp_socket->fd = socket(AF_INET, SOCK_STREAM, 0);

  if (tcp_socket->fd < 0) {

    LOGE("Failed to create socket");
    return -1;
  }

  tcp_blocking_timeout(tcp_socket, 5);
  return 0;
}

int tcp_socket_connect(TcpSocket *tcp_socket, Address *addr) {

  if (tcp_socket->fd < 0) {
    LOGE("Failed to create socket");
    return -1;
  }

  struct sockaddr_in sin;
  socklen_t sin_len = sizeof(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(addr->port);
  memcpy(&sin.sin_addr.s_addr, addr->ipv4, 4);
  LOGI("port: %d", addr->port);
  LOGI("addr: %s", inet_ntoa(sin.sin_addr));
  if (connect(tcp_socket->fd, (struct sockaddr *)&sin, sin_len) < 0) {

    LOGE("Failed to connect to server");
    return -1;
  }
  LOGI("Connected to server");
  return 0;
}

void tcp_socket_close(TcpSocket *tcp_socket) {

  if (tcp_socket->fd > 0) {
    closesocket(tcp_socket->fd);
  }
}

int tcp_get_local_address(TcpSocket *tcp_socket, Address *addr) {

  struct sockaddr_in sin;

  socklen_t len = sizeof(sin);

  if (tcp_socket->fd < 0) {
    LOGE("Failed to create socket");
    return -1;
  }

  if (getsockname(tcp_socket->fd, (struct sockaddr *)&sin, &len) < 0) {
    LOGE("Failed to get local address");
    return -1;
  }

  memcpy(addr->ipv4, &sin.sin_addr.s_addr, 4);

  addr->port = ntohs(sin.sin_port);

  addr->family = AF_INET;

  LOGD("local port: %d", addr->port);
  LOGD("local address: %d.%d.%d.%d", addr->ipv4[0], addr->ipv4[1], addr->ipv4[2], addr->ipv4[3]);

  return 0;
}

int tcp_socket_send(TcpSocket *tcp_socket, const uint8_t *buf, int len) {

  fd_set write_set;
  struct timeval tv;
  int ret = -1;

  if (tcp_socket->fd < 0) {

    LOGE("sendto before socket init");
    return -1;
  }

  FD_ZERO(&write_set);
  FD_SET(tcp_socket->fd, &write_set);

  tv.tv_sec = tcp_socket->timeout_sec;
  tv.tv_usec = tcp_socket->timeout_usec;

  if ((ret = select(tcp_socket->fd + 1, NULL, &write_set, NULL, &tv)) < 0) {

    LOGE("Failed to select: %s", strerror(errno));
    return -1;
  }

  if (FD_ISSET(tcp_socket->fd, &write_set)) {

    ret = send(tcp_socket->fd, buf, len, 0);

    if (ret < 0) {

      LOGE("Failed to send: %s", strerror(errno));
      return -1;
    }
  }

  return ret;
}

int tcp_socket_recv(TcpSocket *tcp_socket, uint8_t *buf, int len) {

  fd_set read_set;
  struct timeval tv;
  int ret;

  if (tcp_socket->fd < 0) {

    LOGE("recvfrom before socket init");
    return -1;
  }

  FD_ZERO(&read_set);
  FD_SET(tcp_socket->fd, &read_set);
  tv.tv_sec = tcp_socket->timeout_sec;
  tv.tv_usec = tcp_socket->timeout_usec;

  if ((ret = select(tcp_socket->fd + 1, &read_set, NULL, NULL, &tv)) < 0) {

    LOGE("Failed to select: %s", strerror(errno));
    return -1;
  }

  if (FD_ISSET(tcp_socket->fd, &read_set)) {

    ret = recv(tcp_socket->fd, buf, len, 0);

    if (ret < 0) {

      LOGE("Failed to recv: %s", strerror(errno));
      return -1;
    }
  }

  return ret;
}
