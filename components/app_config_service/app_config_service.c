/**
 * @file    app_config_service.c
 * @brief   应用级运行时配置的默认值装配、校验和 NVS 持久化实现。
 *
 * 本文件统一管理监控终端的配置模型，使网络、provider 和配置网页都从同一份
 * 结构读取参数，避免不同组件各自持有一份硬编码副本。
 */

#include "app_config_service.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *APP_CONFIG_NAMESPACE = "app_cfg";
static const char *APP_CONFIG_BLOB_KEY = "current";

static void app_config_copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0U)) {
        return;
    }

    snprintf(dst, dst_size, "%s", (src != NULL) ? src : "");
}

static esp_err_t app_config_ensure_nvs_ready(void)
{
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }

    return err;
}

static void app_config_build_defaults(app_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->schema_version = APP_CONFIG_SCHEMA_VERSION;

    app_config_copy_text(config->wifi.ssid, sizeof(config->wifi.ssid), CONFIG_AI_MONITOR_WIFI_SSID);
    app_config_copy_text(config->wifi.password, sizeof(config->wifi.password), CONFIG_AI_MONITOR_WIFI_PASSWORD);
    app_config_copy_text(config->wifi.hostname, sizeof(config->wifi.hostname), CONFIG_AI_MONITOR_WIFI_HOSTNAME);
    app_config_copy_text(config->wifi.eap_identity, sizeof(config->wifi.eap_identity), CONFIG_AI_MONITOR_WIFI_EAP_IDENTITY);
    app_config_copy_text(config->wifi.eap_username, sizeof(config->wifi.eap_username), CONFIG_AI_MONITOR_WIFI_EAP_USERNAME);
    app_config_copy_text(config->wifi.eap_password, sizeof(config->wifi.eap_password), CONFIG_AI_MONITOR_WIFI_EAP_PASSWORD);
#if CONFIG_AI_MONITOR_WIFI_PORTAL_ENABLED
    config->wifi.portal_enabled = true;
#else
    config->wifi.portal_enabled = false;
#endif
    app_config_copy_text(config->wifi.portal_url, sizeof(config->wifi.portal_url), CONFIG_AI_MONITOR_WIFI_PORTAL_URL);
    app_config_copy_text(config->wifi.portal_username,
                         sizeof(config->wifi.portal_username),
                         CONFIG_AI_MONITOR_WIFI_PORTAL_USERNAME);
    app_config_copy_text(config->wifi.portal_password,
                         sizeof(config->wifi.portal_password),
                         CONFIG_AI_MONITOR_WIFI_PORTAL_PASSWORD);

#if CONFIG_AI_MONITOR_WIFI_SECURITY_OPEN
    config->wifi.security = APP_CONFIG_WIFI_SECURITY_OPEN;
#elif CONFIG_AI_MONITOR_WIFI_SECURITY_WPA2_ENTERPRISE
    config->wifi.security = APP_CONFIG_WIFI_SECURITY_WPA2_ENTERPRISE;
#else
    config->wifi.security = APP_CONFIG_WIFI_SECURITY_WPA2_PSK;
#endif

#if CONFIG_AI_MONITOR_CONFIG_AP_ENABLED
    config->config_ap.enabled = true;
#else
    config->config_ap.enabled = false;
#endif
    app_config_copy_text(config->config_ap.ssid, sizeof(config->config_ap.ssid), CONFIG_AI_MONITOR_CONFIG_AP_SSID);
    app_config_copy_text(config->config_ap.password,
                         sizeof(config->config_ap.password),
                         CONFIG_AI_MONITOR_CONFIG_AP_PASSWORD);

    config->provider.kind = APP_CONFIG_PROVIDER_AQI;
    app_config_copy_text(config->provider.display_name,
                         sizeof(config->provider.display_name),
                         CONFIG_AI_MONITOR_PROVIDER_DISPLAY_NAME);
    app_config_copy_text(config->provider.base_url,
                         sizeof(config->provider.base_url),
                         CONFIG_AI_MONITOR_PROVIDER_BASE_URL);
    app_config_copy_text(config->provider.endpoint_path,
                         sizeof(config->provider.endpoint_path),
                         CONFIG_AI_MONITOR_PROVIDER_ENDPOINT_PATH);
    app_config_copy_text(config->provider.access_token,
                         sizeof(config->provider.access_token),
                         CONFIG_AI_MONITOR_PROVIDER_ACCESS_TOKEN);
    app_config_copy_text(config->provider.management_key,
                         sizeof(config->provider.management_key),
                         CONFIG_AI_MONITOR_PROVIDER_MANAGEMENT_KEY);
    app_config_copy_text(config->provider.user_header_name,
                         sizeof(config->provider.user_header_name),
                         CONFIG_AI_MONITOR_PROVIDER_USER_HEADER_NAME);
    app_config_copy_text(config->provider.user_header_value,
                         sizeof(config->provider.user_header_value),
                         CONFIG_AI_MONITOR_PROVIDER_USER_HEADER_VALUE);
    config->provider.refresh_interval_ms = CONFIG_AI_MONITOR_PROVIDER_REFRESH_MS;

    config->ui_refresh_interval_ms = CONFIG_AI_MONITOR_UI_REFRESH_MS;
}

