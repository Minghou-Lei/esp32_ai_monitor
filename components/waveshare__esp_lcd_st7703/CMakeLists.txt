# 项目级 override：复用 registry 里的 ST7703 驱动源码，
# 先补齐 ESP-IDF 6.x 对 `driver/gpio.h` 的新组件依赖声明。
set(WAVESHARE_ST7703_UPSTREAM_DIR "${PROJECT_DIR}/managed_components/waveshare__esp_lcd_st7703")

idf_component_register(
    SRCS "esp_lcd_st7703.c"
    INCLUDE_DIRS "include" "${WAVESHARE_ST7703_UPSTREAM_DIR}/include"
    REQUIRES esp_lcd
    PRIV_REQUIRES esp_driver_gpio)

include(package_manager)
cu_pkg_define_version(${WAVESHARE_ST7703_UPSTREAM_DIR})
