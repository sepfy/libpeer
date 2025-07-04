project(lib)

file(GLOB SRCS "*.c")

file(GLOB HEADERS "peer.h" "peer_connection.h" "peer_signaling.h")

if(WIN32)
add_library(peer STATIC
  ${SRCS}
  ${HTTP_SOURCES}
  ${MQTT_SOURCES}
  ${MQTT_SERIALIZER_SOURCES}
)
else()
add_library(peer
  ${SRCS}
  ${HTTP_SOURCES}
  ${MQTT_SOURCES}
  ${MQTT_SERIALIZER_SOURCES}
)
endif()

include_directories(peer PUBLIC
  ${HTTP_INCLUDE_PUBLIC_DIRS}
  ${MQTT_INCLUDE_PUBLIC_DIRS}
)

add_dependencies(peer cjson mbedtls usrsctp srtp2)

target_link_libraries(peer ${DEP_LIBS})

set_target_properties(peer PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS peer
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)

