project(lib)

file(GLOB_RECURSE SRCS "*.c" "*.h")

file(GLOB HEADERS "peer.h" "peer_connection.h" "peer_signaling.h")

add_library(peer SHARED ${SRCS})
add_library(peer-static STATIC ${SRCS})

target_link_libraries(peer PRIVATE ${DEP_LIBS})
target_link_libraries(peer-static PRIVATE ${DEP_LIBS})
if(WIN32)
    target_link_libraries(peer PUBLIC WS2_32 Iphlpapi)
    target_link_libraries(peer-static PUBLIC WS2_32 Iphlpapi)
endif()

target_include_directories(peer INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(peer-static INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(peer PROPERTIES PUBLIC_HEADER "${HEADERS}")
set_target_properties(peer-static PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS peer
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)

install(TARGETS peer-static
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)
