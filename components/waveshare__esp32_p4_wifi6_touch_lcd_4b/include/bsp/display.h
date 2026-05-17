#pragma once

#include "../../../../managed_components/waveshare__esp32_p4_wifi6_touch_lcd_4b/include/bsp/display.h"

// ESP-IDF 6.x replaces the old LCD color-format constants with lcd_color_format_t values.
#undef ESP_LCD_COLOR_FORMAT_RGB565
#undef ESP_LCD_COLOR_FORMAT_RGB888
#define ESP_LCD_COLOR_FORMAT_RGB565 LCD_COLOR_FMT_RGB565
#define ESP_LCD_COLOR_FORMAT_RGB888 LCD_COLOR_FMT_RGB888

#undef BSP_LCD_COLOR_FORMAT
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
#define BSP_LCD_COLOR_FORMAT   ESP_LCD_COLOR_FORMAT_RGB888
#define BSP_LCD_BITS_PER_PIXEL (24)
#else
#define BSP_LCD_COLOR_FORMAT   ESP_LCD_COLOR_FORMAT_RGB565
#define BSP_LCD_BITS_PER_PIXEL (16)
#endif

// ESP-IDF 6.x uses rgb element order instead of the deprecated color-space enum.
#undef BSP_LCD_COLOR_SPACE
#define BSP_LCD_COLOR_SPACE LCD_RGB_ELEMENT_ORDER_RGB

// Keep the old macro names used by the BSP source, but map them to SDK 6.x values.
#ifndef LCD_COLOR_PIXEL_FORMAT_RGB565
#define LCD_COLOR_PIXEL_FORMAT_RGB565 LCD_COLOR_FMT_RGB565
#endif

#ifndef LCD_COLOR_PIXEL_FORMAT_RGB888
#define LCD_COLOR_PIXEL_FORMAT_RGB888 LCD_COLOR_FMT_RGB888
#endif
