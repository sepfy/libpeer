#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "sctp.h"
#ifdef HAVE_USRSCTP
#include <usrsctp.h>
#endif

#include "dtls_srtp.h"
#include "utils.h"

#define DATA_CHANNEL_PPID_CONTROL           50
#define DATA_CHANNEL_PPID_DOMSTRING         51
#define DATA_CHANNEL_PPID_BINARY_PARTIAL    52
#define DATA_CHANNEL_PPID_BINARY            53
#define DATA_CHANNEL_PPID_DOMSTRING_PARTIAL 54

static const uint32_t crc32c_table[256] = {
    0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
    0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
    0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
    0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
    0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
    0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
    0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
    0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
    0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
    0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
    0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
    0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
    0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
    0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
    0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
    0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
    0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
    0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
    0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
    0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
    0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
    0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
    0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
    0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
    0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
    0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
    0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
    0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
    0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
    0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
    0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
    0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
    0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
    0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
    0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
    0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
    0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
    0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
    0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
    0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
    0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
    0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
    0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
    0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
    0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
    0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
    0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
    0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
    0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
    0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
    0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
    0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
    0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
    0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
    0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
    0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
    0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
    0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
    0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
    0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
    0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
    0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
    0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
    0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L
};


uint32_t crc32c(uint32_t crc, const uint8_t *data, unsigned int length)
{
    while (length--) {
        crc = crc32c_table[(crc ^ *data++) & 0xFFL] ^ (crc >> 8);
    }
    return crc^0xffffffff;
}


static uint32_t sctp_get_checksum(Sctp *sctp, char *buf, size_t len) {

  uint32_t crc = crc32c(0xffffffff, buf, len);
  return crc;
}

int sctp_outgoing_data(Sctp *sctp, char *buf, size_t len) {

#ifdef HAVE_USRSCTP
  struct sctp_sendv_spa spa = {0};

  spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;

  spa.sendv_sndinfo.snd_sid = 0;
  spa.sendv_sndinfo.snd_flags = SCTP_EOR;

  // check ACSII
  if ((buf[0] & 0x80) || (buf[1] & 0x80)) {
    spa.sendv_sndinfo.snd_ppid = htonl(53);
  } else {
    spa.sendv_sndinfo.snd_ppid = htonl(51);
  }

  if(usrsctp_sendv(sctp->sock, buf, len, NULL, 0, &spa, sizeof(spa), SCTP_SENDV_SPA, 0) < 0) {
    LOGE("sctp sendv error");
    return -1;
  }
#else

  static uint32_t tsn = 0;
  SctpPacket *packet = (SctpPacket*)(sctp->buf);
  SctpDataChunk *chunk = (SctpDataChunk*)(packet->chunks);

  packet->header.source_port = htons(sctp->local_port);
  packet->header.destination_port = htons(sctp->remote_port);
  packet->header.verification_tag = sctp->verification_tag;

  chunk->type = SCTP_DATA;
  chunk->iube = 0x03;
  chunk->length = htons(len + sizeof(SctpDataChunk));
  chunk->tsn = htonl(sctp->tsn++);
  chunk->si = htons(0);
  chunk->sqn = htons(0);
  chunk->ppid = htonl(DATA_CHANNEL_PPID_DOMSTRING);
  memcpy(chunk->data, buf, len);
  packet->header.checksum = 0;

  len = 4 * ((len + sizeof(SctpDataChunk) + sizeof(SctpPacket) + 3) / 4);
  packet->header.checksum = sctp_get_checksum(sctp, sctp->buf, len);
  dtls_srtp_write(sctp->dtls_srtp, sctp->buf, len);

#endif
  return len;
}

static int sctp_outgoing_data_cb(void *userdata, void *buf, size_t len, uint8_t tos, uint8_t set_df) {

  Sctp *sctp = (Sctp*)userdata;

  dtls_srtp_write(sctp->dtls_srtp, (const unsigned char*)buf, len);
  return 0;
}

