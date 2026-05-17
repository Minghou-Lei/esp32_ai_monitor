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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "network_service.h"
#include "sdkconfig.h"

static const char *TAG = "wifi_info_screen";
static const uint32_t WIFI_INFO_SCREEN_DRAW_BUFFER_LINES = 20;
static const uint32_t WIFI_INFO_SCREEN_FIRST_REFRESH_MS = 200;
static const int32_t WIFI_INFO_SCREEN_FONT_SIZE_SMALL = 20;
static const int32_t WIFI_INFO_SCREEN_FONT_SIZE_MEDIUM = 28;
static const uint32_t WIFI_INFO_SCREEN_COLOR_BLACK = 0x000000;
static const uint32_t WIFI_INFO_SCREEN_COLOR_WHITE = 0xFFFFFF;
static const uint32_t WIFI_INFO_SCREEN_COLOR_BORDER = 0xF5F5F5;
static const uint32_t WIFI_INFO_SCREEN_COLOR_AMBER = 0xFF6A00;
static const uint32_t WIFI_INFO_SCREEN_COLOR_BLUE = 0x1246B9;
static const uint32_t WIFI_INFO_SCREEN_COLOR_GREEN = 0x51D46A;
static const uint32_t WIFI_INFO_SCREEN_COLOR_RED = 0xD93B30;
static const uint32_t WIFI_INFO_SCREEN_COLOR_DIM = 0xD1D5DB;
static const uint32_t WIFI_INFO_SCREEN_COLOR_PANEL_BG = 0x050505;

typedef struct {
    lv_obj_t *badge_panel;
    lv_obj_t *badge_label;
    lv_obj_t *badge_subtitle;
    lv_obj_t *main_panel;
    lv_obj_t *main_label;
    lv_obj_t *strip_panel;
    lv_obj_t *strip_label;
    lv_obj_t *numeric_panel;
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
static bool s_screen_started;
static bool s_first_refresh_completed;
static bool s_fonts_ready;
static lv_font_t *s_font_small;
static lv_font_t *s_font_medium;
static lv_font_t *s_font_large;
static lv_font_t *s_font_numeric;
static network_service_snapshot_t s_snapshot_cache;
static char s_text_buffer[1536];

extern const uint8_t jnr_sb_font_ttf_start[] asm("_binary_jnr_sb_font_ttf_start");
extern const uint8_t jnr_sb_font_ttf_end[] asm("_binary_jnr_sb_font_ttf_end");

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

    /*
     * 720x720 DPI 面板的整帧缓冲由底层驱动放在 PSRAM。
     * 这里把 LVGL 的绘图缓冲也切到 PSRAM，并收小为 20 行，避免继续挤占片上 SRAM。
     */
    return bsp_display_start_with_config(&display_cfg);
}

