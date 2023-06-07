project(examples)

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/signaling ${GST_INCLUDE_DIRS})

set(EXAMPLE_LIBS pear ${TARGET_LIBS})

#add_subdirectory(screencast)
#add_subdirectory(video_on_demand)
add_subdirectory(surveillance)
add_subdirectory(home_camera)
add_subdirectory(datachannel)