static bool app_config_password_valid_for_ap(const char *password)
{
    size_t len = (password != NULL) ? strlen(password) : 0U;
    return (len == 0U) || ((len >= 8U) && (len < APP_CONFIG_PASSWORD_LEN));
}

static esp_err_t app_config_validate_text_length(const char *text,
                                                 size_t max_len,
                                                 const char *field_name,
                                                 char *error_text,
                                                 size_t error_text_size)
{
    size_t len = (text != NULL) ? strlen(text) : 0U;
    if (len >= max_len) {
        snprintf(error_text,
                 error_text_size,
                 "%s exceeds %u bytes.",
                 field_name,
                 (unsigned)(max_len - 1U));
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t app_config_validate(const app_config_t *config, char *error_text, size_t error_text_size)
{
    if ((config == NULL) || (error_text == NULL) || (error_text_size == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }

    error_text[0] = '\0';

    esp_err_t err = app_config_validate_text_length(config->wifi.ssid,
                                                    sizeof(config->wifi.ssid),
                                                    "Wi-Fi SSID",
                                                    error_text,
                                                    error_text_size);
    if (err != ESP_OK) {
        return err;
    }

    err = app_config_validate_text_length(config->wifi.password,
                                          sizeof(config->wifi.password),
                                          "Wi-Fi password",
                                          error_text,
                                          error_text_size);
    if (err != ESP_OK) {
        return err;
    }

    err = app_config_validate_text_length(config->wifi.hostname,
                                          sizeof(config->wifi.hostname),
                                          "Wi-Fi hostname",
                                          error_text,
                                          error_text_size);
    if (err != ESP_OK) {
        return err;
    }

    err = app_config_validate_text_length(config->wifi.portal_url,
                                          sizeof(config->wifi.portal_url),
                                          "Portal URL",
                                          error_text,
                                          error_text_size);
    if (err != ESP_OK) {
        return err;
    }

    err = app_config_validate_text_length(config->provider.base_url,
                                          sizeof(config->provider.base_url),
                                          "Provider base URL",
                                          error_text,
                                          error_text_size);
    if (err != ESP_OK) {
        return err;
    }

    err = app_config_validate_text_length(config->provider.endpoint_path,
                                          sizeof(config->provider.endpoint_path),
                                          "Provider endpoint path",
                                          error_text,
                                          error_text_size);
    if (err != ESP_OK) {
        return err;
    }

    if ((config->config_ap.enabled) && (config->config_ap.ssid[0] == '\0')) {
        snprintf(error_text, error_text_size, "Config AP SSID is required when AP fallback is enabled.");
        return ESP_ERR_INVALID_ARG;
    }

    if (!app_config_password_valid_for_ap(config->config_ap.password)) {
        snprintf(error_text,
                 error_text_size,
                 "Config AP password must be empty or 8-64 bytes.");
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->wifi.security == APP_CONFIG_WIFI_SECURITY_WPA2_ENTERPRISE)
        && (config->wifi.eap_username[0] == '\0')) {
        snprintf(error_text,
                 error_text_size,
                 "Enterprise Wi-Fi requires an EAP username.");
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->wifi.security == APP_CONFIG_WIFI_SECURITY_WPA2_ENTERPRISE)
        && (config->wifi.eap_password[0] == '\0')) {
        snprintf(error_text,
                 error_text_size,
                 "Enterprise Wi-Fi requires an EAP password.");
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->provider.refresh_interval_ms < 10000U) || (config->provider.refresh_interval_ms > 600000U)) {
        snprintf(error_text,
                 error_text_size,
                 "Provider refresh interval must stay within 10000-600000 ms.");
        return ESP_ERR_INVALID_ARG;
    }

    if ((config->ui_refresh_interval_ms < 250U) || (config->ui_refresh_interval_ms > 5000U)) {
        snprintf(error_text,
                 error_text_size,
                 "UI refresh interval must stay within 250-5000 ms.");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

void app_config_get(app_config_t *out)
{
    if (out == NULL) {
        return;
    }

    app_config_build_defaults(out);

    if (app_config_ensure_nvs_ready() != ESP_OK) {
        return;
    }

    nvs_handle_t handle = 0;
    if (nvs_open(APP_CONFIG_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    size_t blob_size = sizeof(*out);
    app_config_t persisted = {0};
    esp_err_t err = nvs_get_blob(handle, APP_CONFIG_BLOB_KEY, &persisted, &blob_size);
    nvs_close(handle);
    if ((err != ESP_OK) || (blob_size != sizeof(*out)) || (persisted.schema_version != APP_CONFIG_SCHEMA_VERSION)) {
        return;
    }

    *out = persisted;
}

esp_err_t app_config_save(const app_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_config_t normalized = *config;
    normalized.schema_version = APP_CONFIG_SCHEMA_VERSION;

    char error_text[96];
    ESP_RETURN_ON_ERROR(app_config_validate(&normalized, error_text, sizeof(error_text)),
                        "app_config",
                        "%s",
                        error_text);

    ESP_RETURN_ON_ERROR(app_config_ensure_nvs_ready(), "app_config", "nvs init failed");

    nvs_handle_t handle = 0;
    ESP_RETURN_ON_ERROR(nvs_open(APP_CONFIG_NAMESPACE, NVS_READWRITE, &handle), "app_config", "nvs open failed");
    esp_err_t err = nvs_set_blob(handle, APP_CONFIG_BLOB_KEY, &normalized, sizeof(normalized));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t app_config_reset_to_defaults(void)
{
    app_config_t defaults = {0};
    app_config_build_defaults(&defaults);
    return app_config_save(&defaults);
}

const char *app_config_wifi_security_to_string(app_config_wifi_security_t security)
{
    switch (security) {
    case APP_CONFIG_WIFI_SECURITY_OPEN:
        return "open";
    case APP_CONFIG_WIFI_SECURITY_WPA2_ENTERPRISE:
        return "wpa2-enterprise";
    case APP_CONFIG_WIFI_SECURITY_WPA2_PSK:
    default:
        return "wpa2-psk";
    }
}

bool app_config_wifi_security_from_string(const char *text, app_config_wifi_security_t *out_security)
{
    if ((text == NULL) || (out_security == NULL)) {
        return false;
    }

    if (strcmp(text, "open") == 0) {
        *out_security = APP_CONFIG_WIFI_SECURITY_OPEN;
        return true;
    }
    if (strcmp(text, "wpa2-enterprise") == 0) {
        *out_security = APP_CONFIG_WIFI_SECURITY_WPA2_ENTERPRISE;
        return true;
    }
    if (strcmp(text, "wpa2-psk") == 0) {
        *out_security = APP_CONFIG_WIFI_SECURITY_WPA2_PSK;
        return true;
    }

    return false;
}

const char *app_config_provider_kind_to_string(app_config_provider_kind_t kind)
{
    switch (kind) {
    case APP_CONFIG_PROVIDER_AQI:
        return "aqi";
    case APP_CONFIG_PROVIDER_NONE:
    default:
        return "none";
    }
}

bool app_config_provider_kind_from_string(const char *text, app_config_provider_kind_t *out_kind)
{
    if ((text == NULL) || (out_kind == NULL)) {
        return false;
    }

    if (strcmp(text, "aqi") == 0) {
        *out_kind = APP_CONFIG_PROVIDER_AQI;
        return true;
    }
    if (strcmp(text, "none") == 0) {
        *out_kind = APP_CONFIG_PROVIDER_NONE;
        return true;
    }

    return false;
}
