#include <string.h>

#include "gtest/gtest.h"
extern "C" 
{
#include "sdp_attribute.h"
}

TEST(sdp_attribute, sdp_attribute_append) {

  char attributes[] = "a=12345\\r\\nm=12345\\r\\n";
  sdp_attribute_t *sdp_attribute = sdp_attribute_create();
  sdp_attribute_append(sdp_attribute, (char*)"a=12345");
  sdp_attribute_append(sdp_attribute, (char*)"m=12345");
  EXPECT_STREQ(sdp_attribute->attributes, attributes);
}

TEST(sdp_attribute, sdp_attribute_get_answer) {

  char answer[] = "{\"type\": \"answer\", \"sdp\": \"a=12345\\r\\nm=12345\\r\\n\"}";
  sdp_attribute_t *sdp_attribute = sdp_attribute_create();
  sdp_attribute_append(sdp_attribute, (char*)"a=12345");
  sdp_attribute_append(sdp_attribute, (char*)"m=12345");
  EXPECT_STREQ(sdp_attribute_get_answer(sdp_attribute), answer);
}

GTEST_API_ int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
