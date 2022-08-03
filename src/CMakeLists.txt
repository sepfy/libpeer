project(lib)

file(GLOB SRCS "*.c")
file(GLOB TEST_SRCS "*test.c")
file(GLOB SIGNALING_SRCS "signaling*")

list(REMOVE_ITEM SRCS ${TEST_SRCS})

if(NOT ENABLE_SIGNALING)
  list(REMOVE_ITEM SRCS ${SIGNALING_SRCS})
endif()

add_library(pear ${SRCS})

if(ENABLE_TEST)
  foreach(sourcefile ${TEST_SRCS})
    string(REPLACE ".c" "" appname ${sourcefile})
    string(REPLACE "${PROJECT_SOURCE_DIR}/" "" appname ${appname})
    set_source_files_properties(${sourcefile} PROPERTIES LANGUAGE CXX)
    add_executable(${appname} ${sourcefile})
    target_link_libraries(${appname} pear gtest ${TARGET_LIBS})
  endforeach(sourcefile ${TEST_SRCS})
endif(ENABLE_TEST)

file(GLOB HEADERS "*.h")

set_target_properties(pear PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS pear
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)
