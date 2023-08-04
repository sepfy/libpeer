idf_component_register(SRCS
  "app_main.c" "audio.c" "g711.c" "camera.c" "wifi.c" "mqtt.c" 
  INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
