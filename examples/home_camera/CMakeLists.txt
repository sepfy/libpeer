project(home_camera)

file(GLOB SRCS "*.c")

add_executable(home_camera ${SRCS})

target_link_libraries(home_camera pear ${TARGET_LIBS} ${GST_LIBRARIES})
