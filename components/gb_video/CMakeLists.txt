idf_component_register(
    SRCS "gb_video.c" "gb_video_lut.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp esp_lcd freertos
)
