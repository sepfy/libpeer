project(camera_send)

file(GLOB SRCS "*.c")

add_executable(camera_send ${SRCS})

target_link_libraries(camera_send pear ${TARGET_LIBS} ${GST_LIBRARIES})
