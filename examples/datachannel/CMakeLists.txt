project(datachannel)

file(GLOB SRCS "*.c")

add_executable(datachannel ${SRCS} ../signaling/signaling.c)

target_link_libraries(datachannel pear ${TARGET_LIBS} pthread mosquitto cjson)
