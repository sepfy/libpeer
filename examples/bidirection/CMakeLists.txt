project(bidirection)

file(GLOB SRCS "*.c")

add_executable(bidirection ${SRCS})

target_link_libraries(bidirection pear ${TARGET_LIBS} ${GST_LIBRARIES})
