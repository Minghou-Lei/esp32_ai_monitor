idf_component_register(SRCS "wifi_info_screen.c"
                    INCLUDE_DIRS "include"
                    REQUIRES network_service waveshare__esp32_p4_wifi6_touch_lcd_4b)

target_add_binary_data(${COMPONENT_LIB} "assets/jnr_sb_font.ttf" BINARY
                       RENAME_TO jnr_sb_font_ttf)
