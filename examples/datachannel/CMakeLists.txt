project(datachannel)

file(GLOB SRCS "*.c")

add_executable(datachannel ${SRCS})

target_link_libraries(datachannel pear ${TARGET_LIBS} pthread)
