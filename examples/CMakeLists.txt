project(examples)

include_directories(signaling/ ${CMAKE_SOURCE_DIR}/src ${GST_INCLUDE_DIRS})

#add_subdirectory(video_on_demand)
add_subdirectory(surveillance)
add_subdirectory(signaling)
#add_subdirectory(home_camera)
#add_subdirectory(screencast)
add_subdirectory(datachannel)


