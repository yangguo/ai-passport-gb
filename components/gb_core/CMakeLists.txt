idf_component_register(
    SRCS "gb_core.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_system
)

target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_SOURCE_DIR}/third_party/peanut-gb")
