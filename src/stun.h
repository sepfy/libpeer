#ifndef STUN_H_
#define STUN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "address.h"

typedef struct StunHeader StunHeader;

typedef struct StunAttribute StunAttribute;

typedef struct StunMessage StunMessage;

#define STUN_ATTR_BUF_SIZE 256
#define MAGIC_COOKIE 0x2112A442
#define STUN_FINGERPRINT_XOR 0x5354554e

typedef enum StunClass {

  STUN_CLASS_REQUEST = 0x0000,
  STUN_CLASS_INDICATION = 0x0010,
  STUN_CLASS_RESPONSE = 0x0100,
  STUN_CLASS_ERROR = 0x0110,

} StunClass;

typedef enum StunMethod {

  STUN_METHOD_BINDING = 0x0001,
  STUN_METHOD_ALLOCATE = 0x0003,

} StunMethod;

typedef enum StunAttrType {

  STUN_ATTR_TYPE_MAPPED_ADDRESS = 0x0001,
  STUN_ATTR_TYPE_USERNAME = 0x0006,
  STUN_ATTR_TYPE_MESSAGE_INTEGRITY = 0x0008,
  STUN_ATTR_TYPE_LIFETIME = 0x000d,
  STUN_ATTR_TYPE_REALM = 0x0014,
  STUN_ATTR_TYPE_NONCE = 0x0015,
  STUN_ATTR_TYPE_XOR_RELAYED_ADDRESS = 0x0016,
  STUN_ATTR_TYPE_REQUESTED_TRANSPORT = 0x0019,
  STUN_ATTR_TYPE_XOR_MAPPED_ADDRESS = 0x0020,
  STUN_ATTR_TYPE_PRIORITY = 0x0024,
  STUN_ATTR_TYPE_USE_CANDIDATE = 0x0025,
  STUN_ATTR_TYPE_FINGERPRINT = 0x8028,
  STUN_ATTR_TYPE_ICE_CONTROLLED = 0x8029,
  STUN_ATTR_TYPE_ICE_CONTROLLING = 0x802a,
  STUN_ATTR_TYPE_SOFTWARE = 0x8022,
  // https://datatracker.ietf.org/doc/html/draft-thatcher-ice-network-cost-00
  STUN_ATTR_TYPE_NETWORK_COST = 0xc057,

} StunAttrType;

typedef enum StunCredential {

  STUN_CREDENTIAL_SHORT_TERM = 0x0001,
  STUN_CREDENTIAL_LONG_TERM = 0x0002,

} StunCredential;

typedef enum StunFamily {

  STUN_FAMILY_IPV4 = 0x01,
  STUN_FAMILY_IPV6 = 0x02,

} StunFamily;

struct StunHeader {
  uint16_t type;
  uint16_t length;
  uint32_t magic_cookie;
  uint32_t transaction_id[3];
};

struct StunAttribute {
  uint16_t type;
  uint16_t length;
  char value[0];
};

struct StunMessage {
  StunClass stunclass;
  StunMethod stunmethod;
  uint32_t fingerprint;
  char message_integrity[20];
  char username[128];
  char realm[64];
  char nonce[64];
  Address mapped_addr;
  Address relayed_addr;
  uint8_t buf[STUN_ATTR_BUF_SIZE];
  size_t size;
};

void stun_msg_create(StunMessage* msg, uint16_t type);

int stun_set_mapped_address(char* value, uint8_t* mask, Address* addr);

void stun_get_mapped_address(char* value, uint8_t* mask, Address* addr);

void stun_msg_parse(StunMessage* msg, uint8_t* buf, size_t len);

void stun_parse_msg_buf(StunMessage* msg);

void stun_calculate_fingerprint(char* buf, size_t len, uint32_t* fingerprint);

int stun_msg_write_attr(StunMessage* msg, StunAttrType type, uint16_t length, char* value);

int stun_probe(uint8_t* buf, size_t size);

int stun_msg_is_valid(uint8_t* buf, size_t len, char* password);

int stun_msg_finish(StunMessage* msg, StunCredential credential, const char* password, size_t password_len);

#endif  // STUN_H_
