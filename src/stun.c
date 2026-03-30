#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stun.h"
#include "utils.h"

uint32_t CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
    0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd,
    0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d,
    0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
    0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac,
    0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
    0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
    0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb,
    0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea,
    0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce,
    0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
    0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409,
    0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739,
    0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268,
    0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0,
    0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8,
    0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
    0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
    0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
    0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae,
    0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6,
    0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d,
    0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5,
    0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
    0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

void stun_msg_create(StunMessage* msg, uint16_t type) {
  StunHeader* header = (StunHeader*)msg->buf;
  header->type = htons(type);
  header->length = 0;
  header->magic_cookie = htonl(MAGIC_COOKIE);
  // RFC 5389: Transaction ID MUST be uniformly and randomly chosen
  // Use rand() to generate unique IDs for each message
  header->transaction_id[0] = htonl((uint32_t)rand());
  header->transaction_id[1] = htonl((uint32_t)rand());
  header->transaction_id[2] = htonl((uint32_t)rand());
  msg->size = sizeof(StunHeader);
}

int stun_set_mapped_address(char* value, uint8_t* mask, Address* addr) {
  int ret, i;
  char addr_string[ADDRSTRLEN];
  uint8_t* family = (uint8_t*)(value + 1);
  uint16_t* port = (uint16_t*)(value + 2);
  uint32_t* val32 = (uint32_t*)(value + 4);
  uint16_t* val16 = (uint16_t*)(value + 4);
  uint32_t* addr32 = (uint32_t*)(&addr->sin.sin_addr);
  uint16_t* addr16 = (uint16_t*)(&addr->sin6.sin6_addr);

  switch (addr->family) {
    case AF_INET6:
      *family = STUN_FAMILY_IPV6;
      for (i = 0; i < 8; i++) {
        val16[i] = addr16[i] ^ *(uint16_t*)(mask + 2 * i);
      }
      ret = 20;
      break;
    case AF_INET:
    default:
      *family = STUN_FAMILY_IPV4;
      *val32 = *addr32 ^ *(uint32_t*)mask;
      ret = 8;
      break;
  }

  *port = htons(addr->port) ^ *(uint16_t*)mask;
  addr_to_string(addr, addr_string, sizeof(addr_string));

  LOGD("XOR Mapped Address Family: %d", *family);
  LOGD("XOR Mapped Address Port: %d (Port XOR: %04x)", addr->port, *port);
  LOGD("XOR Mapped Address IP: %s (IP XOR: %08" PRIu32 ")", addr_string, *addr32);
  return ret;
}

void stun_get_mapped_address(char* value, uint8_t* mask, Address* addr) {
  int i;
  char addr_string[ADDRSTRLEN];
  uint32_t* addr32 = (uint32_t*)&addr->sin.sin_addr;
  uint16_t* addr16 = (uint16_t*)&addr->sin6.sin6_addr;
  uint8_t family = value[1];
  uint16_t port;

  switch (family) {
    case STUN_FAMILY_IPV6:
      addr_set_family(addr, AF_INET6);
      for (i = 0; i < 8; i++) {
        addr16[i] = (*(uint16_t*)(value + 4 + 2 * i) ^ *(uint16_t*)(mask + 2 * i));
      }
      break;
    case STUN_FAMILY_IPV4:
    default:
      addr_set_family(addr, AF_INET);
      *addr32 = (*(uint32_t*)(value + 4) ^ *(uint32_t*)mask);
      break;
  }

  port = ntohs(*(uint16_t*)(value + 2) ^ *(uint16_t*)mask);
  addr_to_string(addr, addr_string, sizeof(addr_string));
  addr_set_port(addr, port);

  LOGD("XOR Mapped Address Family: %d", family);
  LOGD("XOR Mapped Address Port: %d (Port XOR: %04x)", addr->port, port);
  LOGD("XOR Mapped Address IP: %s (IP XOR: %08" PRIu32 ")", addr_string, *addr32);
}

