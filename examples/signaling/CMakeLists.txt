project(signaling)

file(GLOB SRCS "*.c")

add_executable(signaling ${SRCS})

target_link_libraries(signaling pear ${TARGET_LIBS} ${GST_LIBRARIES})
