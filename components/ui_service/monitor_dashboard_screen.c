/**
 * @file    monitor_dashboard_screen.c
 * @brief   基于 BSP + LVGL 的 AQI 主监控屏幕。
 *
 * 这是板上默认主视图，恢复为卡片式 AQI 大盘：主余额、可用百分比、DELTA
 * 与小时消费金额，以及底部状态区。配置页仍然通过网页提供，不再替代板上主界面。
 */

#include "wifi_info_screen.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "network_service.h"
#include "provider_service.h"
#include "sdkconfig.h"

static const char *TAG = "wifi_info_screen";
static const uint32_t WIFI_INFO_SCREEN_DRAW_BUFFER_LINES = 20;
static const uint32_t WIFI_INFO_SCREEN_FIRST_REFRESH_MS = 200;
static const int32_t WIFI_INFO_SCREEN_FONT_SIZE_SMALL = 20;
static const int32_t WIFI_INFO_SCREEN_FONT_SIZE_MEDIUM = 28;
static const int32_t WIFI_INFO_SCREEN_FONT_SIZE_BALANCE = 58;
static const int32_t WIFI_INFO_SCREEN_FONT_SIZE_DELTA = 34;
static const uint32_t WIFI_INFO_SCREEN_COLOR_BLACK = 0x000000;
static const uint32_t WIFI_INFO_SCREEN_COLOR_WHITE = 0xFFFFFF;
static const uint32_t WIFI_INFO_SCREEN_COLOR_BORDER = 0xF5F5F5;
static const uint32_t WIFI_INFO_SCREEN_COLOR_AMBER = 0x7A4313;
static const uint32_t WIFI_INFO_SCREEN_COLOR_BLUE = 0x0E2A63;
static const uint32_t WIFI_INFO_SCREEN_COLOR_GREEN = 0x2F5E3B;
static const uint32_t WIFI_INFO_SCREEN_COLOR_RED = 0x742A25;
static const uint32_t WIFI_INFO_SCREEN_COLOR_DIM = 0xD1D5DB;
static const uint32_t WIFI_INFO_SCREEN_COLOR_PANEL_BG = 0x050505;
static const uint32_t WIFI_INFO_SCREEN_COLOR_AQI = 0x1E4D35;
static const uint32_t WIFI_INFO_SCREEN_COLOR_DELTA = 0x0E2A63;
static const uint32_t WIFI_INFO_SCREEN_COLOR_NEUTRAL = 0x4B5563;

typedef struct {
    lv_obj_t *badge_panel;
    lv_obj_t *badge_label;
    lv_obj_t *badge_subtitle;
    lv_obj_t *main_panel;
    lv_obj_t *main_label;
    lv_obj_t *strip_panel;
    lv_obj_t *strip_fill_panel;
    lv_obj_t *strip_label;
    lv_obj_t *numeric_panel;
    lv_obj_t *numeric_fill_panel;
    lv_obj_t *numeric_value_label;
    lv_obj_t *numeric_caption_label;
    uint32_t badge_color_cache;
    uint32_t strip_color_cache;
} wifi_info_screen_board_row_t;

typedef struct {
    const char *badge_text_jp;
    uint32_t badge_color;
} wifi_info_screen_state_visual_t;

static wifi_info_screen_board_row_t s_primary_row;
static wifi_info_screen_board_row_t s_secondary_row;
static lv_obj_t *s_details_panel;
static lv_obj_t *s_details_label;
static bool s_details_diagnostic_mode;
static bool s_screen_started;
static bool s_first_refresh_completed;
static bool s_fonts_ready;
static lv_font_t *s_font_small;
static lv_font_t *s_font_medium;
static lv_font_t *s_font_large;
static lv_font_t *s_font_numeric;
static lv_font_t *s_font_balance;
static lv_font_t *s_font_delta;
static network_service_snapshot_t s_snapshot_cache;
static provider_service_snapshot_t s_provider_snapshot_cache;
static char s_text_buffer[1536];
static uint32_t s_delta_flash_ticks_remaining;
static uint32_t s_delta_flash_toggle_count;
static int64_t s_last_delta_flash_marker;

extern const uint8_t jnr_sb_font_ttf_start[] asm("_binary_jnr_sb_font_ttf_start");
extern const uint8_t jnr_sb_font_ttf_end[] asm("_binary_jnr_sb_font_ttf_end");

static const char *wifi_info_screen_get_primary_text(const network_service_snapshot_t *snapshot);
static const char *wifi_info_screen_get_secondary_text(const network_service_snapshot_t *snapshot);
static void wifi_info_screen_appendf(size_t *offset, const char *format, ...);
static void wifi_info_screen_build_details_text(void);
static const char *wifi_info_screen_get_countdown_text(int64_t end_time_unix_seconds);
static uint32_t wifi_info_screen_get_progress_color(uint32_t progress);