void stun_parse_msg_buf(StunMessage* msg) {
  StunHeader* header = (StunHeader*)msg->buf;

  int length = ntohs(header->length) + sizeof(StunHeader);

  int pos = sizeof(StunHeader);

  uint8_t mask[16];

  // Extract class and method according to RFC 5389
  // Type field encoding: M11 M10 M9 M8 M7 C1 M6 M5 M4 C0 M3 M2 M1 M0
  // where M0-M11 are method bits and C0-C1 are class bits
  uint16_t raw_type = ntohs(header->type);
  
  // Extract class: C0 at bit 4, C1 at bit 8
  // STUN_CLASS constants use these same bit positions
  msg->stunclass = raw_type & 0x0110;
  
  // Extract method: M0-M3 from bits 0-3, M4-M6 from bits 5-7, M7-M11 from bits 9-13
  msg->stunmethod = (raw_type & 0x000F) |           // M0-M3 stay in bits 0-3
                    ((raw_type & 0x00E0) >> 1) |    // M4-M6 from bits 5-7 to bits 4-6
                    ((raw_type & 0x3E00) >> 2);     // M7-M11 from bits 9-13 to bits 7-11

  while (pos < length) {
    StunAttribute* attr = (StunAttribute*)(msg->buf + pos);
    memset(mask, 0, sizeof(mask));
    // LOGD("Attribute Type: 0x%04x", ntohs(attr->type));
    // LOGD("Attribute Length: %d", ntohs(attr->length));

    switch (ntohs(attr->type)) {
      case STUN_ATTR_TYPE_MAPPED_ADDRESS:
        stun_get_mapped_address(attr->value, mask, &msg->mapped_addr);
        break;
      case STUN_ATTR_TYPE_USERNAME: {
        uint16_t attr_len = ntohs(attr->length);
        memset(msg->username, 0, sizeof(msg->username));
        if (attr_len >= sizeof(msg->username)) {
          LOGE("USERNAME truncated! attr_len=%u, buf_size=%zu", attr_len, sizeof(msg->username));
          attr_len = sizeof(msg->username) - 1;
        }
        memcpy(msg->username, attr->value, attr_len);
        msg->username_len = attr_len;
        // LOGD("length = %d, Username %s", attr_len, msg->username);
        break;
      }
      case STUN_ATTR_TYPE_MESSAGE_INTEGRITY:
        memcpy(msg->message_integrity, attr->value, ntohs(attr->length));

        char message_integrity_hex[41];

        for (int i = 0; i < 20; i++) {
          sprintf(message_integrity_hex + 2 * i, "%02x", (uint8_t)msg->message_integrity[i]);
        }

        break;
      case STUN_ATTR_TYPE_LIFETIME:
        break;
      case STUN_ATTR_TYPE_REALM: {
        uint16_t attr_len = ntohs(attr->length);
        memset(msg->realm, 0, sizeof(msg->realm));
        if (attr_len >= sizeof(msg->realm)) {
          LOGE("REALM truncated! attr_len=%u, buf_size=%zu", attr_len, sizeof(msg->realm));
          attr_len = sizeof(msg->realm) - 1;
        }
        memcpy(msg->realm, attr->value, attr_len);
        msg->realm_len = attr_len;
        LOGD("Realm (len=%zu): %s", msg->realm_len, msg->realm);
        break;
      }
      case STUN_ATTR_TYPE_NONCE: {
        uint16_t attr_len = ntohs(attr->length);
        memset(msg->nonce, 0, sizeof(msg->nonce));
        if (attr_len >= sizeof(msg->nonce)) {
          LOGE("NONCE truncated! attr_len=%u, buf_size=%zu -- THIS BREAKS TURN AUTH!", attr_len, sizeof(msg->nonce));
          attr_len = sizeof(msg->nonce) - 1;
        }
        memcpy(msg->nonce, attr->value, attr_len);
        msg->nonce_len = attr_len;
        break;
      }
      case STUN_ATTR_TYPE_XOR_PEER_ADDRESS: {
        *((uint32_t*)mask) = htonl(MAGIC_COOKIE);
        memcpy(mask + 4, header->transaction_id, sizeof(header->transaction_id));
        stun_get_mapped_address(attr->value, mask, &msg->peer_addr);
        break;
      }
      case STUN_ATTR_TYPE_DATA: {
        size_t copy_len = ntohs(attr->length);
        if (copy_len > sizeof(msg->data)) {
          LOGW("STUN DATA truncated from %zu to %zu", copy_len, sizeof(msg->data));
          copy_len = sizeof(msg->data);
        }
        memcpy(msg->data, attr->value, copy_len);
        msg->data_len = copy_len;
        break;
      }
      case STUN_ATTR_TYPE_XOR_RELAYED_ADDRESS:
        *((uint32_t*)mask) = htonl(MAGIC_COOKIE);
        memcpy(mask + 4, header->transaction_id, sizeof(header->transaction_id));
        LOGD("XOR Relayed Address");
        stun_get_mapped_address(attr->value, mask, &msg->relayed_addr);
        break;
      case STUN_ATTR_TYPE_CHANNEL_NUMBER:
        // TURN ChannelBind CHANNEL-NUMBER attribute (2-byte channel, 2-byte RFFU). No-op parse.
        break;
      case STUN_ATTR_TYPE_XOR_MAPPED_ADDRESS:
        *((uint32_t*)mask) = htonl(MAGIC_COOKIE);
        memcpy(mask + 4, header->transaction_id, sizeof(header->transaction_id));
        stun_get_mapped_address(attr->value, mask, &msg->mapped_addr);
        break;
      case STUN_ATTR_TYPE_PRIORITY:
        break;
      case STUN_ATTR_TYPE_USE_CANDIDATE:
        // LOGD("Use Candidate");
        break;
      case STUN_ATTR_TYPE_FINGERPRINT:
        memcpy(&msg->fingerprint, attr->value, ntohs(attr->length));
        // LOGD("Fingerprint: 0x%.4x", msg->fingerprint);
        break;
      case STUN_ATTR_TYPE_ERROR_CODE: {
        // Parse ERROR-CODE attribute to see why TURN failed
        // Format: 21-bit padding, 3-bit class, 8-bit number
        // Followed by UTF-8 reason phrase
        if (ntohs(attr->length) >= 4) {
          uint8_t *err_data = (uint8_t *)(attr + 1);
          uint8_t err_class = err_data[2] & 0x07;  // Bits 0-2 of byte 2
          uint8_t err_number = err_data[3];         // Byte 3
          uint16_t err_code = err_class * 100 + err_number;
          
          // Reason phrase starts at byte 4
          int reason_len = ntohs(attr->length) - 4;
          char reason[128] = {0};
          if (reason_len > 0 && reason_len < sizeof(reason)) {
            memcpy(reason, &err_data[4], reason_len);
            reason[reason_len] = '\0';
          }
          
          LOGE("TURN ERROR-CODE %d: %s", err_code, reason[0] ? reason : "(no reason)");
        } else {
          LOGE("Malformed ERROR-CODE attribute (len=%d)", ntohs(attr->length));
        }
        break;
      }
      case STUN_ATTR_TYPE_ICE_CONTROLLED:
      case STUN_ATTR_TYPE_ICE_CONTROLLING:
      case STUN_ATTR_TYPE_NETWORK_COST:
        // Do nothing
        break;
      default:
        LOGE("Unknown Attribute Type: 0x%04x", ntohs(attr->type));
        break;
    }

    pos += 4 * ((ntohs(attr->length) + 3) / 4) + sizeof(StunAttribute);
  }
}

