#include "gtest/gtest.h"

extern "C" {
#include "session_description.h"
}

TEST(SessionDescription, session_description_create) {

  SessionDescription *sdp = session_description_create(NULL);
  if(sdp)
    session_description_destroy(sdp);
}

TEST(session_description, session_description_append) {

  char test_sdp[] = "a=12345\\r\\nm=12345\\r\\n";
  SessionDescription *sdp = session_description_create(NULL);
  session_description_append(sdp, (char*)"a=12345");
  session_description_append(sdp, (char*)"m=12345");

  EXPECT_STREQ(session_description_get_content(sdp), test_sdp);

  if(sdp)
    session_description_destroy(sdp);
}

GTEST_API_ int main(int argc, char *argv[]) {

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
