idf_component_register(
    SRCS
        "main.c"
        "app_registry.c"
        "board_display.c"
        "launcher_ui.c"
        "system_status.c"
    INCLUDE_DIRS "."
    REQUIRES esp32_s3_touch_lcd_1_85B
)
