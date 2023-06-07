project(home_camera)

file(GLOB SRCS "*.c")

add_executable(home_camera ${SRCS} ../signaling/signaling.c)

target_link_libraries(home_camera pear ${TARGET_LIBS} ${GST_LIBRARIES} pthread mosquitto cjson)



