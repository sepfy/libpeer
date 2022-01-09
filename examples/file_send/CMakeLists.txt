project(file_send)

file(GLOB SRCS "*.c")

add_executable(file_send ${SRCS})

target_link_libraries(file_send pear ${TARGET_LIBS})