void sctp_incoming_data(Sctp *sctp, char *buf, size_t len) {

  if(!sctp)
    return;

#ifdef HAVE_USRSCTP
  usrsctp_conninput(sctp, buf, len, 0);
#else

  size_t length = 0;
  int i;
  size_t pos = sizeof(SctpHeader);
  SctpChunkCommon *common_chunk;
  SctpChunkCommon *chunk_common;
  SctpDataChunk *data_chunk;
  SctpSackChunk *sack;
  SctpPacket *in_packet = (SctpPacket*)buf;
  SctpPacket *out_packet = (SctpPacket*)sctp->buf;

  // Header
  LOGD("source_port %d", ntohs(in_packet->header.source_port));
  LOGD("destination_port %d", ntohs(in_packet->header.destination_port));
  LOGD("verification_tag %d", ntohl(in_packet->header.verification_tag));
  LOGD("checksum %d", ntohs(in_packet->header.checksum));

  uint32_t crc32c = in_packet->header.checksum;

  in_packet->header.checksum = 0;

  if (crc32c != sctp_get_checksum(sctp, buf, len)) {
    LOGE("checksum error");
    return;
  }

  // prepare outgoing packet
  memset(sctp->buf, 0, sizeof(sctp->buf));

  while ((4*(pos + 3)/4) < len) {

    chunk_common = (SctpChunkCommon*)(buf + pos);

    switch (chunk_common->type) {

      case SCTP_DATA:

        data_chunk = (SctpDataChunk*)in_packet->chunks;

        if (ntohl(data_chunk->ppid) == DATA_CHANNEL_PPID_CONTROL && data_chunk->data[0] == 0x03) {

          sack = (SctpSackChunk*)out_packet->chunks;
          sack->common.type = SCTP_SACK;
          sack->common.flags = 0x00;
          sack->common.length = htons(16);
          sack->cumulative_tsn_ack = data_chunk->tsn;
          sack->a_rwnd = htonl(0x02);
          length = ntohs(sack->common.length) + sizeof(SctpHeader);
          sctp->connected = 1;
          if(sctp->onopen) {
            sctp->onopen(sctp->userdata);
          }
        }
        break;
      case SCTP_INIT:
        LOGD("SCTP_INIT");

        SctpInitChunk *init_chunk;
        init_chunk = (SctpInitChunk*)in_packet->chunks;
        sctp->verification_tag = init_chunk->initiate_tag;

        SctpInitChunk *init_ack = (SctpInitChunk*)out_packet->chunks;
        init_ack->common.type = SCTP_INIT_ACK;
        init_ack->common.flags = 0x00;
        init_ack->common.length = htons(20 + 8);
        init_ack->initiate_tag = htonl(0x12345678);
        init_ack->a_rwnd = htonl(0x100000);
        init_ack->number_of_outbound_streams = 0xffff;
        init_ack->number_of_inbound_streams = 0xffff;
        init_ack->initial_tsn = htonl(sctp->tsn++);

        SctpChunkParam *param = init_ack->param;

        param->type = htons(SCTP_PARAM_STATE_COOKIE);
        param->length = htons(0x08);
        uint32_t value = htonl(0x02);
        memcpy(&param->value, &value, 4);
        length = ntohs(init_ack->common.length) + sizeof(SctpHeader);
        break;
      case SCTP_COOKIE_ECHO:
        LOGD("SCTP_COOKIE_ECHO");
        SctpChunkCommon *common = (SctpChunkCommon*)out_packet->chunks;
        common->type = SCTP_COOKIE_ACK;
        common->length = htons(4);
        length = ntohs(common->length) + sizeof(SctpHeader);
        break;
      default:
        break;
    }

    out_packet->header.source_port = htons(sctp->local_port);
    out_packet->header.destination_port = htons(sctp->remote_port);
    out_packet->header.verification_tag = sctp->verification_tag;
    out_packet->header.checksum = 0x00;

    if (length > 0) {
      // padding 4
      length = (4*((length + 3)/4));
      out_packet->header.checksum = sctp_get_checksum(sctp, sctp->buf, length);
      dtls_srtp_write(sctp->dtls_srtp, sctp->buf, length);
    }
    pos += ntohs(chunk_common->length);
  }
#endif

}

