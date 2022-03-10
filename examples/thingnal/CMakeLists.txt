project(thingnal)

file(GLOB SRCS "*.c")

add_executable(thingnal ${SRCS})

target_link_libraries(thingnal pear ${TARGET_LIBS} ${GST_LIBRARIES})
