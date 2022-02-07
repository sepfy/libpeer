project(meeting)

file(GLOB SRCS "*.c")

add_executable(meeting ${SRCS})

target_link_libraries(meeting pear ${TARGET_LIBS} ${GST_LIBRARIES})
