#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "stun.h"

#include "peer_connection.h"
#define STUN_ADDR "142.250.21.127"
#define STUN_PORT 19302

int main(int argc, char *argv[]) {
#if 0
  PeerConnection pc;

  const char *sdp_text;

  peer_connection_init(&pc);

  cJSON *offer = cJSON_CreateObject();

  char remote_description[AGENT_MAX_DESCRIPTION];
  char local_description[AGENT_MAX_DESCRIPTION];
  char offer_base64[AGENT_MAX_DESCRIPTION];
  char answer_base64[AGENT_MAX_DESCRIPTION];

  memset(remote_description, 0, sizeof(remote_description));
  memset(local_description, 0, sizeof(local_description));
  memset(offer_base64, 0, sizeof(offer_base64));

  peer_connection_create_offer(&pc);
#if 0
  printf("sdp: \n%s\n", sdp_text);

  // create offer
  cJSON_AddStringToObject(offer, "type", "offer");
  cJSON_AddStringToObject(offer, "sdp", sdp_text);

  char *offer_str = cJSON_Print(offer);

  printf("Offer: \n%s\n", offer_str);

  base64_encode(offer_str, strlen(offer_str), offer_base64, sizeof(offer_base64));

  printf("Offer base64: \n%s\n", offer_base64);

  free(offer_str);
  cJSON_Delete(offer);

  printf("Enter base64 answer\n");
  scanf("%s", answer_base64);

  char answer_str[AGENT_MAX_DESCRIPTION];

  base64_decode(answer_base64, strlen(answer_base64), answer_str, sizeof(answer_str));

  cJSON *answer = cJSON_Parse(answer_str);

  char *answer_sdp = cJSON_GetObjectItem(answer, "sdp")->valuestring;

  printf("Answer SDP: \n%s\n", answer_sdp);

  peer_connection_set_remote_description(&pc, answer_sdp);
  cJSON_Delete(answer);
#endif
  while (1) {
    usleep(100*1000);
  }
#endif
}
