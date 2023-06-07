project(surveillance)

file(GLOB SRCS "*.c")

add_executable(surveillance ${SRCS})

target_link_libraries(surveillance ${EXAMPLE_LIBS})



