/**
 * @file    config_web_service.c
 * @brief   板上配置网页与 REST 接口实现。
 *
 * 当前页面追求“在公司网络 bring-up 阶段就能可靠使用”：HTML 保持单文件、接口
 * 保持少量且可抓包，所有保存动作都走同一份 `app_config_service` 校验逻辑。
 */

#include "config_web_service.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "app_config_service.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "network_service.h"
#include "provider_service.h"

static const char *TAG = "config_web_service";
static const size_t CONFIG_WEB_SERVICE_BODY_LIMIT = 4096U;
static const uint64_t CONFIG_WEB_SERVICE_RESTART_DELAY_US = 700000ULL;
static const uint16_t CONFIG_WEB_SERVICE_DNS_PORT = 53U;
static const size_t CONFIG_WEB_SERVICE_DNS_BUFFER_SIZE = 512U;
static const uint32_t CONFIG_WEB_SERVICE_PROXY_TIMEOUT_MS = 15000U;
static const uint8_t CONFIG_WEB_SERVICE_DNS_HEADER_SIZE = 12U;
static const uint8_t CONFIG_WEB_SERVICE_DNS_TYPE_A = 1U;
static const uint8_t CONFIG_WEB_SERVICE_DNS_CLASS_IN = 1U;

#define CONFIG_WEB_SERVICE_PROXY_URL_LEN 512U
#define CONFIG_WEB_SERVICE_PROXY_COOKIE_LEN 1024U
#define CONFIG_WEB_SERVICE_PROXY_BODY_LIMIT 49152U
#define CONFIG_WEB_SERVICE_PROXY_CONTENT_TYPE_LEN 96U

static httpd_handle_t s_server;
static esp_timer_handle_t s_restart_timer;
static TaskHandle_t s_dns_task;
static char s_portal_proxy_last_url[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
static char s_portal_proxy_cookie[CONFIG_WEB_SERVICE_PROXY_COOKIE_LEN];

static void config_web_service_restart_callback(void *arg)
{
    (void)arg;
    esp_restart();
}

static esp_err_t config_web_service_schedule_restart(void)
{
    if (s_restart_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = config_web_service_restart_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "cfg_restart",
            .skip_unhandled_events = true,
        };
        ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_restart_timer), TAG, "restart timer create failed");
    }

    (void)esp_timer_stop(s_restart_timer);
    return esp_timer_start_once(s_restart_timer, CONFIG_WEB_SERVICE_RESTART_DELAY_US);
}

static size_t config_web_service_find_dns_question_end(const uint8_t *buffer, size_t length)
{
    size_t offset = CONFIG_WEB_SERVICE_DNS_HEADER_SIZE;

    while (offset < length) {
        uint8_t label_len = buffer[offset++];
        if (label_len == 0U) {
            return offset;
        }
        if ((label_len & 0xC0U) != 0U) {
            return 0U;
        }
        if ((offset + label_len) > length) {
            return 0U;
        }
        offset += label_len;
    }

    return 0U;
}

static void config_web_service_put_u16(uint8_t *buffer, size_t offset, uint16_t value)
{
    buffer[offset] = (uint8_t)(value >> 8);
    buffer[offset + 1U] = (uint8_t)(value & 0xFFU);
}

static void config_web_service_put_u32(uint8_t *buffer, size_t offset, uint32_t value)
{
    buffer[offset] = (uint8_t)(value >> 24);
    buffer[offset + 1U] = (uint8_t)((value >> 16) & 0xFFU);
    buffer[offset + 2U] = (uint8_t)((value >> 8) & 0xFFU);
    buffer[offset + 3U] = (uint8_t)(value & 0xFFU);
}

static bool config_web_service_get_ap_ipv4(uint32_t *out_addr)
{
    if (out_addr == NULL) {
        return false;
    }

    network_service_snapshot_t snapshot = {0};
    network_service_get_snapshot(&snapshot);
    if (!snapshot.softap_active || (snapshot.ap_ip[0] == '\0') || (strcmp(snapshot.ap_ip, "-") == 0)) {
        return false;
    }

    struct in_addr ap_addr = {0};
    if (inet_aton(snapshot.ap_ip, &ap_addr) == 0) {
        return false;
    }

    *out_addr = ntohl(ap_addr.s_addr);
    return true;
}

static size_t config_web_service_build_dns_response(uint8_t *buffer, size_t length, uint32_t ap_addr)
{
    if ((buffer == NULL) || (length < (CONFIG_WEB_SERVICE_DNS_HEADER_SIZE + 5U))) {
        return 0U;
    }

    size_t question_end = config_web_service_find_dns_question_end(buffer, length);
    if ((question_end == 0U) || ((question_end + 4U) > length)) {
        return 0U;
    }

    uint16_t qtype = ((uint16_t)buffer[question_end] << 8) | buffer[question_end + 1U];
    uint16_t qclass = ((uint16_t)buffer[question_end + 2U] << 8) | buffer[question_end + 3U];
    if ((qtype != CONFIG_WEB_SERVICE_DNS_TYPE_A) || (qclass != CONFIG_WEB_SERVICE_DNS_CLASS_IN)) {
        return 0U;
    }

    size_t response_len = question_end + 4U;
    if ((response_len + 16U) > CONFIG_WEB_SERVICE_DNS_BUFFER_SIZE) {
        return 0U;
    }

    buffer[2] = 0x81U;
    buffer[3] = 0x80U;
    config_web_service_put_u16(buffer, 4U, 1U);
    config_web_service_put_u16(buffer, 6U, 1U);
    config_web_service_put_u16(buffer, 8U, 0U);
    config_web_service_put_u16(buffer, 10U, 0U);

    buffer[response_len++] = 0xC0U;
    buffer[response_len++] = CONFIG_WEB_SERVICE_DNS_HEADER_SIZE;
    config_web_service_put_u16(buffer, response_len, CONFIG_WEB_SERVICE_DNS_TYPE_A);
    response_len += 2U;
    config_web_service_put_u16(buffer, response_len, CONFIG_WEB_SERVICE_DNS_CLASS_IN);
    response_len += 2U;
    config_web_service_put_u32(buffer, response_len, 60U);
    response_len += 4U;
    config_web_service_put_u16(buffer, response_len, 4U);
    response_len += 2U;
    config_web_service_put_u32(buffer, response_len, ap_addr);
    response_len += 4U;

    return response_len;
}