static int sctp_handle_incoming_data(Sctp *sctp, char *data, size_t len, uint32_t ppid, uint16_t stream, int flags) {

#ifdef HAVE_USRSCTP
  char *msg = NULL;

  switch(ppid) {
    case DATA_CHANNEL_PPID_CONTROL:

      sctp->connected = 1;

      if(sctp->onopen) {
        sctp->onopen(sctp->userdata);
      }

      break;
    case DATA_CHANNEL_PPID_DOMSTRING:

      LOGD("Got message %s (size = %ld)", msg, len);
      msg = strndup(data, len);
      if(msg && sctp->onmessasge) {
        sctp->onmessasge(msg, len, sctp->userdata);
        free(msg);
      }
      break;
    case DATA_CHANNEL_PPID_BINARY:
      break;
    case DATA_CHANNEL_PPID_DOMSTRING_PARTIAL:
      break;
    case DATA_CHANNEL_PPID_BINARY_PARTIAL:
      break;
    default:
      break;
  }
#endif
  return 0;
}


#ifdef HAVE_USRSCTP
static int sctp_incoming_data_cb(struct socket *sock, union sctp_sockstore addr,
 void *data, size_t len, struct sctp_rcvinfo recv_info, int flags, void *userdata) {
  Sctp *sctp = (Sctp*)userdata;
  LOGD("Data of length %u received on stream %u with SSN %u, TSN %u, PPID %u\n",
    (uint32_t)len,
    recv_info.rcv_sid,
    recv_info.rcv_ssn,
    recv_info.rcv_tsn,
    ntohl(recv_info.rcv_ppid));

  if(flags & MSG_NOTIFICATION) {
    LOGI("MSG_NOTIFICATION");
  }
  else {
    sctp_handle_incoming_data(sctp, data, len, ntohl(recv_info.rcv_ppid), recv_info.rcv_sid, flags);
  }
  return 0;
}
#endif

int sctp_create_socket(Sctp *sctp, DtlsSrtp *dtls_srtp) {

  int ret = -1;
  sctp->dtls_srtp = dtls_srtp;
  sctp->local_port = 5000;
  sctp->remote_port = 5000;

#ifdef HAVE_USRSCTP
  usrsctp_init(0, sctp_outgoing_data_cb, NULL);
  usrsctp_sysctl_set_sctp_ecn_enable(0);
  usrsctp_register_address(sctp);

  struct socket *sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
   sctp_incoming_data_cb, NULL, 0, sctp);

  if(!sock) {
    LOGE("usrsctp_socket failed");
    return -1;
  }

  do {

    if(usrsctp_set_non_blocking(sock, 1) < 0) {
      LOGE("usrsctp_set_non_blocking failed");
      break;
    }

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

    struct sockaddr_conn rconn;

    memset(&rconn, 0, sizeof(struct sockaddr_conn));
    rconn.sconn_family = AF_CONN;
    rconn.sconn_port = htons(sctp->remote_port);
    rconn.sconn_addr = (void*)sctp;
    ret = usrsctp_connect(sock, (struct sockaddr *)&rconn, sizeof(struct sockaddr_conn));

    if(ret < 0 && errno != EINPROGRESS) {
      LOGE("connect error");
      break;
    }

    ret = 0;

  } while(0);

  if(ret < 0) {

    usrsctp_shutdown(sctp->sock, SHUT_RDWR);
    usrsctp_close(sctp->sock);
    sctp->sock = NULL;
    return -1;
  }

  sctp->sock = sock;
#endif

  return 0;
}

int sctp_is_connected(Sctp *sctp) {

  return sctp->connected;
}

void sctp_destroy(Sctp *sctp) {
#ifdef HAVE_USRSCTP
  if(sctp) {

    if(sctp->sock) {
      usrsctp_shutdown(sctp->sock, SHUT_RDWR);
      usrsctp_close(sctp->sock);
      sctp->sock = NULL;
    }

    free(sctp);
    sctp = NULL;
  }
#endif
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

