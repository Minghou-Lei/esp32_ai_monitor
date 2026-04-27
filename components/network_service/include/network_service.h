/**
 * @file    network_service.h
 * @brief   Wi-Fi STA 启动与状态快照接口。
 *
 * 本组件负责从 menuconfig 读取凭据、启动 Wi-Fi Station、处理重连，
 * 并把连接详情整理成可直接给 UI 使用的快照结构。
 * 本组件不直接操作 LVGL 或屏幕渲染。
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

typedef enum {
    NETWORK_SERVICE_STATE_UNCONFIGURED = 0,
    NETWORK_SERVICE_STATE_IDLE,
    NETWORK_SERVICE_STATE_CONNECTING,
    NETWORK_SERVICE_STATE_CONNECTED,
    NETWORK_SERVICE_STATE_DISCONNECTED,
} network_service_state_t;

typedef struct {
    network_service_state_t state;
    bool credentials_ready;
    bool password_configured;
    bool ip_ready;
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
    char sta_mac[NETWORK_SERVICE_MAC_LEN];
    char bssid[NETWORK_SERVICE_MAC_LEN];
    char ip[NETWORK_SERVICE_IP_LEN];
    char netmask[NETWORK_SERVICE_IP_LEN];
    char gateway[NETWORK_SERVICE_IP_LEN];
    char dns_main[NETWORK_SERVICE_IP_LEN];
    char dns_backup[NETWORK_SERVICE_IP_LEN];
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

#ifdef __cplusplus
}
#endif
