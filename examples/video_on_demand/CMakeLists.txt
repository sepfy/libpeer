project(video_on_demand)

file(GLOB SRCS "*.c")

add_executable(video_on_demand ${SRCS})

target_link_libraries(video_on_demand pear ${EXAMPLE_LIBS})
