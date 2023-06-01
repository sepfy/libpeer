project(signaling)

file(GLOB SRCS "*.c")

add_executable(signaling_test ${SRCS})

target_link_libraries(signaling_test mosquitto pthread cjson pear)



