cmake_minimum_required(VERSION 3.1)
project(file_send)

file(GLOB SRCS "*.c")

add_executable(file_send ${SRCS})

target_link_libraries(file_send pear glib-2.0 ssl crypto nice srtp2 rtp pthread)