static void config_web_service_dns_task(void *arg)
{
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "captive DNS socket create failed");
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    int reuse = 1;
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in listen_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_WEB_SERVICE_DNS_PORT),
        .sin_addr = {
            .s_addr = htonl(INADDR_ANY),
        },
    };

    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) != 0) {
        ESP_LOGE(TAG, "captive DNS bind failed");
        closesocket(sock);
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    uint8_t buffer[CONFIG_WEB_SERVICE_DNS_BUFFER_SIZE];
    while (true) {
        struct sockaddr_in source_addr = {0};
        socklen_t source_len = sizeof(source_addr);
        int received = recvfrom(sock,
                                buffer,
                                sizeof(buffer),
                                0,
                                (struct sockaddr *)&source_addr,
                                &source_len);
        if (received <= 0) {
            continue;
        }

        uint32_t ap_addr = 0;
        if (!config_web_service_get_ap_ipv4(&ap_addr)) {
            continue;
        }

        size_t response_len = config_web_service_build_dns_response(buffer, (size_t)received, ap_addr);
        if (response_len == 0U) {
            continue;
        }

        (void)sendto(sock,
                     buffer,
                     response_len,
                     0,
                     (struct sockaddr *)&source_addr,
                     source_len);
    }
}

static esp_err_t config_web_service_start_captive_dns(void)
{
    if (s_dns_task != NULL) {
        return ESP_OK;
    }

    BaseType_t task_created = xTaskCreate(config_web_service_dns_task,
                                          "cfg_dns",
                                          4096U,
                                          NULL,
                                          tskIDLE_PRIORITY + 1,
                                          &s_dns_task);
    return (task_created == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}

static const char *CONFIG_WEB_SERVICE_HTML =
    "<!doctype html>"
    "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32 AI Monitor Config</title>"
    "<style>"
    "body{font-family:system-ui,-apple-system,sans-serif;background:#0b1220;color:#f3f4f6;margin:0;padding:20px;}"
    "main{max-width:960px;margin:0 auto;display:grid;gap:16px;}"
    "section{background:#111827;border:1px solid #1f2937;border-radius:14px;padding:16px;}"
    "h1,h2{margin:0 0 12px 0;} h1{font-size:28px;} h2{font-size:18px;color:#d1d5db;}"
    "label{display:block;margin:10px 0 4px;color:#9ca3af;font-size:14px;}"
    "input,select{width:100%;padding:10px 12px;border-radius:10px;border:1px solid #374151;background:#030712;color:#f9fafb;box-sizing:border-box;}"
    ".row{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px;}"
    ".actions{display:flex;flex-wrap:wrap;gap:12px;margin-top:16px;}"
    "button{padding:10px 16px;border:none;border-radius:10px;background:#2563eb;color:white;font-weight:600;cursor:pointer;}"
    "button.secondary{background:#374151;} .status{white-space:pre-wrap;color:#93c5fd;font-size:14px;}"
    ".hint{font-size:13px;color:#9ca3af;margin-top:6px;}"
    "</style></head>"
    "<body><main>"
    "<section><h1>ESP32 AI Monitor Config</h1><div id='runtime' class='status'>Loading runtime status...</div></section>"
    "<section><h2>Wi-Fi</h2><form id='cfgForm'>"
    "<div class='row'><div><label>SSID</label><input name='wifi_ssid'></div><div><label>Password</label><input name='wifi_password' type='password'></div></div>"
    "<div class='row'><div><label>Hostname</label><input name='wifi_hostname'></div><div><label>Security</label><select name='wifi_security'><option value='open'>open</option><option value='wpa2-psk'>wpa2-psk</option><option value='wpa2-enterprise'>wpa2-enterprise</option></select></div></div>"
    "<div class='row'><div><label>EAP Identity</label><input name='wifi_eap_identity'></div><div><label>EAP Username</label><input name='wifi_eap_username'></div><div><label>EAP Password</label><input name='wifi_eap_password' type='password'></div></div>"
    "<label><input type='checkbox' name='wifi_portal_enabled' value='1' style='width:auto;margin-right:8px;'>Portal / OA registration required</label>"
    "<div class='row'><div><label>Portal URL</label><input name='wifi_portal_url'></div><div><label>Portal Username</label><input name='wifi_portal_username'></div><div><label>Portal Password</label><input name='wifi_portal_password' type='password'></div></div>"
    "<div class='actions'><button type='button' class='secondary' id='portalOpen'>Open Portal via Device</button></div>"
    "<h2>Config AP</h2>"
    "<div class='row'><div><label>AP SSID</label><input name='config_ap_ssid'></div><div><label>AP Password</label><input name='config_ap_password' type='password'></div></div>"
    "<h2>Provider</h2>"
    "<div class='row'><div><label>Provider</label><select name='provider_kind'><option value='aqi'>aqi</option><option value='none'>none</option></select></div><div><label>Display Name</label><input name='provider_display_name'></div></div>"
    "<div class='row'><div><label>Base URL</label><input name='provider_base_url'></div><div><label>Endpoint Path</label><input name='provider_endpoint_path'></div></div>"
    "<div class='row'><div><label>Access Token</label><input name='provider_access_token' type='password'></div><div><label>Management Key</label><input name='provider_management_key' type='password'></div></div>"
    "<div class='row'><div><label>User Header Name</label><input name='provider_user_header_name'></div><div><label>User Header Value</label><input name='provider_user_header_value'></div></div>"
    "<div class='row'><div><label>Provider Refresh (ms)</label><input name='provider_refresh_interval_ms' type='number'></div><div><label>UI Refresh (ms)</label><input name='ui_refresh_interval_ms' type='number'></div></div>"
    "<div class='actions'><button type='submit'>Save Config</button><button type='button' class='secondary' id='portalDone'>Mark Portal Complete</button><button type='button' class='secondary' id='restartBtn'>Restart Device</button></div>"
    "<div class='hint'>Saving writes to NVS. The device restarts after save so Wi-Fi and provider changes apply.</div>"
    "</form><div id='result' class='status'></div></section>"
    "<script>"
    "async function loadConfig(){try{const r=await fetch('/api/config'); const cfg=await r.json();"
    "for(const [k,v] of Object.entries(cfg)){const el=document.querySelector(`[name=\"${k}\"]`); if(!el) continue; if(el.type==='checkbox'){el.checked=!!v;} else {el.value=v ?? '';}}"
    "}catch(e){document.getElementById('result').textContent='Load config failed: '+e;}}"
    "async function loadStatus(){try{const r=await fetch('/api/status'); const status=await r.json();"
    "document.getElementById('runtime').textContent=`Network: ${status.network_state}\\nIP: ${status.network_ip}\\nPortal: ${status.portal_state}\\nProvider: ${status.provider_state}\\nProvider Status: ${status.provider_status}`;"
    "}catch(e){document.getElementById('runtime').textContent='Runtime status unavailable: '+e;}}"
    "async function load(){await loadConfig(); await loadStatus();}"
    "document.getElementById('cfgForm').addEventListener('submit', async (e)=>{e.preventDefault(); const body=new URLSearchParams(new FormData(e.target)); const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body}); document.getElementById('result').textContent=await r.text(); if(r.ok){return;} load();});"
    "document.getElementById('portalOpen').addEventListener('click',()=>{const v=document.querySelector('[name=\"wifi_portal_url\"]').value || 'http://1.1.1.1'; location.href='/portal/open?url='+encodeURIComponent(v);});"
    "document.getElementById('portalDone').addEventListener('click', async ()=>{const r=await fetch('/api/portal/complete',{method:'POST'}); document.getElementById('result').textContent=await r.text(); load();});"
    "document.getElementById('restartBtn').addEventListener('click', async ()=>{const r=await fetch('/api/restart',{method:'POST'}); document.getElementById('result').textContent=await r.text();});"
    "load();"
    "</script></main></body></html>";

static void config_web_service_copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0U)) {
        return;
    }
    snprintf(dst, dst_size, "%s", (src != NULL) ? src : "");
}

