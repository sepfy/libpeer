
SRCS = main.c h264_packetizer.c h264_parser.c

INCLUDES = -I../../libnice/agent/ -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include/ -I../../libsrtp/include/ -I../../media-server/librtp/include/ -I../../src/

LIBS = -L../../src/ -L../../libnice/build/nice/ -L../../libsrtp/cmake/ -L../../media-server/librtp/debug.linux/ -lglib-2.0 -lgio-2.0 -lnice -lgobject-2.0 -lssl -lcrypto -lsrtp2 -lrtp -lpthread -lpear

TARGET = main

$(TARGET):
	$(CC) $(INCLUDES) $(SRCS) $(LIBS) -o $@

clean:
	rm -rf $(TARGET)
