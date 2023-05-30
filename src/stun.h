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

typedef enum StunMsgType {

  STUN_MSG_TYPE_INVLID,
  STUN_MSG_TYPE_UNKNOWN,
  STUN_MSG_TYPE_BINDING_REQUEST = 0x0001,
  STUN_MSG_TYPE_BINDING_RESPONSE = 0x0101,
  STUN_MSG_TYPE_BINDING_ERROR_RESPONSE = 0x0111,
  STUN_MSG_TYPE_BINDING_INDICATION = 0x0011

} StunMsgType;

#define STUN_BINDING_REQUEST 0x0001
#define STUN_BINDING_RESPONSE 0x0101
#define STUN_BINDING_ERROR_RESPONSE 0x0111
#define STUN_BINDING_INDICATION 0x0011

#define STUN_ATTRIBUTE_MAPPED_ADDRESS 0x0001
#define STUN_ATTRIBUTE_USERNAME 0x0006
#define STUN_ATTRIBUTE_MESSAGE_INTEGRITY 0x0008
#define STUN_ATTRIBUTE_XOR_MAPPED_ADDRESS 0x0020
#define STUN_ATTRIBUTE_FINGERPRINT 0x8028

typedef enum StunAttrType {

  STUN_ATTR_TYPE_MAPPED_ADDRESS = 0x0001,
  STUN_ATTR_TYPE_USERNAME = 0x0006,
  STUN_ATTR_TYPE_MESSAGE_INTEGRITY = 0x0008,
  STUN_ATTR_TYPE_XOR_MAPPED_ADDRESS = 0x0020,
  STUN_ATTR_TYPE_PRIORITY = 0x0024,
  STUN_ATTR_TYPE_USE_CANDIDATE = 0x0025,
  STUN_ATTR_TYPE_FINGERPRINT = 0x8028,
  STUN_ATTR_TYPE_ICE_CONTROLLED = 0x8029,
  STUN_ATTR_TYPE_ICE_CONTROLLING = 0x802a
} StunAttrType;

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

  char message_integrity[20];
  uint32_t fingerprint;
  char username[10];
  Address mapped_addr;
  char buf[STUN_ATTR_BUF_SIZE];
  size_t size;
};  

void stun_msg_create(StunMessage *msg, StunMsgType type);

void stun_create_binding_request(StunMessage *msg);

void stun_set_mapped_address(char *value, uint8_t *mask, Address *addr);

void stun_get_mapped_address(char *value, uint8_t *mask, Address *addr);

void stun_parse_binding_response(char *attr_buf, size_t len, Address *addr);

void stun_parse_msg_buf(StunMessage *msg);

int stun_get_local_address(const char *stun_server, int stun_port, Address *addr);

void stun_calculate_fingerprint(char *buf, size_t len, uint32_t *fingerprint);

int stun_msg_write_attr2(StunMessage *msg, StunAttrType type, uint16_t length, char *value);

int stun_msg_write_attr(StunMessage *msg, uint16_t type, uint16_t length, char *value);

StunMsgType stun_is_stun_msg(char *buf, size_t len);

int stun_response_is_valid(char *buf, size_t len, char *password);

int stun_msg_finish(StunMessage *msg, char *password);

#endif // STUN_H_