static int config_web_service_hex_value(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'f')) {
        return c - 'a' + 10;
    }
    if ((c >= 'A') && (c <= 'F')) {
        return c - 'A' + 10;
    }
    return -1;
}

static void config_web_service_url_decode(char *text)
{
    if (text == NULL) {
        return;
    }

    char *src = text;
    char *dst = text;
    while (*src != '\0') {
        if ((*src == '%') && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            int hi = config_web_service_hex_value(src[1]);
            int lo = config_web_service_hex_value(src[2]);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static bool config_web_service_url_encode(const char *src, char *dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t out = 0U;

    if ((src == NULL) || (dst == NULL) || (dst_size == 0U)) {
        return false;
    }

    while (*src != '\0') {
        unsigned char c = (unsigned char)*src++;
        bool safe = ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'))
                    || (c == '-') || (c == '_') || (c == '.') || (c == '~');
        size_t needed = safe ? 1U : 3U;
        if ((out + needed + 1U) > dst_size) {
            return false;
        }
        if (safe) {
            dst[out++] = (char)c;
        } else {
            dst[out++] = '%';
            dst[out++] = hex[c >> 4];
            dst[out++] = hex[c & 0x0FU];
        }
    }

    dst[out] = '\0';
    return true;
}

static bool config_web_service_string_starts_with(const char *text, const char *prefix)
{
    return (text != NULL) && (prefix != NULL) && (strncmp(text, prefix, strlen(prefix)) == 0);
}

static bool config_web_service_is_absolute_url(const char *url)
{
    return config_web_service_string_starts_with(url, "http://")
           || config_web_service_string_starts_with(url, "https://");
}

static bool config_web_service_same_text_ci(const char *left, const char *right)
{
    if ((left == NULL) || (right == NULL)) {
        return false;
    }

    while ((*left != '\0') && (*right != '\0')) {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }
        left++;
        right++;
    }

    return (*left == '\0') && (*right == '\0');
}

static bool config_web_service_get_url_origin(const char *url, char *origin, size_t origin_size)
{
    if ((url == NULL) || (origin == NULL) || (origin_size == 0U)) {
        return false;
    }

    const char *scheme_end = strstr(url, "://");
    if (scheme_end == NULL) {
        return false;
    }

    const char *host_start = scheme_end + 3;
    const char *path_start = strchr(host_start, '/');
    size_t len = (path_start != NULL) ? (size_t)(path_start - url) : strlen(url);
    if (len >= origin_size) {
        return false;
    }

    memcpy(origin, url, len);
    origin[len] = '\0';
    return true;
}

static bool config_web_service_build_absolute_url(const char *base_url,
                                                  const char *link,
                                                  char *out,
                                                  size_t out_size)
{
    if ((base_url == NULL) || (link == NULL) || (out == NULL) || (out_size == 0U) || (link[0] == '\0')) {
        return false;
    }

    if (config_web_service_is_absolute_url(link)) {
        return snprintf(out, out_size, "%s", link) < (int)out_size;
    }

    char origin[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
    if (!config_web_service_get_url_origin(base_url, origin, sizeof(origin))) {
        return false;
    }

    if (config_web_service_string_starts_with(link, "//")) {
        const char *scheme_end = strstr(base_url, "://");
        if (scheme_end == NULL) {
            return false;
        }
        size_t scheme_len = (size_t)(scheme_end - base_url);
        return snprintf(out, out_size, "%.*s:%s", (int)scheme_len, base_url, link) < (int)out_size;
    }

    if (link[0] == '/') {
        return snprintf(out, out_size, "%s%s", origin, link) < (int)out_size;
    }

    char current_dir[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
    if (snprintf(current_dir, sizeof(current_dir), "%s", base_url) >= (int)sizeof(current_dir)) {
        return false;
    }

    char *query = strchr(current_dir, '?');
    if (query != NULL) {
        *query = '\0';
    }
    char *last_slash = strrchr(current_dir, '/');
    const char *origin_path = strstr(current_dir, "://");
    if ((last_slash == NULL) || ((origin_path != NULL) && (last_slash < (origin_path + 3)))) {
        return snprintf(out, out_size, "%s/%s", origin, link) < (int)out_size;
    }
    last_slash[1] = '\0';
    return snprintf(out, out_size, "%s%s", current_dir, link) < (int)out_size;
}

static bool config_web_service_proxy_url_for(const char *absolute_url, char *out, size_t out_size)
{
    char encoded[CONFIG_WEB_SERVICE_PROXY_URL_LEN * 3U];
    if (!config_web_service_url_encode(absolute_url, encoded, sizeof(encoded))) {
        return false;
    }
    return snprintf(out, out_size, "/portal/proxy?url=%s", encoded) < (int)out_size;
}

static void config_web_service_store_cookie(const char *set_cookie)
{
    if ((set_cookie == NULL) || (set_cookie[0] == '\0')) {
        return;
    }

    char pair[160];
    size_t len = strcspn(set_cookie, ";");
    if (len >= sizeof(pair)) {
        len = sizeof(pair) - 1U;
    }
    memcpy(pair, set_cookie, len);
    pair[len] = '\0';
    if (strchr(pair, '=') == NULL) {
        return;
    }

    char name[80];
    size_t name_len = (size_t)(strchr(pair, '=') - pair);
    if (name_len >= sizeof(name)) {
        name_len = sizeof(name) - 1U;
    }
    memcpy(name, pair, name_len);
    name[name_len] = '\0';

    char rebuilt[CONFIG_WEB_SERVICE_PROXY_COOKIE_LEN] = {0};
    char existing[CONFIG_WEB_SERVICE_PROXY_COOKIE_LEN];
    snprintf(existing, sizeof(existing), "%s", s_portal_proxy_cookie);
    char *cursor = existing;
    while ((cursor != NULL) && (*cursor != '\0')) {
        while (*cursor == ' ') {
            cursor++;
        }
        char *next = strstr(cursor, "; ");
        if (next != NULL) {
            *next = '\0';
            next += 2;
        }

        if ((strncmp(cursor, name, name_len) != 0) || (cursor[name_len] != '=')) {
            if (rebuilt[0] != '\0') {
                strlcat(rebuilt, "; ", sizeof(rebuilt));
            }
            strlcat(rebuilt, cursor, sizeof(rebuilt));
        }
        cursor = next;
    }

    if (rebuilt[0] != '\0') {
        strlcat(rebuilt, "; ", sizeof(rebuilt));
    }
    strlcat(rebuilt, pair, sizeof(rebuilt));
    snprintf(s_portal_proxy_cookie, sizeof(s_portal_proxy_cookie), "%s", rebuilt);
}

static bool config_web_service_append_text(char *out, size_t out_size, size_t *offset, const char *text, size_t len)
{
    if ((out == NULL) || (offset == NULL) || ((*offset + len + 1U) > out_size)) {
        return false;
    }

    memcpy(out + *offset, text, len);
    *offset += len;
    out[*offset] = '\0';
    return true;
}

static bool config_web_service_try_rewrite_attr(const char *html,
                                                size_t html_len,
                                                size_t *cursor,
                                                const char *base_url,
                                                char *out,
                                                size_t out_size,
                                                size_t *out_offset)
{
    static const char *attrs[] = {"href=", "src=", "action="};

    for (size_t i = 0; i < (sizeof(attrs) / sizeof(attrs[0])); ++i) {
        const char *attr = attrs[i];
        size_t attr_len = strlen(attr);
        if ((*cursor + attr_len + 1U) >= html_len) {
            continue;
        }
        if (strncasecmp(html + *cursor, attr, attr_len) != 0) {
            continue;
        }

        char quote = html[*cursor + attr_len];
        if ((quote != '\'') && (quote != '"')) {
            continue;
        }

        size_t value_start = *cursor + attr_len + 1U;
        size_t value_end = value_start;
        while ((value_end < html_len) && (html[value_end] != quote)) {
            value_end++;
        }
        if (value_end >= html_len) {
            return false;
        }

        size_t value_len = value_end - value_start;
        if ((value_len == 0U) || (value_len >= CONFIG_WEB_SERVICE_PROXY_URL_LEN)) {
            return false;
        }

        char link[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
        memcpy(link, html + value_start, value_len);
        link[value_len] = '\0';
        if (config_web_service_string_starts_with(link, "#")
            || config_web_service_string_starts_with(link, "javascript:")
            || config_web_service_string_starts_with(link, "data:")
            || config_web_service_string_starts_with(link, "mailto:")) {
            return false;
        }

        char absolute[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
        char proxied[CONFIG_WEB_SERVICE_PROXY_URL_LEN * 3U];
        if (!config_web_service_build_absolute_url(base_url, link, absolute, sizeof(absolute))
            || !config_web_service_proxy_url_for(absolute, proxied, sizeof(proxied))) {
            return false;
        }

        if (!config_web_service_append_text(out, out_size, out_offset, html + *cursor, attr_len + 1U)
            || !config_web_service_append_text(out, out_size, out_offset, proxied, strlen(proxied))
            || !config_web_service_append_text(out, out_size, out_offset, &quote, 1U)) {
            return false;
        }

        *cursor = value_end + 1U;
        return true;
    }

    return false;
}

static char *config_web_service_rewrite_html(const char *html, size_t html_len, const char *base_url)
{
    size_t out_size = (html_len * 3U) + 1024U;
    if (out_size > (CONFIG_WEB_SERVICE_PROXY_BODY_LIMIT * 4U)) {
        return NULL;
    }

    char *out = calloc(1U, out_size);
    if (out == NULL) {
        return NULL;
    }

    size_t out_offset = 0U;
    size_t cursor = 0U;
    while (cursor < html_len) {
        if (config_web_service_try_rewrite_attr(html,
                                                html_len,
                                                &cursor,
                                                base_url,
                                                out,
                                                out_size,
                                                &out_offset)) {
            continue;
        }
        if (!config_web_service_append_text(out, out_size, &out_offset, html + cursor, 1U)) {
            free(out);
            return NULL;
        }
        cursor++;
    }

    return out;
}

static esp_err_t config_web_service_read_body(httpd_req_t *req, char **out_body)
{
    if ((req == NULL) || (out_body == NULL) || (req->content_len > CONFIG_WEB_SERVICE_BODY_LIMIT)) {
        return ESP_ERR_INVALID_ARG;
    }

    char *body = calloc(1U, req->content_len + 1U);
    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int received = 0;
    while (received < (int)req->content_len) {
        int ret = httpd_req_recv(req, body + received, req->content_len - received);
        if (ret <= 0) {
            free(body);
            return ESP_FAIL;
        }
        received += ret;
    }

    body[received] = '\0';
    *out_body = body;
    return ESP_OK;
}

static bool config_web_service_find_form_value(const char *body,
                                               const char *key,
                                               char *out,
                                               size_t out_size)
{
    if ((body == NULL) || (key == NULL) || (out == NULL) || (out_size == 0U)) {
        return false;
    }

    size_t key_len = strlen(key);
    const char *cursor = body;
    while ((cursor != NULL) && (*cursor != '\0')) {
        const char *pair_end = strchr(cursor, '&');
        size_t pair_len = (pair_end != NULL) ? (size_t)(pair_end - cursor) : strlen(cursor);
        if ((pair_len > key_len) && (strncmp(cursor, key, key_len) == 0) && (cursor[key_len] == '=')) {
            size_t value_len = pair_len - key_len - 1U;
            if (value_len >= out_size) {
                value_len = out_size - 1U;
            }
            memcpy(out, cursor + key_len + 1U, value_len);
            out[value_len] = '\0';
            config_web_service_url_decode(out);
            return true;
        }
        cursor = (pair_end != NULL) ? (pair_end + 1) : NULL;
    }

    return false;
}

static esp_err_t config_web_service_write_json_config(httpd_req_t *req)
{
    app_config_t config = {0};
    app_config_get(&config);

    char *payload = calloc(1U, 4096U);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }
    snprintf(payload,
             4096U,
             "{"
             "\"wifi_ssid\":\"%s\","
             "\"wifi_password\":\"%s\","
             "\"wifi_hostname\":\"%s\","
             "\"wifi_security\":\"%s\","
             "\"wifi_eap_identity\":\"%s\","
             "\"wifi_eap_username\":\"%s\","
             "\"wifi_eap_password\":\"%s\","
             "\"wifi_portal_enabled\":%s,"
             "\"wifi_portal_url\":\"%s\","
             "\"wifi_portal_username\":\"%s\","
             "\"wifi_portal_password\":\"%s\","
             "\"config_ap_enabled\":%s,"
             "\"config_ap_ssid\":\"%s\","
             "\"config_ap_password\":\"%s\","
             "\"provider_kind\":\"%s\","
             "\"provider_display_name\":\"%s\","
             "\"provider_base_url\":\"%s\","
             "\"provider_endpoint_path\":\"%s\","
             "\"provider_access_token\":\"%s\","
             "\"provider_management_key\":\"%s\","
             "\"provider_user_header_name\":\"%s\","
             "\"provider_user_header_value\":\"%s\","
             "\"provider_refresh_interval_ms\":%" PRIu32 ","
             "\"ui_refresh_interval_ms\":%" PRIu32
             "}",
             config.wifi.ssid,
             config.wifi.password,
             config.wifi.hostname,
             app_config_wifi_security_to_string(config.wifi.security),
             config.wifi.eap_identity,
             config.wifi.eap_username,
             config.wifi.eap_password,
             config.wifi.portal_enabled ? "true" : "false",
             config.wifi.portal_url,
             config.wifi.portal_username,
             config.wifi.portal_password,
             config.config_ap.enabled ? "true" : "false",
             config.config_ap.ssid,
             config.config_ap.password,
             app_config_provider_kind_to_string(config.provider.kind),
             config.provider.display_name,
             config.provider.base_url,
             config.provider.endpoint_path,
             config.provider.access_token,
             config.provider.management_key,
             config.provider.user_header_name,
             config.provider.user_header_value,
             config.provider.refresh_interval_ms,
             config.ui_refresh_interval_ms);

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, payload);
    free(payload);
    return err;
}

static esp_err_t config_web_service_write_json_status(httpd_req_t *req)
{
    network_service_snapshot_t network = {0};
    provider_service_snapshot_t provider = {0};
    network_service_get_snapshot(&network);
    provider_service_get_snapshot(&provider);

    char *payload = calloc(1U, 1536U);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }
    snprintf(payload,
             1536U,
             "{"
             "\"network_state\":\"%s\","
             "\"network_ip\":\"%s\","
             "\"portal_state\":\"%s\","
             "\"provider_state\":\"%s\","
             "\"provider_status\":\"%s\""
             "}",
             network.state_text,
             network.ip,
             (network.portal_state == NETWORK_SERVICE_PORTAL_STATE_COMPLETED) ? "completed"
                 : (network.portal_state == NETWORK_SERVICE_PORTAL_STATE_REQUIRED) ? "required"
                 : (network.portal_state == NETWORK_SERVICE_PORTAL_STATE_WAITING) ? "waiting"
                 : "disabled",
             provider.state_text,
             provider.status_text);

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, payload);
    free(payload);
    return err;
}

static esp_err_t config_web_service_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req, CONFIG_WEB_SERVICE_HTML);
}

