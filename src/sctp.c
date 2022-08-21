#include <stdlib.h>
#include <string.h>
#include <usrsctp.h>
#include <pthread.h>

#include "dtls_transport.h"
#include "sctp.h"
#include "utils.h"

typedef struct Sctp {

  struct socket *sock;

  int local_port;
  int remote_port;
  int connected;

  DtlsTransport *dtls_transport;

  /* datachannel */
  void (*onmessasge)(char *msg, size_t len, void *userdata);
  void (*onopen)(void *userdata);
  void (*onclose)(void *userdata);

  void *userdata;

} Sctp;

static int sctp_outgoing_data_cb(void *userdata, void *buf, size_t len, uint8_t tos, uint8_t set_df) {

  Sctp *sctp = (Sctp*)userdata;

  dtls_transport_sctp_to_dtls(sctp->dtls_transport, buf, len);

  return 0;
}

void sctp_incoming_data(Sctp *sctp, char *buf, int len) {

  if(!sctp) return;
  usrsctp_conninput(sctp, buf, len, 0);
}


static int sctp_incoming_data_cb(struct socket *sock, union sctp_sockstore addr,
 void *data, size_t len, struct sctp_rcvinfo recv_info, int flags, void *userdata) {

  char *msg = (char*)calloc(1, len + 1);

  Sctp *sctp = (Sctp*)userdata;

  if(!msg)
    return -1;

  strncpy(msg, data, len);
  LOG_DEBUG("Got message %s (size = %ld)", msg, len);

  if(sctp->onmessasge) {
    sctp->onmessasge(msg, len, sctp->userdata);
  }

  free(msg);
}

void* sctp_connection_thread(void *data) {
  int ret= 0;
  Sctp *sctp = (Sctp*)data;

  struct sockaddr_conn rconn;

  memset(&rconn, 0, sizeof(struct sockaddr_conn));
  rconn.sconn_family = AF_CONN;
  rconn.sconn_port = htons(sctp->remote_port);
  rconn.sconn_addr = (void*)sctp;
  ret = usrsctp_connect(sctp->sock, (struct sockaddr *)&rconn, sizeof(struct sockaddr_conn));

  if(sctp->onopen) {
    sctp->onopen(sctp->userdata);
  }

  pthread_exit(NULL);
}


Sctp* sctp_create(DtlsTransport *dtls_transport) {

  Sctp *sctp = (Sctp*)calloc(1, sizeof(Sctp));

  if(sctp == NULL)
    return NULL;

  sctp->dtls_transport = dtls_transport;
  sctp->local_port = 5000;
  sctp->remote_port = 5000;

  return sctp;
}

int sctp_do_connect(Sctp *sctp) {

  int ret = -1;
  usrsctp_init(0, sctp_outgoing_data_cb, NULL);
  usrsctp_sysctl_set_sctp_ecn_enable(0);
  usrsctp_register_address(sctp);

  struct socket *sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
   sctp_incoming_data_cb, NULL, 0, sctp);

  if(!sock) {
    LOG_ERROR("usrsctp_socket failed");
    return -1;
  }
#if 0
  if(usrsctp_set_non_blocking(sock, 1) < 0) {
    LOG_ERROR("usrsctp_set_non_blocking failed");
  }
#endif
  struct linger lopt;
  lopt.l_onoff = 1;
  lopt.l_linger = 0;
  usrsctp_setsockopt(sock, SOL_SOCKET, SO_LINGER, &lopt, sizeof(lopt));

#if 0
  struct sctp_paddrparams peer_param;
  memset(&peer_param, 0, sizeof peer_param);
  peer_param.spp_flags = SPP_PMTUD_DISABLE;
  peer_param.spp_pathmtu = 1200;
  usrsctp_setsockopt(s, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &peer_param, sizeof peer_param);
#endif

  struct sctp_assoc_value av;
  av.assoc_id = SCTP_ALL_ASSOC;
  av.assoc_value = 1;
  usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av, sizeof(av));

  uint32_t nodelay = 1;
  usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_NODELAY, &nodelay, sizeof(nodelay));

  struct sctp_initmsg init_msg;
  memset(&init_msg, 0, sizeof init_msg);
  init_msg.sinit_num_ostreams = 300;
  init_msg.sinit_max_instreams = 300;
  usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_INITMSG, &init_msg, sizeof init_msg);

  struct sockaddr_conn sconn;
  memset(&sconn, 0, sizeof(sconn));
  sconn.sconn_family = AF_CONN;
  sconn.sconn_port = htons(sctp->local_port);
  sconn.sconn_addr = (void *)sctp;
  ret = usrsctp_bind(sock, (struct sockaddr *)&sconn, sizeof(sconn));

  sctp->sock = sock;

  sctp->connected = 1;
  pthread_t t;
  pthread_create(&t, NULL, sctp_connection_thread, sctp);
  pthread_detach(t);

  return 0;
}

int sctp_is_connected(Sctp *sctp) {

  return sctp->connected;
}

void sctp_destroy(Sctp *sctp) {

  if(sctp) {

    if(sctp->sock) {
      usrsctp_shutdown(sctp->sock, SHUT_RDWR);
      usrsctp_close(sctp->sock);
      sctp->sock = NULL;
    }

    free(sctp);
    sctp = NULL;
  }
}

void sctp_onmessage(Sctp *sctp, void (*onmessasge)(char *msg, size_t len, void *userdata)) {

  sctp->onmessasge = onmessasge;
}

void sctp_onopen(Sctp *sctp, void (*onopen)(void *userdata)) {

  sctp->onopen = onopen;
}

void sctp_onclose(Sctp *sctp, void (*onclose)(void *userdata)) {

  sctp->onclose = onclose;
}

