/**
 * @file    network_service.h
 * @brief   Wi-Fi 接入与公司门户状态快照接口。
 *
 * 本组件负责根据运行时配置启动 `STA` / `SoftAP`、处理企业 Wi-Fi 认证、
 * 维护公司门户注册状态，并把网络详情整理成 UI 与配置网页可复用的快照。
 * 本组件不直接创建 LVGL 对象，也不持久化配置。
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_SERVICE_SSID_LEN           33
#define NETWORK_SERVICE_HOSTNAME_LEN       64
#define NETWORK_SERVICE_MAC_LEN            18
#define NETWORK_SERVICE_IP_LEN             16
#define NETWORK_SERVICE_SHORT_TEXT_LEN     24
#define NETWORK_SERVICE_MEDIUM_TEXT_LEN    32
#define NETWORK_SERVICE_STATUS_TEXT_LEN    96
#define NETWORK_SERVICE_URL_LEN            128

typedef enum {
    NETWORK_SERVICE_MODE_STA_ONLY = 0,
    NETWORK_SERVICE_MODE_APSTA_FALLBACK,
} network_service_mode_t;

typedef enum {
    NETWORK_SERVICE_STATE_UNCONFIGURED = 0,
    NETWORK_SERVICE_STATE_IDLE,
    NETWORK_SERVICE_STATE_CONNECTING,
    NETWORK_SERVICE_STATE_CONNECTED,
    NETWORK_SERVICE_STATE_DISCONNECTED,
    NETWORK_SERVICE_STATE_PORTAL_REQUIRED,
    NETWORK_SERVICE_STATE_CONFIG_AP,
} network_service_state_t;

typedef enum {
    NETWORK_SERVICE_PORTAL_STATE_DISABLED = 0,
    NETWORK_SERVICE_PORTAL_STATE_WAITING,
    NETWORK_SERVICE_PORTAL_STATE_REQUIRED,
    NETWORK_SERVICE_PORTAL_STATE_COMPLETED,
} network_service_portal_state_t;

typedef struct {
    network_service_state_t state;
    network_service_mode_t mode;
    bool credentials_ready;
    bool password_configured;
    bool ip_ready;
    bool softap_active;
    bool enterprise_auth_enabled;
    bool portal_enabled;
    network_service_portal_state_t portal_state;
    uint8_t configured_password_length;
    uint32_t reconnect_attempts;
    uint16_t last_disconnect_reason;
    int8_t rssi;
    uint8_t primary_channel;
    char state_text[NETWORK_SERVICE_SHORT_TEXT_LEN];
    char status_text[NETWORK_SERVICE_STATUS_TEXT_LEN];
    char configured_ssid[NETWORK_SERVICE_SSID_LEN];
    char connected_ssid[NETWORK_SERVICE_SSID_LEN];
    char hostname[NETWORK_SERVICE_HOSTNAME_LEN];
    char config_ap_ssid[NETWORK_SERVICE_SSID_LEN];
    char config_ap_password[NETWORK_SERVICE_STATUS_TEXT_LEN];
    char sta_mac[NETWORK_SERVICE_MAC_LEN];
    char ap_mac[NETWORK_SERVICE_MAC_LEN];
    char bssid[NETWORK_SERVICE_MAC_LEN];
    char ip[NETWORK_SERVICE_IP_LEN];
    char ap_ip[NETWORK_SERVICE_IP_LEN];
    char netmask[NETWORK_SERVICE_IP_LEN];
    char gateway[NETWORK_SERVICE_IP_LEN];
    char dns_main[NETWORK_SERVICE_IP_LEN];
    char dns_backup[NETWORK_SERVICE_IP_LEN];
    char portal_url[NETWORK_SERVICE_URL_LEN];
    char second_channel[NETWORK_SERVICE_SHORT_TEXT_LEN];
    char auth_mode[NETWORK_SERVICE_MEDIUM_TEXT_LEN];
    char pairwise_cipher[NETWORK_SERVICE_MEDIUM_TEXT_LEN];
    char group_cipher[NETWORK_SERVICE_MEDIUM_TEXT_LEN];
} network_service_snapshot_t;

/**
 * @brief 启动 Wi-Fi Station，并异步维护连接详情。
 *
 * @note 该函数会初始化 NVS、默认事件循环、默认 STA netif，并启动
 *       esp_wifi 状态机。建议仅在启动阶段调用一次。
 *
 * @return
 *      - ESP_OK                启动成功，或凭据未配置但状态页已进入可展示状态
 *      - ESP_ERR_INVALID_ARG   menuconfig 中的 SSID 或密码长度超过 Wi-Fi 驱动限制
 *      - Other                 ESP-IDF 子系统初始化失败
 */
esp_err_t network_service_start(void);

/**
 * @brief 读取一份线程安全的 Wi-Fi 状态快照。
 *
 * @param[out] out 用于接收状态快照的结构体，不能为空。
 *
 * @note 该函数会在持锁状态下补齐 MAC、IP、DNS、AP 信息，适合被 UI 周期性调用。
 */
void network_service_get_snapshot(network_service_snapshot_t *out);

/**
 * @brief 手动切换本地配置热点。
 *
 * 这个入口用于物理按键触发的配网流程。它在 SoftAP / APSTA 与非 AP 模式之间切换，
 * 不持久化配置，也不替代 `network_service_start()` 的初始化职责。
 */
esp_err_t network_service_toggle_config_ap(void);

/**
 * @brief 将公司门户状态标记为已完成。
 *
 * @note `1.1.1.1` 这类设备注册网页不是 Wi-Fi 鉴权的一部分，而是连上公司内网后
 *       的外网放行步骤；因此这个状态需要被 UI / 配置页显式推进，而不是塞回
 *       `esp_wifi` 的连接状态机里。
 */
esp_err_t network_service_mark_portal_complete(void);

#ifdef __cplusplus
}
#endif
