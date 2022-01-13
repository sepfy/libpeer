project(examples)

pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4)

include_directories(${CMAKE_SOURCE_DIR}/src ${GST_INCLUDE_DIRS})

if(ENABLE_EXAMPLES)
  add_subdirectory(file_send)
  add_subdirectory(camera_send)
  add_subdirectory(screencast)
  add_subdirectory(home_camera)
  add_subdirectory(bidirection)
endif(ENABLE_EXAMPLES)