/**
 * @brief 在 BSP 完成 LVGL 启动后，从嵌入式 TTF 资产构建一组站牌风格运行时字体。
 *
 * TinyTTF 的缓存分配走 LVGL 堆；因此必须等 `bsp_display_start_with_config()`
 * 成功后再创建字体对象，否则字体缓存会绑定到未初始化的 LVGL 运行时。
 */
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

    /*
     * P4 + LVGL9 + TinyTTF 在首帧绘制大字号时，会为字形临时申请 A8 draw buffer。
     * 68/104 px 站牌字体在当前内存布局下容易让 `lv_draw_buf_create_ex()` 失败，
     * 随后 taskLVGL 卡死在 LV_ASSERT_MALLOC 分支并持续喂不动 task WDT。
     *
     * 这里先复用中号字体给大标题和数字卡片，优先保证板子稳定启动；
     * 等运行时链路稳定后，再单独优化大字号显示方案。
     */
    s_font_large = s_font_medium;
    s_font_numeric = s_font_medium;

    if ((s_font_small == NULL) || (s_font_medium == NULL)) {
        ESP_LOGW(TAG, "Failed to create TinyTTF base fonts for station board layout");
    } else {
        ESP_LOGI(TAG,
                 "Loaded TinyTTF station fonts (%u bytes, %ld/%ld px, large/numeric reuse medium)",
                 (unsigned int)font_data_size,
                 (long)WIFI_INFO_SCREEN_FONT_SIZE_SMALL,
                 (long)WIFI_INFO_SCREEN_FONT_SIZE_MEDIUM);
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

/**
 * @brief 仅在文本发生变化时更新大号标签，避免 taskLVGL 每秒重复重排站牌字体。
 *
 * 这次 UI 使用了多个 TinyTTF 大字号对象；如果在定时器里无差别地反复
 * `lv_label_set_text()`，LVGL 会持续重建文本布局和字形缓存，容易把 IDLE0
 * 饿死并触发 watchdog。
 */
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
    int32_t main_label_top = (height >= 200) ? 18 : 12;
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
    lv_obj_align(row->main_label, LV_ALIGN_TOP_MID, 0, main_label_top);

    row->strip_panel = wifi_info_screen_create_panel(row->main_panel,
                                                     332,
                                                     strip_height,
                                                     WIFI_INFO_SCREEN_COLOR_BLUE,
                                                     WIFI_INFO_SCREEN_COLOR_BORDER);
    lv_obj_align(row->strip_panel, LV_ALIGN_BOTTOM_MID, 0, -12);

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

static wifi_info_screen_state_visual_t wifi_info_screen_get_state_visual(
    const network_service_snapshot_t *snapshot)
{
    switch (snapshot->state) {
    case NETWORK_SERVICE_STATE_CONNECTED:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "接続", .badge_color = WIFI_INFO_SCREEN_COLOR_GREEN};
    case NETWORK_SERVICE_STATE_CONNECTING:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "接続中",
                                                 .badge_color = WIFI_INFO_SCREEN_COLOR_AMBER};
    case NETWORK_SERVICE_STATE_DISCONNECTED:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "切断", .badge_color = WIFI_INFO_SCREEN_COLOR_RED};
    case NETWORK_SERVICE_STATE_UNCONFIGURED:
        return (wifi_info_screen_state_visual_t){.badge_text_jp = "未設定",
                                                 .badge_color = WIFI_INFO_SCREEN_COLOR_RED};
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
    if ((s_primary_row.badge_panel == NULL) || (s_secondary_row.badge_panel == NULL)
        || (s_details_label == NULL)) {
        return;
    }

    if (!s_first_refresh_completed) {
        wifi_info_screen_log_heap_snapshot("first-refresh");
    }

    network_service_get_snapshot(&s_snapshot_cache);

    wifi_info_screen_state_visual_t state_visual = wifi_info_screen_get_state_visual(&s_snapshot_cache);
    wifi_info_screen_set_panel_color_if_changed(s_primary_row.badge_panel,
                                                &s_primary_row.badge_color_cache,
                                                state_visual.badge_color);
    wifi_info_screen_set_label_text_if_changed(s_primary_row.badge_label, state_visual.badge_text_jp);
    wifi_info_screen_set_label_text_if_changed(s_primary_row.badge_subtitle, s_snapshot_cache.state_text);
    wifi_info_screen_set_label_text_if_changed(s_primary_row.main_label,
                                               wifi_info_screen_get_primary_text(&s_snapshot_cache));
    wifi_info_screen_set_label_text_if_changed(s_primary_row.strip_label, s_snapshot_cache.status_text);

    char primary_channel_text[8];
    if (s_snapshot_cache.primary_channel > 0U) {
        snprintf(primary_channel_text, sizeof(primary_channel_text), "%" PRIu8, s_snapshot_cache.primary_channel);
    } else {
        snprintf(primary_channel_text, sizeof(primary_channel_text), "--");
    }
    wifi_info_screen_set_label_text_if_changed(s_primary_row.numeric_value_label, primary_channel_text);

    wifi_info_screen_set_label_text_if_changed(s_secondary_row.main_label,
                                               wifi_info_screen_get_secondary_text(&s_snapshot_cache));

    char host_strip_text[128];
    const char *associated_ap = wifi_info_screen_has_value(s_snapshot_cache.connected_ssid)
                                    ? s_snapshot_cache.connected_ssid
                                    : wifi_info_screen_get_primary_text(&s_snapshot_cache);
    snprintf(host_strip_text,
             sizeof(host_strip_text),
             "HOST %s / AP %s",
             s_snapshot_cache.hostname,
             associated_ap);
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.strip_label, host_strip_text);

    char rssi_text[16];
    if (s_snapshot_cache.state == NETWORK_SERVICE_STATE_CONNECTED) {
        snprintf(rssi_text, sizeof(rssi_text), "%d", s_snapshot_cache.rssi);
    } else {
        snprintf(rssi_text, sizeof(rssi_text), "--");
    }
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.numeric_value_label, rssi_text);

    s_text_buffer[0] = '\0';
    size_t offset = 0;
    wifi_info_screen_appendf(&offset,
                             "STATE %s / STATUS %s\n",
                             s_snapshot_cache.state_text,
                             s_snapshot_cache.status_text);
    wifi_info_screen_appendf(&offset,
                             "CRED menuconfig / CFG SSID %s / PWD %s (%" PRIu8 ")\n",
                             s_snapshot_cache.configured_ssid,
                             s_snapshot_cache.password_configured ? "yes" : "no",
                             s_snapshot_cache.configured_password_length);
    wifi_info_screen_appendf(&offset,
                             "AP %s / HOST %s / STA MAC %s\n",
                             s_snapshot_cache.connected_ssid,
                             s_snapshot_cache.hostname,
                             s_snapshot_cache.sta_mac);
    wifi_info_screen_appendf(&offset,
                             "BSSID %s / IPv4 %s / MASK %s\n",
                             s_snapshot_cache.bssid,
                             s_snapshot_cache.ip,
                             s_snapshot_cache.netmask);
    wifi_info_screen_appendf(&offset,
                             "GW %s / DNS %s , %s\n",
                             s_snapshot_cache.gateway,
                             s_snapshot_cache.dns_main,
                             s_snapshot_cache.dns_backup);
    wifi_info_screen_appendf(&offset,
                             "RSSI %d dBm / CH %" PRIu8 " / SECOND %s\n",
                             s_snapshot_cache.rssi,
                             s_snapshot_cache.primary_channel,
                             s_snapshot_cache.second_channel);
    wifi_info_screen_appendf(&offset,
                             "AUTH %s / PAIR %s / GROUP %s\n",
                             s_snapshot_cache.auth_mode,
                             s_snapshot_cache.pairwise_cipher,
                             s_snapshot_cache.group_cipher);
    wifi_info_screen_appendf(&offset,
                             "RETRY %" PRIu32 " / DISC %u",
                             s_snapshot_cache.reconnect_attempts,
                             s_snapshot_cache.last_disconnect_reason);

    wifi_info_screen_set_label_text_if_changed(s_details_label, s_text_buffer);

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
    wifi_info_screen_prepare_fonts();

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(WIFI_INFO_SCREEN_COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    wifi_info_screen_create_board_row(&s_primary_row,
                                      screen,
                                      24,
                                      232,
                                      54,
                                      s_font_large,
                                      s_font_numeric);
    wifi_info_screen_create_board_row(&s_secondary_row,
                                      screen,
                                      272,
                                      168,
                                      42,
                                      s_font_large,
                                      s_font_large);
    wifi_info_screen_set_panel_color_if_changed(s_secondary_row.badge_panel,
                                                &s_secondary_row.badge_color_cache,
                                                WIFI_INFO_SCREEN_COLOR_BLUE);
    wifi_info_screen_set_panel_color_if_changed(s_secondary_row.strip_panel,
                                                &s_secondary_row.strip_color_cache,
                                                WIFI_INFO_SCREEN_COLOR_GREEN);
    wifi_info_screen_set_label_text_if_changed(s_primary_row.numeric_caption_label, "CH");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.badge_label, "IPV4");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.badge_subtitle, "ADDRESS");
    wifi_info_screen_set_label_text_if_changed(s_secondary_row.numeric_caption_label, "RSSI");

    s_details_panel = wifi_info_screen_create_panel(screen,
                                                    672,
                                                    232,
                                                    WIFI_INFO_SCREEN_COLOR_PANEL_BG,
                                                    WIFI_INFO_SCREEN_COLOR_BORDER);
    lv_obj_align(s_details_panel, LV_ALIGN_TOP_LEFT, 24, 460);
    lv_obj_add_flag(s_details_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_details_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_details_panel, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_top(s_details_panel, 10, 0);
    lv_obj_set_style_pad_bottom(s_details_panel, 10, 0);

    lv_obj_t *details_title = wifi_info_screen_create_label(s_details_panel,
                                                            s_font_medium,
                                                            WIFI_INFO_SCREEN_COLOR_DIM);
    lv_label_set_text(details_title, "DETAILS / 詳細");
    lv_obj_align(details_title, LV_ALIGN_TOP_LEFT, 14, 10);

    s_details_label = wifi_info_screen_create_label(s_details_panel,
                                                    s_font_small,
                                                    WIFI_INFO_SCREEN_COLOR_WHITE);
    lv_obj_set_width(s_details_label, 636);
    lv_label_set_long_mode(s_details_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_details_label, LV_ALIGN_TOP_LEFT, 14, 50);
    lv_label_set_text(s_details_label, "Wi-Fi board loading...");

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