static const char *wifi_info_screen_get_countdown_text(int64_t end_time_unix_seconds)
{
    static char buffer[24];
    if (end_time_unix_seconds <= 0) {
        snprintf(buffer, sizeof(buffer), "T-00:00:00");
        return buffer;
    }

    int64_t now = s_provider_snapshot_cache.last_fetch_unix_seconds;
    if ((now <= 0) || (s_provider_snapshot_cache.last_fetch_monotonic_us <= 0)) {
        snprintf(buffer, sizeof(buffer), "T-00:00:00");
        return buffer;
    }

    int64_t elapsed_us = esp_timer_get_time() - s_provider_snapshot_cache.last_fetch_monotonic_us;
    if (elapsed_us > 0) {
        now += elapsed_us / 1000000LL;
    }

    int64_t remaining = end_time_unix_seconds - now;
    if (remaining < 0) {
        remaining = 0;
    }

    int64_t hours = remaining / 3600;
    int64_t minutes = (remaining % 3600) / 60;
    int64_t seconds = remaining % 60;
    uint32_t display_hours = (uint32_t)(hours % 100LL);
    snprintf(buffer,
             sizeof(buffer),
             "T-%02" PRIu32 ":%02lld:%02lld",
             display_hours,
             minutes,
             seconds);
    return buffer;
}

static void wifi_info_screen_log_heap_snapshot(const char *phase)
{
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG,
             "Heap[%s] internal free=%u largest=%u, spiram free=%u largest=%u",
             phase,
             (unsigned)internal_free,
             (unsigned)internal_largest,
             (unsigned)spiram_free,
             (unsigned)spiram_largest);
}

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

    return bsp_display_start_with_config(&display_cfg);
}

static void wifi_info_screen_prepare_fonts(void)
{
    if (s_fonts_ready) {
        return;
    }

    size_t font_data_size = (size_t)(jnr_sb_font_ttf_end - jnr_sb_font_ttf_start);
    if (font_data_size == 0U) {
        ESP_LOGW(TAG, "Embedded font asset is empty");
        s_fonts_ready = true;
        return;
    }

    wifi_info_screen_log_heap_snapshot("before-fonts");

    s_font_small = lv_tiny_ttf_create_data(jnr_sb_font_ttf_start,
                                           font_data_size,
                                           WIFI_INFO_SCREEN_FONT_SIZE_SMALL);
    s_font_medium = lv_tiny_ttf_create_data(jnr_sb_font_ttf_start,
                                            font_data_size,
                                            WIFI_INFO_SCREEN_FONT_SIZE_MEDIUM);
    s_font_balance = lv_tiny_ttf_create_data(jnr_sb_font_ttf_start,
                                             font_data_size,
                                             WIFI_INFO_SCREEN_FONT_SIZE_BALANCE);
    s_font_delta = lv_tiny_ttf_create_data(jnr_sb_font_ttf_start,
                                           font_data_size,
                                           WIFI_INFO_SCREEN_FONT_SIZE_DELTA);

    s_font_large = s_font_medium;
    s_font_numeric = s_font_medium;
    if (s_font_balance == NULL) {
        s_font_balance = s_font_medium;
    }
    if (s_font_delta == NULL) {
        s_font_delta = s_font_medium;
    }

    if ((s_font_small == NULL) || (s_font_medium == NULL)) {
        ESP_LOGW(TAG, "Failed to create TinyTTF base fonts for station board layout");
    } else {
        ESP_LOGI(TAG,
                 "Loaded TinyTTF station fonts (%u bytes, %ld/%ld px, balance=%ld delta=%ld)",
                 (unsigned int)font_data_size,
                 (long)WIFI_INFO_SCREEN_FONT_SIZE_SMALL,
                 (long)WIFI_INFO_SCREEN_FONT_SIZE_MEDIUM,
                 (long)WIFI_INFO_SCREEN_FONT_SIZE_BALANCE,
                 (long)WIFI_INFO_SCREEN_FONT_SIZE_DELTA);
    }

    wifi_info_screen_log_heap_snapshot("after-fonts");
    s_fonts_ready = true;
}

static lv_obj_t *wifi_info_screen_create_panel(lv_obj_t *parent,
                                               int32_t width,
                                               int32_t height,
                                               uint32_t bg_color,
                                               uint32_t border_color)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(bg_color), 0);
    lv_obj_set_style_border_width(panel, 4, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(border_color), 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static lv_obj_t *wifi_info_screen_create_label(lv_obj_t *parent, lv_font_t *font, uint32_t text_color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_color(label, lv_color_hex(text_color), 0);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    return label;
}

static void wifi_info_screen_set_panel_color_if_changed(lv_obj_t *panel,
                                                        uint32_t *color_cache,
                                                        uint32_t next_color)
{
    if ((panel == NULL) || (color_cache == NULL) || (*color_cache == next_color)) {
        return;
    }

    lv_obj_set_style_bg_color(panel, lv_color_hex(next_color), 0);
    *color_cache = next_color;
}

static void wifi_info_screen_set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if ((label == NULL) || (text == NULL)) {
        return;
    }

    const char *current_text = lv_label_get_text(label);
    if ((current_text != NULL) && (strcmp(current_text, text) == 0)) {
        return;
    }

    lv_label_set_text(label, text);
}

