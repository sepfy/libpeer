
file(GLOB CODES "../../../../src/*.c")

idf_component_register(
    SRCS ${CODES}
    INCLUDE_DIRS "../../../../src/"
    REQUIRES mbedtls srtp json mdns
)

add_compile_definitions(ESP32 WITHOUT_MQTT)

