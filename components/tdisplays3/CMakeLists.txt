idf_component_register(
    SRCS "t_display_s3.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lvgl_port driver esp_lcd lvgl button
)