void stun_calculate_fingerprint(char* buf, size_t len, uint32_t* fingerprint) {
  uint32_t c = 0xFFFFFFFF;
  int i = 0;

  for (i = 0; i < len; ++i) {
    c = CRC32_TABLE[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
  }

  *fingerprint = htonl((c ^ 0xFFFFFFFF) ^ STUN_FINGERPRINT_XOR);
}

int stun_msg_write_attr(StunMessage* msg, StunAttrType type, uint16_t length, char* value) {
  StunHeader* header = (StunHeader*)msg->buf;

  StunAttribute* stun_attr = (StunAttribute*)(msg->buf + msg->size);

  uint16_t padded_len = 4 * ((length + 3) / 4);
  if (msg->size + sizeof(StunAttribute) + padded_len > STUN_ATTR_BUF_SIZE) {
    LOGE("stun_msg_write_attr: overflow (need %u, have %zu)", (unsigned)(msg->size + sizeof(StunAttribute) + padded_len), (size_t)STUN_ATTR_BUF_SIZE);
    return -1;
  }

  stun_attr->type = htons(type);
  stun_attr->length = htons(length);
  if (value)
    memcpy(stun_attr->value, value, length);

  length = padded_len;
  header->length = htons(ntohs(header->length) + sizeof(StunAttribute) + length);

  msg->size += length + sizeof(StunAttribute);

  // Store attribute values and their lengths for MESSAGE-INTEGRITY computation
  uint16_t orig_len = ntohs(stun_attr->length);  // Use original length before padding
  switch (type) {
    case STUN_ATTR_TYPE_REALM:
      if (orig_len < sizeof(msg->realm)) {
        memset(msg->realm, 0, sizeof(msg->realm));
        memcpy(msg->realm, value, orig_len);
        msg->realm_len = orig_len;
      }
      break;
    case STUN_ATTR_TYPE_NONCE:
      if (orig_len < sizeof(msg->nonce)) {
        memset(msg->nonce, 0, sizeof(msg->nonce));
        memcpy(msg->nonce, value, orig_len);
        msg->nonce_len = orig_len;
      }
      break;
    case STUN_ATTR_TYPE_USERNAME:
      if (orig_len < sizeof(msg->username)) {
        memset(msg->username, 0, sizeof(msg->username));
        memcpy(msg->username, value, orig_len);
        msg->username_len = orig_len;
      }
      break;
    default:
      break;
  }

  return 0;
}

int stun_msg_finish(StunMessage* msg, StunCredential credential, const char* password, size_t password_len) {
  StunHeader* header = (StunHeader*)msg->buf;
  StunAttribute* stun_attr;

  uint16_t header_length = ntohs(header->length);
  char key[512];  // Increased: username(256) + realm(128) + password + separators
  char hash_key[17];
  memset(key, 0, sizeof(key));
  memset(hash_key, 0, sizeof(hash_key));

  switch (credential) {
    case STUN_CREDENTIAL_LONG_TERM:
      snprintf(key, sizeof(key), "%s:%s:%s", msg->username, msg->realm, password);
      utils_get_md5(key, strlen(key), (unsigned char*)hash_key);
      password = hash_key;
      password_len = 16;
      break;
    default:
      break;
  }

  // MESSAGE-INTEGRITY:
  // Many deployed TURN servers (including Cloudflare) expect the HMAC to be computed
  // over the STUN message up to (but excluding) the MESSAGE-INTEGRITY attribute,
  // with the header length field including the MESSAGE-INTEGRITY attribute (24 bytes)
  // and excluding any later FINGERPRINT attribute.
  if (msg->size + sizeof(StunAttribute) + 20 > STUN_ATTR_BUF_SIZE) {
    LOGE("stun_msg_finish: overflow adding MESSAGE-INTEGRITY (need %zu, have %d)",
         msg->size + sizeof(StunAttribute) + 20, STUN_ATTR_BUF_SIZE);
    return -1;
  }
  stun_attr = (StunAttribute*)(msg->buf + msg->size);
  stun_attr->type = htons(STUN_ATTR_TYPE_MESSAGE_INTEGRITY);
  stun_attr->length = htons(20);
  header->length = htons(header_length + 24); /* header length INCLUDES MI */
  if (credential == STUN_CREDENTIAL_SHORT_TERM) {
    // RFC 5389 §15.4: For short-term credentials, the HMAC input is the message
    // UP TO (but not including) the MESSAGE-INTEGRITY attribute. The header length
    // is adjusted to include MI BEFORE computing the HMAC.
    utils_get_hmac_sha1((char*)msg->buf, msg->size, password, password_len, (unsigned char*)stun_attr->value);
  } else {
    // TURN long-term: same computation (HMAC excludes MI attribute).
    utils_get_hmac_sha1((char*)msg->buf, msg->size, password, password_len, (unsigned char*)stun_attr->value);
  }
  msg->size += sizeof(StunAttribute) + 20;

  // FINGERPRINT: RFC 5389 §15.5 says optional, but aiortc/aioice ALWAYS includes it.
  // Pipecat Cloud (aiortc-based) may silently reject binding requests without FINGERPRINT.
  // Always add FINGERPRINT for maximum interoperability.
  if (msg->size + sizeof(StunAttribute) + 4 > STUN_ATTR_BUF_SIZE) {
    LOGE("stun_msg_finish: overflow adding FINGERPRINT (need %zu, have %d)", msg->size + sizeof(StunAttribute) + 4, STUN_ATTR_BUF_SIZE);
    return -1;
  }
  stun_attr = (StunAttribute*)(msg->buf + msg->size);
  // Header length must include the FINGERPRINT attribute (8 bytes total)
  header_length = ntohs(header->length);
  header->length = htons(header_length + 8);
  stun_attr->type = htons(STUN_ATTR_TYPE_FINGERPRINT);
  stun_attr->length = htons(4);
  stun_calculate_fingerprint((char*)msg->buf, msg->size, (uint32_t*)stun_attr->value);
  msg->size += sizeof(StunAttribute) + 4;

  return 0;
}

// Detect TURN ChannelData packets per RFC 5766 §11: first two bits must be 01
int stun_is_channel_data(const uint8_t* data, size_t len) {
  if (data == NULL || len < 1) {
    return 0;
  }
  return (data[0] & 0xC0) == 0x40;
}

int stun_probe(uint8_t* buf, size_t size) {
  StunHeader* header;
  if (size < sizeof(StunHeader)) {
    LOGE("STUN message is too short.");
    return -1;
  }

  header = (StunHeader*)buf;
  if (header->magic_cookie != htonl(MAGIC_COOKIE)) {
    return -1;
  }

  return 0;
}

#if 0
StunMsgType stun_is_stun_msg(uint8_t *buf, size_t size) {

  if (size < sizeof(StunHeader)) {
    //LOGE("STUN message is too short.");
    return STUN_MSG_TYPE_INVLID;
  }

  StunHeader *header = (StunHeader *)buf;
  if (header->magic_cookie != htonl(MAGIC_COOKIE)) {
    //LOGE("STUN magic cookie does not match.");
    return STUN_MSG_TYPE_INVLID;
  }

  if (ntohs(header->type) == STUN_BINDING_REQUEST) {

    return STUN_MSG_TYPE_BINDING_REQUEST;

  } else if (ntohs(header->type) == STUN_BINDING_RESPONSE) {

    return STUN_MSG_TYPE_BINDING_RESPONSE;

  } else if (ntohs(header->type) == STUN_BINDING_ERROR_RESPONSE) {

    return STUN_MSG_TYPE_BINDING_ERROR_RESPONSE;
  } else {

    return STUN_MSG_TYPE_INVLID;
  }

  return 0;
}
#endif
int stun_msg_is_valid(uint8_t* buf, size_t size, const char* password, size_t password_len) {
  StunMessage msg;

  memcpy(msg.buf, buf, size);

  stun_parse_msg_buf(&msg);

  StunHeader* header = (StunHeader*)msg.buf;

  // FINGERPRINT (optional).
  // If absent, msg.fingerprint will be 0; skip validation in that case.
  if (msg.fingerprint != 0) {
    uint32_t fingerprint = 0;
    size_t length = size - 4 - sizeof(StunAttribute);
    stun_calculate_fingerprint((char*)msg.buf, length, &fingerprint);
    if (fingerprint != msg.fingerprint) {
      return -1;
    }
  }

  // MESSAGE-INTEGRITY
  // Validate MESSAGE-INTEGRITY per RFC 5389 semantics used by ICE short-term credentials:
  // compute HMAC over the message up to and including the MESSAGE-INTEGRITY attribute,
  // with the MI value bytes treated as zero during computation, and with the header
  // length excluding the FINGERPRINT attribute.
  unsigned char computed_mi[20];
  memset(computed_mi, 0, sizeof(computed_mi));

  // Copy message excluding FINGERPRINT (assumed last).
  size_t msg_no_fp_size = size - (sizeof(StunAttribute) + 4);
  if (msg_no_fp_size > STUN_ATTR_BUF_SIZE) {
    return -1;
  }
  uint8_t tmp[STUN_ATTR_BUF_SIZE];
  memcpy(tmp, msg.buf, msg_no_fp_size);

  // Adjust header length to exclude fingerprint.
  StunHeader* tmp_header = (StunHeader*)tmp;
  tmp_header->length = htons(ntohs(tmp_header->length) - (uint16_t)(sizeof(StunAttribute) + 4));

  // Find MESSAGE-INTEGRITY offset.
  size_t pos = sizeof(StunHeader);
  size_t mi_attr_offset = 0;
  while (pos + sizeof(StunAttribute) <= msg_no_fp_size) {
    StunAttribute* attr = (StunAttribute*)(tmp + pos);
    uint16_t attr_type = ntohs(attr->type);
    uint16_t attr_len = ntohs(attr->length);
    size_t padded_len = 4 * ((attr_len + 3) / 4);
    if (pos + sizeof(StunAttribute) + padded_len > msg_no_fp_size) {
      break;
    }
    if (attr_type == STUN_ATTR_TYPE_MESSAGE_INTEGRITY && attr_len == 20) {
      mi_attr_offset = pos;
      break;
    }
    pos += sizeof(StunAttribute) + padded_len;
  }
  if (mi_attr_offset == 0) {
    return -1;
  }

  // RFC 5389 §15.4: HMAC input is up to (but NOT including) the MESSAGE-INTEGRITY attribute.
  // The header length should already be adjusted to include MI in the received message.
  // We compute HMAC over the bytes up to the MI attribute offset.
  utils_get_hmac_sha1((char*)tmp, mi_attr_offset, password, password_len, computed_mi);

  if (memcmp(computed_mi, msg.message_integrity, 20) != 0) {
    // LOGE("Message Integrity does not match.");
    return -1;
  } else {
    // LOGD("Message Integrity matches.");
  }

  return 0;
}
