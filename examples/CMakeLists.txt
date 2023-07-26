project(example)

file(GLOB SRCS "*.c")

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/signaling)

add_executable(example ${SRCS})

target_link_libraries(example pear ${TARGET_LIBS})

