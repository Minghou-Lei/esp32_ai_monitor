#pragma once

#include "../../../managed_components/waveshare__esp_lcd_st7703/include/esp_lcd_st7703.h"

// ESP-IDF 6.x replaces `pixel_format` with explicit in/out color formats.
#undef ST7703_720_720_PANEL_60HZ_DPI_CONFIG
#define ST7703_720_720_PANEL_60HZ_DPI_CONFIG(px_format) \
    {                                                   \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,    \
        .dpi_clock_freq_mhz = 38,                       \
        .virtual_channel = 0,                           \
        .in_color_format = (px_format),                 \
        .out_color_format = (px_format),                \
        .num_fbs = 1,                                   \
        .video_timing = {                               \
            .h_size = 720,                              \
            .v_size = 720,                              \
            .hsync_back_porch = 50,                     \
            .hsync_pulse_width = 20,                    \
            .hsync_front_porch = 50,                    \
            .vsync_back_porch = 20,                     \
            .vsync_pulse_width = 4,                     \
            .vsync_front_porch = 20,                    \
        },                                              \
    }
