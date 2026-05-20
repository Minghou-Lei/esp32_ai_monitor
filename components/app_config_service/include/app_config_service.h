/**
 * @file    app_config_service.h
 * @brief   应用级运行时配置读写接口。
 *
 * 本组件负责维护监控终端的持久配置，包括 Wi-Fi、公司门户、配置热点和
 * provider 参数；缺省值来自 `sdkconfig.defaults` / `sdkconfig`，运行时覆盖
 * 通过 NVS 持久化。
 * 本组件不直接启动 Wi-Fi、HTTP 服务或 provider 轮询。
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_CONFIG_SCHEMA_VERSION                1U
#define APP_CONFIG_SSID_LEN                      33
#define APP_CONFIG_PASSWORD_LEN                  65
#define APP_CONFIG_HOSTNAME_LEN                  64
#define APP_CONFIG_EAP_TEXT_LEN                  128
#define APP_CONFIG_URL_LEN                       128
#define APP_CONFIG_PROVIDER_NAME_LEN             32
#define APP_CONFIG_PROVIDER_HEADER_NAME_LEN      32
#define APP_CONFIG_PROVIDER_HEADER_VALUE_LEN     64

typedef enum {
    APP_CONFIG_WIFI_SECURITY_OPEN = 0,
    APP_CONFIG_WIFI_SECURITY_WPA2_PSK,
    APP_CONFIG_WIFI_SECURITY_WPA2_ENTERPRISE,
} app_config_wifi_security_t;

typedef enum {
    APP_CONFIG_PROVIDER_NONE = 0,
    APP_CONFIG_PROVIDER_AQI,
} app_config_provider_kind_t;

typedef struct {
    char ssid[APP_CONFIG_SSID_LEN];
    char password[APP_CONFIG_PASSWORD_LEN];
    char hostname[APP_CONFIG_HOSTNAME_LEN];
    app_config_wifi_security_t security;
    char eap_identity[APP_CONFIG_EAP_TEXT_LEN];
    char eap_username[APP_CONFIG_EAP_TEXT_LEN];
    char eap_password[APP_CONFIG_EAP_TEXT_LEN];
    bool portal_enabled;
    char portal_url[APP_CONFIG_URL_LEN];
    char portal_username[APP_CONFIG_EAP_TEXT_LEN];
    char portal_password[APP_CONFIG_EAP_TEXT_LEN];
} app_config_wifi_t;

typedef struct {
    bool enabled;
    char ssid[APP_CONFIG_SSID_LEN];
    char password[APP_CONFIG_PASSWORD_LEN];
} app_config_ap_fallback_t;

typedef struct {
    app_config_provider_kind_t kind;
    char display_name[APP_CONFIG_PROVIDER_NAME_LEN];
    char base_url[APP_CONFIG_URL_LEN];
    char endpoint_path[APP_CONFIG_URL_LEN];
    char access_token[APP_CONFIG_EAP_TEXT_LEN];
    char management_key[APP_CONFIG_EAP_TEXT_LEN];
    char user_header_name[APP_CONFIG_PROVIDER_HEADER_NAME_LEN];
    char user_header_value[APP_CONFIG_PROVIDER_HEADER_VALUE_LEN];
    uint32_t refresh_interval_ms;
} app_config_provider_t;

typedef struct {
    uint32_t schema_version;
    app_config_wifi_t wifi;
    app_config_ap_fallback_t config_ap;
    app_config_provider_t provider;
    uint32_t ui_refresh_interval_ms;
} app_config_t;

/**
 * @brief 读取当前生效配置。
 *
 * @param[out] out 用于接收配置结构；如果 NVS 中没有有效覆盖，则返回基线默认值。
 */
void app_config_get(app_config_t *out);

/**
 * @brief 将完整配置写回 NVS。
 *
 * @param[in] config 待保存配置，必须先通过 `app_config_validate()`。
 *
 * @return
 *      - ESP_OK                保存成功
 *      - ESP_ERR_INVALID_ARG   配置为空或字段越界
 *      - Other                 NVS 初始化、写入或提交失败
 */
esp_err_t app_config_save(const app_config_t *config);

/**
 * @brief 重置为编译期默认配置并写回 NVS。
 */
esp_err_t app_config_reset_to_defaults(void);

/**
 * @brief 校验配置约束并给出可展示的错误文本。
 *
 * @note 该函数只校验长度、模式组合与热点密码等本地可判定约束，不验证
 *       外部网络、provider 凭据或门户页面是否真实可用。
 */
esp_err_t app_config_validate(const app_config_t *config, char *error_text, size_t error_text_size);

const char *app_config_wifi_security_to_string(app_config_wifi_security_t security);
bool app_config_wifi_security_from_string(const char *text, app_config_wifi_security_t *out_security);
const char *app_config_provider_kind_to_string(app_config_provider_kind_t kind);
bool app_config_provider_kind_from_string(const char *text, app_config_provider_kind_t *out_kind);

#ifdef __cplusplus
}
#endif
