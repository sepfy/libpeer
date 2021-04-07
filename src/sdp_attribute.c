#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "sdp_attribute.h"

sdp_attribute_t* sdp_attribute_create(void) {
  sdp_attribute_t *sdp_attribute = NULL;
  sdp_attribute = (sdp_attribute_t*)malloc(sizeof(sdp_attribute_t));
  if(sdp_attribute == NULL)
    return sdp_attribute;

  memset(sdp_attribute->answer, 0, sizeof(sdp_attribute->answer));
  memset(sdp_attribute->attributes, 0, sizeof(sdp_attribute->attributes));
  return sdp_attribute;
}

void sdp_attribute_free(sdp_attribute_t *sdp_attribute) {

  if(sdp_attribute != NULL) {
    free(sdp_attribute);
  }
}

int sdp_attribute_append(sdp_attribute_t *sdp_attribute, const char *format, ...) {

  va_list argptr;
  char attribute[128] = {0};

  if(strstr(format, "=") == NULL)
    return -1;

  va_start(argptr, format);
  vsnprintf(attribute, sizeof(attribute), format, argptr);
  va_end(argptr);

  strcat(sdp_attribute->attributes, attribute);
  strcat(sdp_attribute->attributes, "\\r\\n");

  return 0;
}

char *sdp_attribute_get_answer(sdp_attribute_t *sdp_attribute) {

  snprintf(sdp_attribute->answer, sizeof(sdp_attribute->answer), 
   "{\"type\": \"answer\", \"sdp\": \"%s\"}", sdp_attribute->attributes);

  return sdp_attribute->answer;
}
