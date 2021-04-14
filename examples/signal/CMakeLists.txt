cmake_minimum_required(VERSION 3.1)
project(signal)

file(GLOB SRCS "*.c")

add_executable(signal ${SRCS})

target_link_libraries(signal pear glib-2.0 ssl crypto nice srtp2 rtp pthread event cjson)
