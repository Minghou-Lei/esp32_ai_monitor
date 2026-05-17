// ESP-IDF 6.x renames `esp_lcd_panel_dev_config_t::color_space` to `rgb_ele_order`.
#define color_space rgb_ele_order
#include "../../managed_components/waveshare__esp_lcd_st7703/esp_lcd_st7703.c"
