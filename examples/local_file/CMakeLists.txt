cmake_minimum_required(VERSION 3.1)
project(local_file)

file(GLOB SRCS "*.c")

add_executable(local_file ${SRCS})

target_link_libraries(local_file pear glib-2.0 ssl crypto nice srtp2 rtp pthread)
