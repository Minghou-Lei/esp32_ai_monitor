/**
 * @file    config_web_service.c
 * @brief   板上配置网页与 REST 接口实现。
 *
 * 当前页面追求“在公司网络 bring-up 阶段就能可靠使用”：HTML 保持单文件、接口
 * 保持少量且可抓包，所有保存动作都走同一份 `app_config_service` 校验逻辑。
 */

#include "config_web_service.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config_service.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "network_service.h"
#include "provider_service.h"

static const char *TAG = "config_web_service";
static const size_t CONFIG_WEB_SERVICE_BODY_LIMIT = 4096U;

static httpd_handle_t s_server;
static esp_timer_handle_t s_restart_timer;

static void config_web_service_restart_callback(void *arg)
{
    (void)arg;
    esp_restart();
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
    "<h2>Config AP</h2>"
    "<label><input type='checkbox' name='config_ap_enabled' value='1' style='width:auto;margin-right:8px;'>Enable fallback config AP</label>"
    "<div class='row'><div><label>AP SSID</label><input name='config_ap_ssid'></div><div><label>AP Password</label><input name='config_ap_password' type='password'></div></div>"
    "<h2>Provider</h2>"
    "<div class='row'><div><label>Provider</label><select name='provider_kind'><option value='aqi'>aqi</option><option value='none'>none</option></select></div><div><label>Display Name</label><input name='provider_display_name'></div></div>"
    "<div class='row'><div><label>Base URL</label><input name='provider_base_url'></div><div><label>Endpoint Path</label><input name='provider_endpoint_path'></div></div>"
    "<div class='row'><div><label>Access Token</label><input name='provider_access_token' type='password'></div><div><label>Management Key</label><input name='provider_management_key' type='password'></div></div>"
    "<div class='row'><div><label>User Header Name</label><input name='provider_user_header_name'></div><div><label>User Header Value</label><input name='provider_user_header_value'></div></div>"
    "<div class='row'><div><label>Provider Refresh (ms)</label><input name='provider_refresh_interval_ms' type='number'></div><div><label>UI Refresh (ms)</label><input name='ui_refresh_interval_ms' type='number'></div></div>"
    "<div class='actions'><button type='submit'>Save Config</button><button type='button' class='secondary' id='portalDone'>Mark Portal Complete</button><button type='button' class='secondary' id='restartBtn'>Restart Device</button></div>"
    "<div class='hint'>Saving writes to NVS. Restart after changing Wi-Fi / provider wiring.</div>"
    "</form><div id='result' class='status'></div></section>"
    "<script>"
    "async function loadConfig(){try{const r=await fetch('/api/config'); const cfg=await r.json();"
    "for(const [k,v] of Object.entries(cfg)){const el=document.querySelector(`[name=\"${k}\"]`); if(!el) continue; if(el.type==='checkbox'){el.checked=!!v;} else {el.value=v ?? '';}}"
    "}catch(e){document.getElementById('result').textContent='Load config failed: '+e;}}"
    "async function loadStatus(){try{const r=await fetch('/api/status'); const status=await r.json();"
    "document.getElementById('runtime').textContent=`Network: ${status.network_state}\\nIP: ${status.network_ip}\\nPortal: ${status.portal_state}\\nProvider: ${status.provider_state}\\nProvider Status: ${status.provider_status}`;"
    "}catch(e){document.getElementById('runtime').textContent='Runtime status unavailable: '+e;}}"
    "async function load(){await loadConfig(); await loadStatus();}"
    "document.getElementById('cfgForm').addEventListener('submit', async (e)=>{e.preventDefault(); const body=new URLSearchParams(new FormData(e.target)); const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body}); document.getElementById('result').textContent=await r.text(); load();});"
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
    config.config_ap.enabled = config_web_service_find_form_value(body, "config_ap_enabled", value, sizeof(value));
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
    return httpd_resp_sendstr(req, "Config saved. Restart device to apply Wi-Fi / provider changes.");
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
    ESP_RETURN_ON_ERROR(httpd_resp_sendstr(req, "Restarting..."), TAG, "restart response send failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_once(s_restart_timer, 400000), TAG, "restart timer start failed");
    return ESP_OK;
}

esp_err_t config_web_service_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
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

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &root_uri), TAG, "register root failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_get_uri), TAG, "register config GET failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_post_uri), TAG, "register config POST failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &status_uri), TAG, "register status failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &portal_done_uri), TAG, "register portal complete failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &restart_uri), TAG, "register restart failed");

    ESP_LOGI(TAG, "Configuration web service started on port %u", config.server_port);
    return ESP_OK;
}
