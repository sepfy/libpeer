project(examples)

include_directories(signaling/ ${CMAKE_SOURCE_DIR}/src ${GST_INCLUDE_DIRS})

if(ENABLE_EXAMPLES)
  #add_subdirectory(video_on_demand)
  add_subdirectory(surveillance)
  add_subdirectory(home_camera)
  #add_subdirectory(screencast)
  #add_subdirectory(datachannel)
endif(ENABLE_EXAMPLES)
