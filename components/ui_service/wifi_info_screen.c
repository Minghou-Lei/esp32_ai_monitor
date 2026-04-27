/**
 * @file    wifi_info_screen.c
 * @brief   基于 BSP + LVGL 的 Wi-Fi 详情屏幕。
 *
 * 本文件负责初始化板级显示、创建滚动详情页，并将 network_service
 * 的状态快照渲染为可读的监控终端界面。
 * 本文件不直接调用 esp_wifi 控制接口。
 */

#include "wifi_info_screen.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"
#include "network_service.h"
#include "sdkconfig.h"

static const char *TAG = "wifi_info_screen";
static const uint32_t WIFI_INFO_SCREEN_DRAW_BUFFER_LINES = 20;
static const uint32_t WIFI_INFO_SCREEN_FIRST_REFRESH_MS = 200;
static const int32_t WIFI_INFO_SCREEN_FONT_SIZE = 24;

static lv_obj_t *s_details_label;
static bool s_screen_started;
static bool s_first_refresh_completed;
static lv_font_t *s_runtime_font;
static network_service_snapshot_t s_snapshot_cache;
static char s_text_buffer[1536];

extern const uint8_t jnr_sb_font_ttf_start[] asm("_binary_jnr_sb_font_ttf_start");
extern const uint8_t jnr_sb_font_ttf_end[] asm("_binary_jnr_sb_font_ttf_end");

static lv_display_t *wifi_info_screen_start_display(void)
{
    bsp_display_cfg_t display_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * WIFI_INFO_SCREEN_DRAW_BUFFER_LINES,
        .double_buffer = false,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        },
    };

    /*
     * 720x720 DPI 面板的整帧缓冲由底层驱动放在 PSRAM。
     * 这里把 LVGL 的绘图缓冲也切到 PSRAM，并收小为 20 行，避免继续挤占片上 SRAM。
     */
    return bsp_display_start_with_config(&display_cfg);
}

/**
 * @brief 在 BSP 完成 LVGL 启动后，从嵌入式 TTF 资产构建运行时字体。
 *
 * TinyTTF 的缓存分配走 LVGL 堆；因此必须等 `bsp_display_start_with_config()`
 * 成功后再创建字体对象，否则字体缓存会绑定到未初始化的 LVGL 运行时。
 */
static void wifi_info_screen_prepare_font(void)
{
    if (s_runtime_font != NULL) {
        return;
    }

    size_t font_data_size = (size_t)(jnr_sb_font_ttf_end - jnr_sb_font_ttf_start);
    if (font_data_size == 0U) {
        ESP_LOGW(TAG, "Embedded font asset is empty");
        return;
    }

    s_runtime_font = lv_tiny_ttf_create_data(jnr_sb_font_ttf_start,
                                             font_data_size,
                                             WIFI_INFO_SCREEN_FONT_SIZE);
    if (s_runtime_font == NULL) {
        ESP_LOGW(TAG, "Failed to create TinyTTF font from embedded asset");
        return;
    }

    ESP_LOGI(TAG,
             "Loaded TinyTTF font asset (%u bytes, %ld px)",
             (unsigned int)font_data_size,
             (long)WIFI_INFO_SCREEN_FONT_SIZE);
}

/**
 * @brief 逐段拼接状态文本，避免单次巨型格式化把 `main` / LVGL 上下文栈压得过深。
 *
 * `ESP32-P4` 这条启动路径里，首帧 UI 刷新可能仍与 `main` 任务时序重叠；
 * 如果把二十多个参数塞进一次 `snprintf`，`newlib` 的 `vfprintf` 很容易触发栈保护。
 */
static void wifi_info_screen_appendf(size_t *offset, const char *format, ...)
{
    if ((offset == NULL) || (*offset >= sizeof(s_text_buffer))) {
        return;
    }

    va_list args;
    va_start(args, format);
    int written = vsnprintf(&s_text_buffer[*offset], sizeof(s_text_buffer) - *offset, format, args);
    va_end(args);

    if (written <= 0) {
        return;
    }

    size_t remaining = sizeof(s_text_buffer) - *offset;
    if ((size_t)written >= remaining) {
        *offset = sizeof(s_text_buffer) - 1;
        return;
    }

    *offset += (size_t)written;
}

