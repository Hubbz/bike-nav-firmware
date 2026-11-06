idf_component_register(
    SRCS
        "main.c"
        "app_registry.c"
        "apps/alert/alert_app.c"
        "apps/security/security_app.c"
        "board_display.c"
        "launcher_ui.c"
        "system_status.c"
    INCLUDE_DIRS "."
    REQUIRES esp32_s3_touch_lcd_1_85B bsp_extra
)