static void wifi_info_screen_create_board_row(wifi_info_screen_board_row_t *row,
                                              lv_obj_t *parent,
                                              int32_t y,
                                              int32_t height,
                                              int32_t strip_height,
                                              lv_font_t *main_font,
                                              lv_font_t *numeric_font)
{
    int32_t main_label_top = (height >= 240) ? 22 : ((height >= 180) ? 14 : 8);
    row->badge_color_cache = UINT32_MAX;
    row->strip_color_cache = UINT32_MAX;

    row->badge_panel = wifi_info_screen_create_panel(parent,
                                                     160,
                                                     height,
                                                     WIFI_INFO_SCREEN_COLOR_AMBER,
                                                     WIFI_INFO_SCREEN_COLOR_BORDER);
    lv_obj_align(row->badge_panel, LV_ALIGN_TOP_LEFT, 24, y);

    row->badge_label = wifi_info_screen_create_label(row->badge_panel,
                                                     s_font_large,
                                                     WIFI_INFO_SCREEN_COLOR_WHITE);
    lv_obj_set_width(row->badge_label, 136);
    lv_label_set_long_mode(row->badge_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(row->badge_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(row->badge_label, -10, 0);
    lv_obj_align(row->badge_label, LV_ALIGN_CENTER, 0, -18);

    row->badge_subtitle = wifi_info_screen_create_label(row->badge_panel,
                                                        s_font_small,
                                                        WIFI_INFO_SCREEN_COLOR_WHITE);
    lv_obj_set_width(row->badge_subtitle, 136);
    lv_obj_set_style_text_align(row->badge_subtitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(row->badge_subtitle, LV_ALIGN_BOTTOM_MID, 0, -14);

    row->main_panel = wifi_info_screen_create_panel(parent,
                                                    360,
                                                    height,
                                                    WIFI_INFO_SCREEN_COLOR_PANEL_BG,
                                                    WIFI_INFO_SCREEN_COLOR_BORDER);
    lv_obj_align(row->main_panel, LV_ALIGN_TOP_LEFT, 196, y);

    row->main_label = wifi_info_screen_create_label(row->main_panel,
                                                    main_font,
                                                    WIFI_INFO_SCREEN_COLOR_WHITE);
    lv_obj_set_width(row->main_label, 332);
    lv_label_set_long_mode(row->main_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(row->main_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(row->main_label, -6, 0);
    lv_obj_align(row->main_label, LV_ALIGN_TOP_MID, 0, main_label_top);

    row->strip_panel = wifi_info_screen_create_panel(row->main_panel,
                                                     332,
                                                     strip_height,
                                                     WIFI_INFO_SCREEN_COLOR_PANEL_BG,
                                                     WIFI_INFO_SCREEN_COLOR_BORDER);
    lv_obj_align(row->strip_panel, LV_ALIGN_BOTTOM_MID, 0, -12);

    row->strip_fill_panel = wifi_info_screen_create_panel(row->strip_panel,
                                                          0,
                                                          strip_height - 8,
                                                          WIFI_INFO_SCREEN_COLOR_BLUE,
                                                          WIFI_INFO_SCREEN_COLOR_BLUE);
    lv_obj_set_style_border_width(row->strip_fill_panel, 0, 0);
    lv_obj_set_style_radius(row->strip_fill_panel, 6, 0);
    lv_obj_align(row->strip_fill_panel, LV_ALIGN_LEFT_MID, 0, 0);

    row->strip_label = wifi_info_screen_create_label(row->strip_panel,
                                                     s_font_medium,
                                                     WIFI_INFO_SCREEN_COLOR_WHITE);
    lv_obj_set_width(row->strip_label, 304);
    lv_label_set_long_mode(row->strip_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(row->strip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(row->strip_label, LV_ALIGN_CENTER, 0, 0);

    row->numeric_panel = wifi_info_screen_create_panel(parent,
                                                       128,
                                                       height,
                                                       WIFI_INFO_SCREEN_COLOR_PANEL_BG,
                                                       WIFI_INFO_SCREEN_COLOR_BORDER);
    lv_obj_align(row->numeric_panel, LV_ALIGN_TOP_LEFT, 568, y);

    row->numeric_fill_panel = wifi_info_screen_create_panel(row->numeric_panel,
                                                            120,
                                                            0,
                                                            WIFI_INFO_SCREEN_COLOR_BLUE,
                                                            WIFI_INFO_SCREEN_COLOR_BLUE);
    lv_obj_set_style_border_width(row->numeric_fill_panel, 0, 0);
    lv_obj_set_style_radius(row->numeric_fill_panel, 6, 0);
    lv_obj_align(row->numeric_fill_panel, LV_ALIGN_BOTTOM_MID, 0, -4);

    row->numeric_caption_label = wifi_info_screen_create_label(row->numeric_panel,
                                                               s_font_small,
                                                               WIFI_INFO_SCREEN_COLOR_WHITE);
    lv_obj_set_width(row->numeric_caption_label, 100);
    lv_obj_set_style_text_align(row->numeric_caption_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(row->numeric_caption_label, LV_ALIGN_TOP_MID, 0, 16);

    row->numeric_value_label = wifi_info_screen_create_label(row->numeric_panel,
                                                             numeric_font,
                                                             WIFI_INFO_SCREEN_COLOR_WHITE);
    lv_obj_set_width(row->numeric_value_label, 112);
    lv_obj_set_style_text_align(row->numeric_value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(row->numeric_value_label, LV_ALIGN_CENTER, 0, 14);
}

static bool wifi_info_screen_has_value(const char *text)
{
    return (text != NULL) && (text[0] != '\0') && (strcmp(text, "-") != 0);
}

static bool wifi_info_screen_has_subscription(const provider_service_item_t *item)
{
    return (item != NULL) && item->valid;
}

static const char *wifi_info_screen_get_short_end_time(const char *full_time)
{
    if ((full_time == NULL) || (strlen(full_time) < 16U)) {
        return "-";
    }

    return full_time + 5;
}

static uint32_t wifi_info_screen_get_aqi_badge_color(const provider_service_snapshot_t *snapshot,
                                                     const provider_service_item_t *item)
{
    if ((snapshot == NULL) || (item == NULL) || !item->valid) {
        return WIFI_INFO_SCREEN_COLOR_NEUTRAL;
    }

    if (snapshot->state == PROVIDER_SERVICE_STATE_ERROR) {
        return WIFI_INFO_SCREEN_COLOR_RED;
    }
    if ((item->used_percent >= 90U) || (strcmp(item->remaining_amount, "$0.00") == 0)) {
        return WIFI_INFO_SCREEN_COLOR_RED;
    }
    if (item->used_percent >= 70U) {
        return WIFI_INFO_SCREEN_COLOR_AMBER;
    }
    return WIFI_INFO_SCREEN_COLOR_GREEN;
}

static const char *wifi_info_screen_get_wifi_summary_state(const network_service_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return "WIFI ?";
    }

    switch (snapshot->state) {
    case NETWORK_SERVICE_STATE_CONNECTED:
        return "WIFI READY";
    case NETWORK_SERVICE_STATE_CONNECTING:
        return "WIFI CONNECTING";
    case NETWORK_SERVICE_STATE_PORTAL_REQUIRED:
        return "WIFI PORTAL";
    case NETWORK_SERVICE_STATE_CONFIG_AP:
        return "CONFIG AP";
    case NETWORK_SERVICE_STATE_DISCONNECTED:
        return "WIFI DOWN";
    case NETWORK_SERVICE_STATE_UNCONFIGURED:
        return "WIFI UNCONFIGURED";
    case NETWORK_SERVICE_STATE_IDLE:
    default:
        return "WIFI IDLE";
    }
}

static const char *wifi_info_screen_get_interval_text(uint32_t seconds)
{
    static char buffer[24];
    if (seconds >= 3600U) {
        snprintf(buffer, sizeof(buffer), "%" PRIu32 "h%" PRIu32 "m", seconds / 3600U, (seconds % 3600U) / 60U);
    } else if (seconds >= 60U) {
        snprintf(buffer, sizeof(buffer), "%" PRIu32 "m%" PRIu32 "s", seconds / 60U, seconds % 60U);
    } else {
        snprintf(buffer, sizeof(buffer), "%" PRIu32 "s", seconds);
    }
    return buffer;
}

static void wifi_info_screen_format_token_compact(const char *token_text,
                                                  char *buffer,
                                                  size_t buffer_size)
{
    if ((buffer == NULL) || (buffer_size == 0U)) {
        return;
    }

    long long tokens = 0;
    if (token_text != NULL) {
        tokens = strtoll(token_text, NULL, 10);
    }

    if (tokens >= 1000000LL) {
        snprintf(buffer, buffer_size, "%.1fM", (double)tokens / 1000000.0);
    } else if (tokens >= 1000LL) {
        snprintf(buffer, buffer_size, "%.1fk", (double)tokens / 1000.0);
    } else {
        snprintf(buffer, buffer_size, "%lld", tokens);
    }
}

static void wifi_info_screen_format_token_delta_k(const char *token_text,
                                                  char *buffer,
                                                  size_t buffer_size)
{
    if ((buffer == NULL) || (buffer_size == 0U)) {
        return;
    }

    long long tokens = 0;
    if (token_text != NULL) {
        tokens = strtoll(token_text, NULL, 10);
    }

    double token_k = (double)tokens / 1000.0;
    snprintf(buffer, buffer_size, "-%.2fK", token_k);
}

static void wifi_info_screen_build_details_text(void)
{
    s_text_buffer[0] = '\0';
    size_t offset = 0;

    if (!s_details_diagnostic_mode) {
        wifi_info_screen_appendf(&offset,
                                 "AQI %s / %s\n",
                                 s_provider_snapshot_cache.state_text,
                                 s_provider_snapshot_cache.status_text);
        wifi_info_screen_appendf(&offset,
                                 "%s / %s\n",
                                 wifi_info_screen_get_wifi_summary_state(&s_snapshot_cache),
                                 wifi_info_screen_get_primary_text(&s_snapshot_cache));
        wifi_info_screen_appendf(&offset,
                                 "IPV4 %s / RSSI %d dBm\n",
                                 s_snapshot_cache.ip,
                                 s_snapshot_cache.rssi);
        wifi_info_screen_appendf(&offset,
                                 "DELTA %s / %s",
                                 s_provider_snapshot_cache.delta_used_amount,
                                 s_provider_snapshot_cache.delta_used_percent);
        return;
    }

    wifi_info_screen_appendf(&offset,
                             "AQI %s / %s / ACTIVE %" PRIu32 "\n",
                             s_provider_snapshot_cache.state_text,
                             s_provider_snapshot_cache.status_text,
                             s_provider_snapshot_cache.active_count);
    wifi_info_screen_appendf(&offset,
                             "AQI FETCH %" PRIu32 " / OK %" PRIu32 " / FAIL %" PRIu32 " / HTTP %ld\n",
                             s_provider_snapshot_cache.fetch_count,
                             s_provider_snapshot_cache.success_count,
                             s_provider_snapshot_cache.failure_count,
                             (long)s_provider_snapshot_cache.last_http_status);
    wifi_info_screen_appendf(&offset,
                             "DELTA %s / %s / Δ %s\n",
                             s_provider_snapshot_cache.delta_used_amount,
                             s_provider_snapshot_cache.delta_used_percent,
                             wifi_info_screen_get_interval_text(s_provider_snapshot_cache.last_success_interval_seconds));
    wifi_info_screen_appendf(&offset,
                             "WIFI %s / STATUS %s\n",
                             s_snapshot_cache.state_text,
                             s_snapshot_cache.status_text);
    wifi_info_screen_appendf(&offset,
                             "SSID %s / IPV4 %s / RSSI %d dBm\n",
                             s_snapshot_cache.connected_ssid,
                             s_snapshot_cache.ip,
                             s_snapshot_cache.rssi);
    wifi_info_screen_appendf(&offset,
                             "HOST %s / AP %s\n",
                             s_snapshot_cache.hostname,
                             s_snapshot_cache.connected_ssid);
    wifi_info_screen_appendf(&offset,
                             "CONFIG AP %s / IP %s\n",
                             s_snapshot_cache.config_ap_ssid,
                             s_snapshot_cache.ap_ip);
    wifi_info_screen_appendf(&offset,
                             "CONFIG AP %s / PASS %s\n",
                             s_snapshot_cache.softap_active ? "ON" : "OFF",
                             s_snapshot_cache.softap_active
                                 ? ((s_snapshot_cache.config_ap_password[0] != '\0') ? s_snapshot_cache.config_ap_password
                                                                                     : "(open)")
                                  : "hold BOOT 2s");
    if (wifi_info_screen_has_subscription(&s_provider_snapshot_cache.items[0])) {
        const provider_service_item_t *primary = &s_provider_snapshot_cache.items[0];
        wifi_info_screen_appendf(&offset,
                                 "#%ld LEFT %s / TOTAL %s / USED %u%% / END %s\n",
                                 (long)primary->id,
                                 primary->remaining_amount,
                                 primary->total_amount,
                                 primary->used_percent,
                                 primary->end_time);
    }
}

static void wifi_info_screen_set_label_color_recursive(lv_obj_t *root, uint32_t color)
{
    if (root == NULL) {
        return;
    }

    lv_obj_set_style_text_color(root, lv_color_hex(color), 0);
    uint32_t child_count = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < child_count; ++i) {
        wifi_info_screen_set_label_color_recursive(lv_obj_get_child(root, i), color);
    }
}

static void wifi_info_screen_set_delta_flash_visual(bool inverted)
{
    uint32_t bg_color = inverted ? WIFI_INFO_SCREEN_COLOR_WHITE : WIFI_INFO_SCREEN_COLOR_PANEL_BG;
    uint32_t text_color = inverted ? WIFI_INFO_SCREEN_COLOR_BLACK : WIFI_INFO_SCREEN_COLOR_WHITE;

    lv_obj_set_style_bg_color(s_secondary_row.badge_panel, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_color(s_secondary_row.main_panel, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_color(s_secondary_row.numeric_panel, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_color(s_secondary_row.strip_panel,
                              lv_color_hex(inverted ? WIFI_INFO_SCREEN_COLOR_WHITE
                                                    : WIFI_INFO_SCREEN_COLOR_PANEL_BG),
                              0);
    wifi_info_screen_set_label_color_recursive(s_secondary_row.badge_panel, text_color);
    wifi_info_screen_set_label_color_recursive(s_secondary_row.main_panel, text_color);
    wifi_info_screen_set_label_color_recursive(s_secondary_row.numeric_panel, text_color);
    wifi_info_screen_set_label_color_recursive(s_secondary_row.strip_panel,
                                               inverted ? WIFI_INFO_SCREEN_COLOR_BLACK
                                                        : WIFI_INFO_SCREEN_COLOR_WHITE);
    if (s_secondary_row.strip_fill_panel != NULL) {
        lv_obj_set_style_bg_color(s_secondary_row.strip_fill_panel,
                                  lv_color_hex(inverted ? WIFI_INFO_SCREEN_COLOR_WHITE
                                                        : WIFI_INFO_SCREEN_COLOR_DELTA),
                                  0);
    }
}

static uint32_t wifi_info_screen_get_delta_progress_percent(void)
{
    if (s_provider_snapshot_cache.refresh_interval_ms == 0U) {
        return 100U;
    }

    uint32_t remaining = s_provider_snapshot_cache.refresh_interval_ms;
    if (s_provider_snapshot_cache.last_fetch_monotonic_us > 0) {
        int64_t now_us = esp_timer_get_time();
        uint32_t elapsed_ms = (now_us > s_provider_snapshot_cache.last_fetch_monotonic_us)
                                  ? (uint32_t)((now_us - s_provider_snapshot_cache.last_fetch_monotonic_us) / 1000LL)
                                  : 0U;
        remaining = (elapsed_ms >= s_provider_snapshot_cache.refresh_interval_ms)
                        ? 0U
                        : (s_provider_snapshot_cache.refresh_interval_ms - elapsed_ms);
    }

    return (remaining * 100U) / s_provider_snapshot_cache.refresh_interval_ms;
}

static uint32_t wifi_info_screen_get_progress_color(uint32_t progress)
{
    if (progress >= 66U) {
        return WIFI_INFO_SCREEN_COLOR_GREEN;
    }
    if (progress >= 33U) {
        return WIFI_INFO_SCREEN_COLOR_AMBER;
    }
    return WIFI_INFO_SCREEN_COLOR_RED;
}

static void wifi_info_screen_update_delta_progress_fill(void)
{
    if ((s_secondary_row.strip_fill_panel == NULL) || (s_secondary_row.strip_panel == NULL)) {
        return;
    }

    uint32_t progress = wifi_info_screen_get_delta_progress_percent();
    uint32_t fill_width = (304U * progress) / 100U;
    uint32_t fill_color = wifi_info_screen_get_progress_color(progress);

    lv_obj_set_size(s_secondary_row.strip_fill_panel, (int32_t)fill_width, 34);
    lv_obj_align(s_secondary_row.strip_fill_panel, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_secondary_row.strip_fill_panel, lv_color_hex(fill_color), 0);
}

static uint32_t wifi_info_screen_get_primary_available_percent(const provider_service_item_t *item)
{
    if (!wifi_info_screen_has_subscription(item)) {
        return 0U;
    }

    // Provider 快照提供的是 used_percent，主卡右侧显示的是剩余额度百分比。
    return (item->used_percent >= 100U) ? 0U : (100U - item->used_percent);
}

static void wifi_info_screen_update_primary_available_fill(const provider_service_item_t *item)
{
    if ((s_primary_row.numeric_fill_panel == NULL) || (s_primary_row.numeric_panel == NULL)) {
        return;
    }

    uint32_t available_percent = wifi_info_screen_get_primary_available_percent(item);
    int32_t fill_width = lv_obj_get_width(s_primary_row.numeric_panel) - 8;
    int32_t fill_height = ((lv_obj_get_height(s_primary_row.numeric_panel) - 8) * (int32_t)available_percent) / 100;

    if (fill_width < 0) {
        fill_width = 0;
    }
    if (fill_height < 0) {
        fill_height = 0;
    }

    lv_obj_set_size(s_primary_row.numeric_fill_panel, fill_width, fill_height);
    lv_obj_align(s_primary_row.numeric_fill_panel, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(s_primary_row.numeric_fill_panel,
                              lv_color_hex(wifi_info_screen_get_progress_color(available_percent)),
                              0);
}

static void wifi_info_screen_handle_primary_touch(lv_event_t *event)
{
    (void)event;
    if (provider_service_request_refresh() != ESP_OK) {
        ESP_LOGW(TAG, "Manual provider refresh request failed");
    }
}

static void wifi_info_screen_handle_details_touch(lv_event_t *event)
{
    (void)event;
    s_details_diagnostic_mode = !s_details_diagnostic_mode;
    wifi_info_screen_build_details_text();
    wifi_info_screen_set_label_text_if_changed(s_details_label, s_text_buffer);
}

static wifi_info_screen_state_visual_t wifi_info_screen_get_state_visual(
    const network_service_snapshot_t *snapshot)
{
    switch (snapshot->state) {
    case NETWORK_SERVICE_STATE_CONNECTED:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "接続", .badge_color = WIFI_INFO_SCREEN_COLOR_GREEN};
    case NETWORK_SERVICE_STATE_CONNECTING:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "接続中",
                                                 .badge_color = WIFI_INFO_SCREEN_COLOR_AMBER};
    case NETWORK_SERVICE_STATE_PORTAL_REQUIRED:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "放行待ち",
                                                 .badge_color = WIFI_INFO_SCREEN_COLOR_AMBER};
    case NETWORK_SERVICE_STATE_DISCONNECTED:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "切断", .badge_color = WIFI_INFO_SCREEN_COLOR_RED};
    case NETWORK_SERVICE_STATE_UNCONFIGURED:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "未設定",
                                                 .badge_color = WIFI_INFO_SCREEN_COLOR_RED};
    case NETWORK_SERVICE_STATE_CONFIG_AP:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "設定AP",
                                                 .badge_color = WIFI_INFO_SCREEN_COLOR_BLUE};
    case NETWORK_SERVICE_STATE_IDLE:
    default:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "待機", .badge_color = WIFI_INFO_SCREEN_COLOR_BLUE};
    }
}

static const char *wifi_info_screen_get_primary_text(const network_service_snapshot_t *snapshot)
{
    if (wifi_info_screen_has_value(snapshot->connected_ssid)) {
        return snapshot->connected_ssid;
    }

    if (wifi_info_screen_has_value(snapshot->configured_ssid)) {
        return snapshot->configured_ssid;
    }

    return "NO SSID";
}

static const char *wifi_info_screen_get_secondary_text(const network_service_snapshot_t *snapshot)
{
    if (snapshot->ip_ready && wifi_info_screen_has_value(snapshot->ip)) {
        return snapshot->ip;
    }

    return "NO ADDRESS";
}

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
    if ((s_primary_row.badge_panel == NULL) || (s_secondary_row.badge_panel == NULL)
        || (s_details_label == NULL)) {
        return;
    }

    if (!s_first_refresh_completed) {
        wifi_info_screen_log_heap_snapshot("first-refresh");
    }

    network_service_get_snapshot(&s_snapshot_cache);
    provider_service_get_snapshot(&s_provider_snapshot_cache);

    const provider_service_item_t *primary_subscription = &s_provider_snapshot_cache.items[0];

    wifi_info_screen_set_panel_color_if_changed(s_primary_row.badge_panel,
                                                &s_primary_row.badge_color_cache,
                                                wifi_info_screen_get_aqi_badge_color(&s_provider_snapshot_cache,
                                                                                     primary_subscription));
    wifi_info_screen_set_label_text_if_changed(s_primary_row.badge_label, "AQI");
    wifi_info_screen_set_label_text_if_changed(s_primary_row.badge_subtitle,
                                               s_provider_snapshot_cache.state == PROVIDER_SERVICE_STATE_FETCHING
                                                   ? "REFRESHING"
                                                   : (wifi_info_screen_has_subscription(primary_subscription)
                                                          ? "SUB ACTIVE"
                                                          : s_provider_snapshot_cache.state_text));
    wifi_info_screen_set_label_text_if_changed(s_primary_row.main_label,
                                               wifi_info_screen_has_subscription(primary_subscription)
                                                   ? primary_subscription->remaining_amount
                                                   : "NO ACTIVE SUB");

    char primary_strip_text[128];
    if (wifi_info_screen_has_subscription(primary_subscription)) {
        snprintf(primary_strip_text,
                 sizeof(primary_strip_text),
                 "%s",
                 wifi_info_screen_get_countdown_text(primary_subscription->end_time_unix_seconds));
    } else {
        snprintf(primary_strip_text, sizeof(primary_strip_text), "%s", s_provider_snapshot_cache.status_text);
    }
    wifi_info_screen_set_label_text_if_changed(s_primary_row.strip_label, primary_strip_text);

    char primary_percent_text[8];
    if (wifi_info_screen_has_subscription(primary_subscription)) {
        snprintf(primary_percent_text,
                 sizeof(primary_percent_text),
                 "%" PRIu32 "%%",
                 wifi_info_screen_get_primary_available_percent(primary_subscription));
    } else {
        snprintf(primary_percent_text, sizeof(primary_percent_text), "--");
    }
    wifi_info_screen_update_primary_available_fill(primary_subscription);
    wifi_info_screen_set_label_text_if_changed(s_primary_row.numeric_value_label, primary_percent_text);

    wifi_info_screen_set_panel_color_if_changed(s_secondary_row.badge_panel,
                                                &s_secondary_row.badge_color_cache,
                                                (s_provider_snapshot_cache.delta_used_raw > 0)
                                                    ? WIFI_INFO_SCREEN_COLOR_AMBER
                                                    : WIFI_INFO_SCREEN_COLOR_BLUE);
    wifi_info_screen_set_panel_color_if_changed(s_secondary_row.strip_panel,
                                                &s_secondary_row.strip_color_cache,
                                                WIFI_INFO_SCREEN_COLOR_PANEL_BG);
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.badge_label, "DELTA");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.badge_subtitle, "SINCE LAST OK");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.main_label,
                                               s_provider_snapshot_cache.delta_used_amount);
    wifi_info_screen_update_delta_progress_fill();

    char secondary_strip_text[128];
    char secondary_token_text[24];
    wifi_info_screen_format_token_delta_k(s_provider_snapshot_cache.delta_used_tokens,
                                          secondary_token_text,
                                          sizeof(secondary_token_text));
    snprintf(secondary_strip_text,
             sizeof(secondary_strip_text),
             "Δ %s / %s",
             wifi_info_screen_get_interval_text(s_provider_snapshot_cache.last_success_interval_seconds),
             secondary_token_text);
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.strip_label, secondary_strip_text);

    wifi_info_screen_set_label_text_if_changed(s_secondary_row.numeric_caption_label, "$ / Hour");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.numeric_value_label,
                                               s_provider_snapshot_cache.hourly_used_amount);

    if ((s_provider_snapshot_cache.last_success_interval_seconds > 0U)
        && (s_provider_snapshot_cache.last_fetch_unix_seconds != s_last_delta_flash_marker)) {
        s_last_delta_flash_marker = s_provider_snapshot_cache.last_fetch_unix_seconds;
        s_delta_flash_ticks_remaining = 6U;
        s_delta_flash_toggle_count = 0U;
    }

    if (s_delta_flash_ticks_remaining > 0U) {
        bool inverted = ((s_delta_flash_toggle_count % 2U) == 0U);
        wifi_info_screen_set_delta_flash_visual(inverted);
        s_delta_flash_ticks_remaining--;
        s_delta_flash_toggle_count++;
    } else {
        wifi_info_screen_set_delta_flash_visual(false);
        wifi_info_screen_update_delta_progress_fill();
    }

    wifi_info_screen_build_details_text();
    wifi_info_screen_set_label_text_if_changed(s_details_label, s_text_buffer);

    if ((timer != NULL) && !s_first_refresh_completed) {
        s_first_refresh_completed = true;
        lv_timer_set_period(timer, CONFIG_AI_MONITOR_UI_REFRESH_MS);
    }
}

