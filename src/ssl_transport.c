#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "utils.h"
#include "config.h"
#include "ports.h"
#include "ssl_transport.h"

static int ssl_transport_mbedtls_recv(void *ctx, unsigned char *buf, size_t len) {

  return tcp_socket_recv((TcpSocket*)ctx, buf, len);
}

static int ssl_transport_mbedlts_send(void *ctx, const uint8_t *buf, size_t len) {

  return tcp_socket_send((TcpSocket*)ctx, buf, len);
}

int ssl_transport_connect(NetworkContext_t *net_ctx,
 const char *host, uint16_t port, const char *cacert) {

  const char *pers = "ssl_client";
  int ret;
  Address resolved_addr;

  mbedtls_ssl_init(&net_ctx->ssl);
  mbedtls_ssl_config_init(&net_ctx->conf);
  //mbedtls_x509_crt_init(&net_ctx->cacert);
  mbedtls_ctr_drbg_init(&net_ctx->ctr_drbg);
  mbedtls_entropy_init(&net_ctx->entropy);

  if ((ret = mbedtls_ctr_drbg_seed(&net_ctx->ctr_drbg, mbedtls_entropy_func, &net_ctx->entropy,
   (const unsigned char *) pers, strlen(pers))) != 0) {
  }

  if ((ret = mbedtls_ssl_config_defaults(&net_ctx->conf,
   MBEDTLS_SSL_IS_CLIENT,
   MBEDTLS_SSL_TRANSPORT_STREAM,
   MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {

    LOGE("ssl config error: -0x%x", (unsigned int) -ret);
  }


  mbedtls_ssl_conf_authmode(&net_ctx->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);

  /*
  XXX: not sure if this is needed
  ret = mbedtls_x509_crt_parse(&net_ctx->cacert, (const unsigned char *) cacert, strlen(cacert) + 1);
  if (ret < 0) {
    LOGE("ssl parse error: -0x%x", (unsigned int) -ret);
  }
  mbedtls_ssl_conf_ca_chain(&net_ctx->conf, &net_ctx->cacert, NULL);
  */

  mbedtls_ssl_conf_rng(&net_ctx->conf, mbedtls_ctr_drbg_random, &net_ctx->ctr_drbg);

  if ((ret = mbedtls_ssl_setup(&net_ctx->ssl, &net_ctx->conf)) != 0) {
    LOGE("ssl setup error: -0x%x", (unsigned int) -ret);
  }

  if ((ret = mbedtls_ssl_set_hostname(&net_ctx->ssl, host)) != 0) {
    LOGE("ssl set hostname error: -0x%x", (unsigned int) -ret);
  }

  tcp_socket_open(&net_ctx->tcp_socket);
  tcp_blocking_timeout(&net_ctx->tcp_socket, 1000);
  ports_resolve_addr(host, &resolved_addr);

  resolved_addr.port = port;
  tcp_socket_connect(&net_ctx->tcp_socket, &resolved_addr);

  mbedtls_ssl_set_bio(&net_ctx->ssl, &net_ctx->tcp_socket,
   ssl_transport_mbedlts_send, ssl_transport_mbedtls_recv, NULL);

  LOGI("start to handshake");

  while ((ret = mbedtls_ssl_handshake(&net_ctx->ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      LOGE("ssl handshake error: -0x%x", (unsigned int) -ret);
    }
  }

  LOGI("handshake success");

  return 0;
}

void ssl_transport_disconnect(NetworkContext_t *net_ctx) {

  mbedtls_ssl_config_free(&net_ctx->conf);
  //mbedtls_x509_crt_free(&net_ctx->cacert);
  mbedtls_ctr_drbg_free(&net_ctx->ctr_drbg);
  mbedtls_entropy_free(&net_ctx->entropy);
  mbedtls_ssl_free(&net_ctx->ssl);

  tcp_socket_close(&net_ctx->tcp_socket);
}

int ssl_transport_recv(NetworkContext_t *net_ctx, void *buf, size_t len) {

  int ret;
  memset(buf, 0, len);
  ret = mbedtls_ssl_read(&net_ctx->ssl, buf, len);

  return ret;
}

int ssl_transport_send(NetworkContext_t *net_ctx, const void *buf, size_t len) {

  int ret;

  while ((ret = mbedtls_ssl_write(&net_ctx->ssl, buf, len)) <= 0) {

    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      LOGE("");
    }
  }

  return ret;
}
