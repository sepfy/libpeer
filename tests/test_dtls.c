#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dtls_srtp.h"

void test_handshake(int argc, char *argv[]) {
#if 0
  DtlsSrtp dtls_srtp;
  UdpSocket udp_socket;
  Address local_addr;
  Address remote_addr;

  if (argc < 2) {

    printf("Usage: %s client/server\n", argv[0]);
    return;
  }

  local_addr.ipv4[0] = 192;
  local_addr.ipv4[1] = 168;
  local_addr.ipv4[2] = 1;
  local_addr.ipv4[3] = 110;

  remote_addr.ipv4[0] = 192;
  remote_addr.ipv4[1] = 168;
  remote_addr.ipv4[2] = 1;
  remote_addr.ipv4[3] = 110;

  if (strstr(argv[1], "client")) {

    local_addr.port = 1234;
    remote_addr.port = 5677; // 5678 for MNDP
    dtls_srtp_init(&dtls_srtp, DTLS_SRTP_ROLE_CLIENT, &udp_socket);

  } else {

    local_addr.port = 5677; // 5678 for MNDP
    remote_addr.port = 1234;
    dtls_srtp_init(&dtls_srtp, DTLS_SRTP_ROLE_SERVER, &udp_socket);
  }

  udp_socket_open(&udp_socket);

  udp_socket_bind(&udp_socket, &local_addr);

  dtls_srtp_handshake(&dtls_srtp, &remote_addr);

  char buf[64];

  memset(buf, 0, sizeof(buf));

  if (strstr(argv[1], "client")) {

    snprintf(buf, sizeof(buf), "hello from client");

    printf("client sending: %s\n", buf);

    usleep(100 * 1000);

    dtls_srtp_write(&dtls_srtp, buf, sizeof(buf));

    dtls_srtp_read(&dtls_srtp, buf, sizeof(buf));

    printf("client received: %s\n", buf);

  } else {

    dtls_srtp_read(&dtls_srtp, buf, sizeof(buf));

    printf("server received: %s\n", buf);

    snprintf(buf, sizeof(buf), "hello from server");

    printf("server sending: %s\n", buf);

    usleep(100 * 1000);

    dtls_srtp_write(&dtls_srtp, buf, sizeof(buf));

  }

  dtls_srtp_deinit(&dtls_srtp);
#endif
}

void test_reset() {

  DtlsSrtp dtls_srtp;
  dtls_srtp_init(&dtls_srtp, DTLS_SRTP_ROLE_CLIENT, NULL);
  dtls_srtp_deinit(&dtls_srtp);
}

int main(int argc, char *argv[]) {

  test_reset();
  test_handshake(argc, argv);

  return 0;
}

