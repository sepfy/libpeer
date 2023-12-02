project(sample)

file(GLOB SRCS "*.c" "*.h")

add_executable(sample ${SRCS})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(sample PRIVATE peer::static Threads::Threads)
