#include <gtest/gtest.h>

#include "peer_connection.h"

TEST(media_stream, media_stream_create) {

  MediaStream *media_stream = media_stream_create(CODEC_H264, "videotestsrc ! v4l2h264enc capture-io-mode=4 output-io-mode=4", NULL);
  media_stream_destroy(media_stream);
}

GTEST_API_ int main(int argc, char *argv[]) {

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
