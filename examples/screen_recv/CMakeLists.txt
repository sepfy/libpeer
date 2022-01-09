project(screen_recv)

file(GLOB SRCS "*.c")

add_executable(screen_recv ${SRCS})

target_link_libraries(screen_recv pear ${TARGET_LIBS} ${GST_LIBRARIES})