static esp_err_t config_web_service_get_config_handler(httpd_req_t *req)
{
    return config_web_service_write_json_config(req);
}

static esp_err_t config_web_service_get_status_handler(httpd_req_t *req)
{
    return config_web_service_write_json_status(req);
}

typedef struct {
    char content_type[CONFIG_WEB_SERVICE_PROXY_CONTENT_TYPE_LEN];
    char location[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
} config_web_service_proxy_event_t;

static esp_err_t config_web_service_proxy_http_event_handler(esp_http_client_event_t *event)
{
    if ((event == NULL) || (event->event_id != HTTP_EVENT_ON_HEADER) || (event->user_data == NULL)
        || (event->header_key == NULL) || (event->header_value == NULL)) {
        return ESP_OK;
    }

    config_web_service_proxy_event_t *proxy_event = (config_web_service_proxy_event_t *)event->user_data;
    if (config_web_service_same_text_ci(event->header_key, "Content-Type")) {
        snprintf(proxy_event->content_type, sizeof(proxy_event->content_type), "%s", event->header_value);
    } else if (config_web_service_same_text_ci(event->header_key, "Location")) {
        snprintf(proxy_event->location, sizeof(proxy_event->location), "%s", event->header_value);
    } else if (config_web_service_same_text_ci(event->header_key, "Set-Cookie")) {
        config_web_service_store_cookie(event->header_value);
    }

    return ESP_OK;
}

static esp_err_t config_web_service_get_proxy_target(httpd_req_t *req, char *target, size_t target_size)
{
    char query[CONFIG_WEB_SERVICE_PROXY_URL_LEN * 3U];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    if (httpd_query_key_value(query, "url", target, target_size) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    config_web_service_url_decode(target);
    return config_web_service_is_absolute_url(target) ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static esp_err_t config_web_service_send_proxy_redirect(httpd_req_t *req, const char *base_url, const char *location)
{
    char absolute[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
    char proxied[CONFIG_WEB_SERVICE_PROXY_URL_LEN * 3U];
    if (!config_web_service_build_absolute_url(base_url, location, absolute, sizeof(absolute))
        || !config_web_service_proxy_url_for(absolute, proxied, sizeof(proxied))) {
        httpd_resp_set_status(req, "502 Bad Gateway");
        return httpd_resp_sendstr(req, "Portal redirect URL is too large.");
    }

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", proxied);
    return httpd_resp_sendstr(req, "Redirecting through device portal proxy.");
}

static esp_err_t config_web_service_send_proxy_body(httpd_req_t *req,
                                                   const char *target_url,
                                                   const char *content_type,
                                                   const char *body,
                                                   size_t body_len)
{
    bool is_html = (content_type != NULL) && (strstr(content_type, "text/html") != NULL);
    if (is_html) {
        char *rewritten = config_web_service_rewrite_html(body, body_len, target_url);
        if (rewritten == NULL) {
            httpd_resp_set_status(req, "502 Bad Gateway");
            return httpd_resp_sendstr(req, "Portal page is too large to rewrite.");
        }
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        esp_err_t err = httpd_resp_sendstr(req, rewritten);
        free(rewritten);
        return err;
    }

    if ((content_type != NULL) && (content_type[0] != '\0')) {
        httpd_resp_set_type(req, content_type);
    } else {
        httpd_resp_set_type(req, "application/octet-stream");
    }
    return httpd_resp_send(req, body, body_len);
}

static esp_err_t config_web_service_portal_proxy_handler(httpd_req_t *req)
{
    char target_url[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
    esp_err_t err = config_web_service_get_proxy_target(req, target_url, sizeof(target_url));
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "Missing or invalid portal proxy target URL.");
    }

    char *request_body = NULL;
    int write_len = 0;
    if (req->method == HTTP_POST) {
        ESP_RETURN_ON_ERROR(config_web_service_read_body(req, &request_body), TAG, "read portal proxy body failed");
        write_len = (int)strlen(request_body);
    }

    config_web_service_proxy_event_t proxy_event = {0};
    esp_http_client_config_t client_config = {
        .url = target_url,
        .method = (req->method == HTTP_POST) ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = CONFIG_WEB_SERVICE_PROXY_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = config_web_service_proxy_http_event_handler,
        .user_data = &proxy_event,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .max_redirection_count = 5,
    };

    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    if (client == NULL) {
        free(request_body);
        httpd_resp_set_status(req, "502 Bad Gateway");
        return httpd_resp_sendstr(req, "Portal proxy HTTP client init failed.");
    }

    if (s_portal_proxy_cookie[0] != '\0') {
        esp_http_client_set_header(client, "Cookie", s_portal_proxy_cookie);
    }
    esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0 ESP32-AI-Monitor-Portal-Proxy");
    esp_http_client_set_header(client, "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    if (req->method == HTTP_POST) {
        char content_type[96] = "application/x-www-form-urlencoded";
        if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK) {
            snprintf(content_type, sizeof(content_type), "application/x-www-form-urlencoded");
        }
        esp_http_client_set_header(client, "Content-Type", content_type);
    }

    err = esp_http_client_open(client, write_len);
    if ((err == ESP_OK) && (request_body != NULL) && (write_len > 0)) {
        int written = esp_http_client_write(client, request_body, write_len);
        if (written != write_len) {
            err = ESP_FAIL;
        }
    }
    free(request_body);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Portal proxy open/write failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        httpd_resp_set_status(req, "502 Bad Gateway");
        return httpd_resp_sendstr(req, "Portal proxy request failed before response.");
    }

    int64_t content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    char final_url[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
    if (esp_http_client_get_url(client, final_url, sizeof(final_url)) != ESP_OK) {
        snprintf(final_url, sizeof(final_url), "%s", target_url);
    }
    snprintf(s_portal_proxy_last_url, sizeof(s_portal_proxy_last_url), "%s", final_url);

    if ((status_code >= 300) && (status_code < 400) && (proxy_event.location[0] != '\0')) {
        esp_http_client_cleanup(client);
        return config_web_service_send_proxy_redirect(req, final_url, proxy_event.location);
    }

    if (content_length > (int64_t)CONFIG_WEB_SERVICE_PROXY_BODY_LIMIT) {
        esp_http_client_cleanup(client);
        httpd_resp_set_status(req, "502 Bad Gateway");
        return httpd_resp_sendstr(req, "Portal response is too large for the device proxy.");
    }

    char *response_body = calloc(1U, CONFIG_WEB_SERVICE_PROXY_BODY_LIMIT + 1U);
    if (response_body == NULL) {
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    size_t total_read = 0U;
    while (total_read < CONFIG_WEB_SERVICE_PROXY_BODY_LIMIT) {
        int read_len = esp_http_client_read(client,
                                            response_body + total_read,
                                            (int)(CONFIG_WEB_SERVICE_PROXY_BODY_LIMIT - total_read));
        if (read_len == -ESP_ERR_HTTP_EAGAIN) {
            continue;
        }
        if (read_len <= 0) {
            break;
        }
        total_read += (size_t)read_len;
    }
    esp_http_client_cleanup(client);

    if (total_read >= CONFIG_WEB_SERVICE_PROXY_BODY_LIMIT) {
        free(response_body);
        httpd_resp_set_status(req, "502 Bad Gateway");
        return httpd_resp_sendstr(req, "Portal response exceeded proxy buffer.");
    }
    response_body[total_read] = '\0';

    if ((status_code >= 400) && (status_code < 600)) {
        httpd_resp_set_status(req, "502 Bad Gateway");
    }
    err = config_web_service_send_proxy_body(req,
                                             final_url,
                                             proxy_event.content_type,
                                             response_body,
                                             total_read);
    free(response_body);
    return err;
}

static esp_err_t config_web_service_portal_open_handler(httpd_req_t *req)
{
    char target_url[CONFIG_WEB_SERVICE_PROXY_URL_LEN];
    esp_err_t err = config_web_service_get_proxy_target(req, target_url, sizeof(target_url));
    if (err != ESP_OK) {
        app_config_t config = {0};
        app_config_get(&config);
        snprintf(target_url,
                 sizeof(target_url),
                 "%s",
                 (config.wifi.portal_url[0] != '\0') ? config.wifi.portal_url : "http://1.1.1.1");
    }

    s_portal_proxy_cookie[0] = '\0';
    s_portal_proxy_last_url[0] = '\0';
    char proxied[CONFIG_WEB_SERVICE_PROXY_URL_LEN * 3U];
    if (!config_web_service_proxy_url_for(target_url, proxied, sizeof(proxied))) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "Portal URL is too large.");
    }

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", proxied);
    return httpd_resp_sendstr(req, "Opening portal through device proxy.");
}

static esp_err_t config_web_service_post_config_handler(httpd_req_t *req)
{
    char *body = NULL;
    ESP_RETURN_ON_ERROR(config_web_service_read_body(req, &body), TAG, "read body failed");

    app_config_t config = {0};
    app_config_get(&config);

    char value[256];
    if (config_web_service_find_form_value(body, "wifi_ssid", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.ssid, sizeof(config.wifi.ssid), value);
    }
    if (config_web_service_find_form_value(body, "wifi_password", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.password, sizeof(config.wifi.password), value);
    }
    if (config_web_service_find_form_value(body, "wifi_hostname", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.hostname, sizeof(config.wifi.hostname), value);
    }
    if (config_web_service_find_form_value(body, "wifi_security", value, sizeof(value))) {
        (void)app_config_wifi_security_from_string(value, &config.wifi.security);
    }
    if (config_web_service_find_form_value(body, "wifi_eap_identity", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.eap_identity, sizeof(config.wifi.eap_identity), value);
    }
    if (config_web_service_find_form_value(body, "wifi_eap_username", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.eap_username, sizeof(config.wifi.eap_username), value);
    }
    if (config_web_service_find_form_value(body, "wifi_eap_password", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.eap_password, sizeof(config.wifi.eap_password), value);
    }
    config.wifi.portal_enabled = config_web_service_find_form_value(body, "wifi_portal_enabled", value, sizeof(value));
    if (config_web_service_find_form_value(body, "wifi_portal_url", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.portal_url, sizeof(config.wifi.portal_url), value);
    }
    if (config_web_service_find_form_value(body, "wifi_portal_username", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.portal_username, sizeof(config.wifi.portal_username), value);
    }
    if (config_web_service_find_form_value(body, "wifi_portal_password", value, sizeof(value))) {
        config_web_service_copy_text(config.wifi.portal_password, sizeof(config.wifi.portal_password), value);
    }
    config.config_ap.enabled = true;
    if (config_web_service_find_form_value(body, "config_ap_ssid", value, sizeof(value))) {
        config_web_service_copy_text(config.config_ap.ssid, sizeof(config.config_ap.ssid), value);
    }
    if (config_web_service_find_form_value(body, "config_ap_password", value, sizeof(value))) {
        config_web_service_copy_text(config.config_ap.password, sizeof(config.config_ap.password), value);
    }
    if (config_web_service_find_form_value(body, "provider_kind", value, sizeof(value))) {
        (void)app_config_provider_kind_from_string(value, &config.provider.kind);
    }
    if (config_web_service_find_form_value(body, "provider_display_name", value, sizeof(value))) {
        config_web_service_copy_text(config.provider.display_name, sizeof(config.provider.display_name), value);
    }
    if (config_web_service_find_form_value(body, "provider_base_url", value, sizeof(value))) {
        config_web_service_copy_text(config.provider.base_url, sizeof(config.provider.base_url), value);
    }
    if (config_web_service_find_form_value(body, "provider_endpoint_path", value, sizeof(value))) {
        config_web_service_copy_text(config.provider.endpoint_path, sizeof(config.provider.endpoint_path), value);
    }
    if (config_web_service_find_form_value(body, "provider_access_token", value, sizeof(value))) {
        config_web_service_copy_text(config.provider.access_token, sizeof(config.provider.access_token), value);
    }
    if (config_web_service_find_form_value(body, "provider_management_key", value, sizeof(value))) {
        config_web_service_copy_text(config.provider.management_key, sizeof(config.provider.management_key), value);
    }
    if (config_web_service_find_form_value(body, "provider_user_header_name", value, sizeof(value))) {
        config_web_service_copy_text(config.provider.user_header_name, sizeof(config.provider.user_header_name), value);
    }
    if (config_web_service_find_form_value(body, "provider_user_header_value", value, sizeof(value))) {
        config_web_service_copy_text(config.provider.user_header_value, sizeof(config.provider.user_header_value), value);
    }
    if (config_web_service_find_form_value(body, "provider_refresh_interval_ms", value, sizeof(value))) {
        config.provider.refresh_interval_ms = (uint32_t)strtoul(value, NULL, 10);
    }
    if (config_web_service_find_form_value(body, "ui_refresh_interval_ms", value, sizeof(value))) {
        config.ui_refresh_interval_ms = (uint32_t)strtoul(value, NULL, 10);
    }

    free(body);

    char error_text[128];
    esp_err_t err = app_config_validate(&config, error_text, sizeof(error_text));
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, error_text);
    }

    ESP_RETURN_ON_ERROR(app_config_save(&config), TAG, "config save failed");
    ESP_RETURN_ON_ERROR(config_web_service_schedule_restart(), TAG, "restart schedule failed");
    return httpd_resp_sendstr(req, "Config saved. Device restarting to apply Wi-Fi / provider changes.");
}

