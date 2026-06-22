project(tests)

file(GLOB SRCS "*.c")

include_directories(${PROJECT_SOURCE_DIR}/../src)

foreach(sourcefile ${SRCS})
  string(REPLACE ".c" "" appname ${sourcefile})
  string(REPLACE "${PROJECT_SOURCE_DIR}/" "" appname ${appname})
  add_executable(${appname} ${sourcefile})
  target_link_libraries(${appname} peer)
endforeach(sourcefile ${TEST_SRCS})

target_link_libraries(test_peer_connection cjson)
if(NOT WIN32)
  target_link_libraries(test_peer_connection pthread)
endif()
