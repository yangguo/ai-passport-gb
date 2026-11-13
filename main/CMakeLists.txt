idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES gb_core gb_video bsp esp_timer mbedtls
)

target_add_binary_data(${COMPONENT_LIB}
    "${CMAKE_SOURCE_DIR}/roms/homebrew/pattern.gb"
    BINARY)