static void wifi_info_screen_refresh(lv_timer_t *timer)
{
    if (s_details_label == NULL) {
        return;
    }

    network_service_get_snapshot(&s_snapshot_cache);

    s_text_buffer[0] = '\0';
    size_t offset = 0;
    wifi_info_screen_appendf(&offset, "State: %s\n", s_snapshot_cache.state_text);
    wifi_info_screen_appendf(&offset, "Status: %s\n", s_snapshot_cache.status_text);
    wifi_info_screen_appendf(&offset, "Credential source: menuconfig\n");
    wifi_info_screen_appendf(&offset, "Configured SSID: %s\n", s_snapshot_cache.configured_ssid);
    wifi_info_screen_appendf(&offset,
                             "Password set: %s\n",
                             s_snapshot_cache.password_configured ? "yes" : "no");
    wifi_info_screen_appendf(&offset,
                             "Password length: %" PRIu8 "\n",
                             s_snapshot_cache.configured_password_length);
    wifi_info_screen_appendf(&offset, "Associated SSID: %s\n", s_snapshot_cache.connected_ssid);
    wifi_info_screen_appendf(&offset, "Hostname: %s\n", s_snapshot_cache.hostname);
    wifi_info_screen_appendf(&offset, "Station MAC: %s\n", s_snapshot_cache.sta_mac);
    wifi_info_screen_appendf(&offset, "BSSID: %s\n", s_snapshot_cache.bssid);
    wifi_info_screen_appendf(&offset, "IPv4: %s\n", s_snapshot_cache.ip);
    wifi_info_screen_appendf(&offset, "Netmask: %s\n", s_snapshot_cache.netmask);
    wifi_info_screen_appendf(&offset, "Gateway: %s\n", s_snapshot_cache.gateway);
    wifi_info_screen_appendf(&offset, "DNS Main: %s\n", s_snapshot_cache.dns_main);
    wifi_info_screen_appendf(&offset, "DNS Backup: %s\n", s_snapshot_cache.dns_backup);
    wifi_info_screen_appendf(&offset, "RSSI: %d dBm\n", s_snapshot_cache.rssi);
    wifi_info_screen_appendf(&offset,
                             "Primary channel: %" PRIu8 "\n",
                             s_snapshot_cache.primary_channel);
    wifi_info_screen_appendf(&offset, "Second channel: %s\n", s_snapshot_cache.second_channel);
    wifi_info_screen_appendf(&offset, "Auth mode: %s\n", s_snapshot_cache.auth_mode);
    wifi_info_screen_appendf(&offset, "Pairwise cipher: %s\n", s_snapshot_cache.pairwise_cipher);
    wifi_info_screen_appendf(&offset, "Group cipher: %s\n", s_snapshot_cache.group_cipher);
    wifi_info_screen_appendf(&offset,
                             "Reconnect attempts: %" PRIu32 "\n",
                             s_snapshot_cache.reconnect_attempts);
    wifi_info_screen_appendf(&offset,
                             "Last disconnect reason: %u",
                             s_snapshot_cache.last_disconnect_reason);

    lv_label_set_text(s_details_label, s_text_buffer);

    if ((timer != NULL) && !s_first_refresh_completed) {
        s_first_refresh_completed = true;
        lv_timer_set_period(timer, CONFIG_AI_MONITOR_UI_REFRESH_MS);
    }
}

/**
 * @brief 该回调运行在 LVGL 定时器上下文。
 *
 * 这里直接更新标签文本是安全的；如果把 UI 更新放到 Wi-Fi 事件回调里，
 * 会跨线程触碰 LVGL，破坏 BSP 提供的互斥约束。
 */
static void wifi_info_screen_create_layout(void)
{
    wifi_info_screen_prepare_font();

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF3F5F7), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Wi-Fi Details");
    lv_obj_set_style_text_color(title, lv_color_hex(0x0F172A), 0);
    if (s_runtime_font != NULL) {
        lv_obj_set_style_text_font(title, s_runtime_font, 0);
    }
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 28, 20);

    lv_obj_t *subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "ESP32-P4 monitor terminal");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x475569), 0);
    if (s_runtime_font != NULL) {
        lv_obj_set_style_text_font(subtitle, s_runtime_font, 0);
    }
    lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 672, 612);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_set_style_radius(panel, 20, 0);
    lv_obj_set_style_pad_all(panel, 18, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xD7DEE8), 0);
    lv_obj_set_style_shadow_width(panel, 12, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_20, 0);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_ACTIVE);

    s_details_label = lv_label_create(panel);
    lv_obj_set_width(s_details_label, 620);
    lv_label_set_long_mode(s_details_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_details_label, lv_color_hex(0x111827), 0);
    if (s_runtime_font != NULL) {
        lv_obj_set_style_text_font(s_details_label, s_runtime_font, 0);
    }
    lv_label_set_text(s_details_label, "Wi-Fi details loading...");

    lv_timer_create(wifi_info_screen_refresh, WIFI_INFO_SCREEN_FIRST_REFRESH_MS, NULL);
}

esp_err_t wifi_info_screen_start(void)
{
    if (s_screen_started) {
        return ESP_OK;
    }

#if !CONFIG_SPIRAM
    ESP_LOGE(TAG, "PSRAM is disabled. Enable it in SDK Configuration editor before starting the display.");
    return ESP_ERR_NOT_SUPPORTED;
#endif

    lv_display_t *display = wifi_info_screen_start_display();
    if (display == NULL) {
        ESP_LOGE(TAG, "bsp_display_start_with_config() failed");
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(bsp_display_backlight_on(), TAG, "failed to enable display backlight");

    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Failed to lock LVGL mutex");
        return ESP_ERR_TIMEOUT;
    }

    wifi_info_screen_create_layout();
    bsp_display_unlock();

    s_screen_started = true;
    ESP_LOGI(TAG, "Wi-Fi detail screen started on display=%p", (void *)display);
    return ESP_OK;
}
