project(lib)

file(GLOB SRCS "*.c")

file(GLOB HEADERS "*.h")

if(ENABLE_GST)
  file(GLOB GST_SRCS "gst/*.c")
endif()

add_library(pear SHARED ${SRCS} ${GST_SRCS})

target_include_directories(pear PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pear ${TARGET_LIBS})

set_target_properties(pear PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS pear
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)