static void wifi_info_screen_create_layout(void)
{
    wifi_info_screen_prepare_fonts();

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(WIFI_INFO_SCREEN_COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    wifi_info_screen_create_board_row(&s_primary_row,
                                      screen,
                                      24,
                                      264,
                                      54,
                                      s_font_balance,
                                      s_font_medium);
    wifi_info_screen_create_board_row(&s_secondary_row,
                                      screen,
                                      304,
                                      180,
                                      42,
                                      s_font_delta,
                                      s_font_medium);
    wifi_info_screen_set_panel_color_if_changed(s_secondary_row.badge_panel,
                                                &s_secondary_row.badge_color_cache,
                                                WIFI_INFO_SCREEN_COLOR_DELTA);
    wifi_info_screen_set_panel_color_if_changed(s_secondary_row.strip_panel,
                                                &s_secondary_row.strip_color_cache,
                                                WIFI_INFO_SCREEN_COLOR_BLUE);
    wifi_info_screen_set_panel_color_if_changed(s_primary_row.badge_panel,
                                                &s_primary_row.badge_color_cache,
                                                WIFI_INFO_SCREEN_COLOR_AQI);
    wifi_info_screen_set_panel_color_if_changed(s_primary_row.strip_panel,
                                                &s_primary_row.strip_color_cache,
                                                WIFI_INFO_SCREEN_COLOR_BLUE);
    wifi_info_screen_set_label_text_if_changed(s_primary_row.numeric_caption_label, "Avail %");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.badge_label, "DELTA");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.badge_subtitle, "SINCE LAST OK");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.numeric_caption_label, "$ / Hour");
    lv_obj_add_flag(s_primary_row.badge_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_primary_row.main_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_primary_row.numeric_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_primary_row.badge_panel, wifi_info_screen_handle_primary_touch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_primary_row.main_panel, wifi_info_screen_handle_primary_touch, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_primary_row.numeric_panel, wifi_info_screen_handle_primary_touch, LV_EVENT_CLICKED, NULL);

    s_details_panel = wifi_info_screen_create_panel(screen,
                                                    672,
                                                    156,
                                                    WIFI_INFO_SCREEN_COLOR_PANEL_BG,
                                                    WIFI_INFO_SCREEN_COLOR_BORDER);
    lv_obj_align(s_details_panel, LV_ALIGN_TOP_LEFT, 24, 500);
    lv_obj_add_flag(s_details_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_details_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(s_details_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_details_panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_top(s_details_panel, 10, 0);
    lv_obj_set_style_pad_bottom(s_details_panel, 10, 0);
    lv_obj_add_event_cb(s_details_panel, wifi_info_screen_handle_details_touch, LV_EVENT_CLICKED, NULL);

    lv_obj_t *details_title = wifi_info_screen_create_label(s_details_panel,
                                                            s_font_medium,
                                                            WIFI_INFO_SCREEN_COLOR_DIM);
    lv_label_set_text(details_title, "STATUS / TAP FOR DIAG");
    lv_obj_align(details_title, LV_ALIGN_TOP_LEFT, 14, 10);

    s_details_label = wifi_info_screen_create_label(s_details_panel,
                                                    s_font_small,
                                                    WIFI_INFO_SCREEN_COLOR_WHITE);
    lv_obj_set_width(s_details_label, 636);
    lv_label_set_long_mode(s_details_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_details_label, LV_ALIGN_TOP_LEFT, 14, 50);
    lv_label_set_text(s_details_label, "AQI and Wi-Fi board loading...");

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
