# 项目级 override：复用 registry 里的 Waveshare BSP 源码，
# 仅补齐 ESP-IDF 6.x 新拆分 `esp_driver_*` 组件的依赖声明。
set(WAVESHARE_BSP_UPSTREAM_DIR "${PROJECT_DIR}/managed_components/waveshare__esp32_p4_wifi6_touch_lcd_4b")

idf_component_register(
    SRCS "${WAVESHARE_BSP_UPSTREAM_DIR}/esp32_p4_wifi6_touch_lcd_4b.c"
    INCLUDE_DIRS "include" "${WAVESHARE_BSP_UPSTREAM_DIR}/include"
    PRIV_INCLUDE_DIRS "${WAVESHARE_BSP_UPSTREAM_DIR}/priv_include"
    REQUIRES driver esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_sdmmc
    PRIV_REQUIRES esp_driver_ledc esp_lcd usb spiffs fatfs)