static esp_err_t config_web_service_mark_portal_complete_handler(httpd_req_t *req)
{
    esp_err_t err = network_service_mark_portal_complete();
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "Portal state is not active.");
    }
    return httpd_resp_sendstr(req, "Portal marked as completed.");
}

static esp_err_t config_web_service_restart_handler(httpd_req_t *req)
{
    ESP_RETURN_ON_ERROR(httpd_resp_sendstr(req, "Restarting..."), TAG, "restart response send failed");
    ESP_RETURN_ON_ERROR(config_web_service_schedule_restart(), TAG, "restart schedule failed");
    return ESP_OK;
}

esp_err_t config_web_service_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    config.stack_size = 12288;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "httpd_start failed");

    static const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = config_web_service_root_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t config_get_uri = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = config_web_service_get_config_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t config_post_uri = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = config_web_service_post_config_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = config_web_service_get_status_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t portal_done_uri = {
        .uri = "/api/portal/complete",
        .method = HTTP_POST,
        .handler = config_web_service_mark_portal_complete_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t restart_uri = {
        .uri = "/api/restart",
        .method = HTTP_POST,
        .handler = config_web_service_restart_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t portal_open_uri = {
        .uri = "/portal/open",
        .method = HTTP_GET,
        .handler = config_web_service_portal_open_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t portal_proxy_uri = {
        .uri = "/portal/proxy*",
        .method = HTTP_ANY,
        .handler = config_web_service_portal_proxy_handler,
        .user_ctx = NULL,
    };
    static const httpd_uri_t captive_root_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = config_web_service_root_handler,
        .user_ctx = NULL,
    };

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &root_uri), TAG, "register root failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_get_uri), TAG, "register config GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_post_uri), TAG, "register config POST failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &status_uri), TAG, "register status failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &portal_done_uri), TAG, "register portal complete failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &restart_uri), TAG, "register restart failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &portal_open_uri), TAG, "register portal open failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &portal_proxy_uri), TAG, "register portal proxy failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &captive_root_uri), TAG, "register captive root failed");
    ESP_RETURN_ON_ERROR(config_web_service_start_captive_dns(), TAG, "captive DNS start failed");

    ESP_LOGI(TAG, "Configuration web service started on port %u", config.server_port);
    return ESP_OK;
}
