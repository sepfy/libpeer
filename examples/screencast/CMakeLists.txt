project(screencast)

file(GLOB SRCS "*.c")

add_executable(screencast ${SRCS})

target_link_libraries(screencast pear ${TARGET_LIBS} ${GST_LIBRARIES})
