/**
 * @file    network_service.c
 * @brief   Wi-Fi STA 生命周期管理与连接详情采集。
 *
 * 本文件负责初始化 ESP-IDF Wi-Fi Station、监听连接事件、自动重连，
 * 并把 AP、IP、DNS、MAC 等详情收敛成 UI 可直接显示的快照。
 * 本文件不创建任何界面对象。
 */

#include "network_service.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "network_service";

static SemaphoreHandle_t s_state_lock;
static esp_netif_t *s_wifi_netif;
static bool s_wifi_started;
static network_service_snapshot_t s_snapshot;

static void network_service_copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0)) {
        return;
    }

    snprintf(dst, dst_size, "%s", (src != NULL) ? src : "-");
}

static void network_service_format_mac(char *buffer, size_t buffer_size, const uint8_t *mac)
{
    if ((buffer == NULL) || (buffer_size == 0)) {
        return;
    }

    if (mac == NULL) {
        network_service_copy_text(buffer, buffer_size, "-");
        return;
    }

    snprintf(buffer,
             buffer_size,
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);
}

static void network_service_format_ipv4(char *buffer, size_t buffer_size, const esp_ip4_addr_t *ip)
{
    if ((buffer == NULL) || (buffer_size == 0)) {
        return;
    }

    if ((ip == NULL) || (ip->addr == 0)) {
        network_service_copy_text(buffer, buffer_size, "-");
        return;
    }

    snprintf(buffer, buffer_size, IPSTR, IP2STR(ip));
}

static void network_service_format_dns(char *buffer, size_t buffer_size, const esp_netif_dns_info_t *dns_info)
{
    if ((dns_info == NULL) || (dns_info->ip.type != ESP_IPADDR_TYPE_V4)) {
        network_service_copy_text(buffer, buffer_size, "-");
        return;
    }

    network_service_format_ipv4(buffer, buffer_size, &dns_info->ip.u_addr.ip4);
}

static const char *network_service_auth_mode_to_string(wifi_auth_mode_t auth_mode)
{
    switch (auth_mode) {
    case WIFI_AUTH_OPEN:
        return "OPEN";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2/WPA3";
    default:
        return "UNKNOWN";
    }
}

static const char *network_service_cipher_to_string(wifi_cipher_type_t cipher)
{
    switch (cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        return "NONE";
    case WIFI_CIPHER_TYPE_WEP40:
        return "WEP40";
    case WIFI_CIPHER_TYPE_WEP104:
        return "WEP104";
    case WIFI_CIPHER_TYPE_TKIP:
        return "TKIP";
    case WIFI_CIPHER_TYPE_CCMP:
        return "CCMP";
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        return "TKIP/CCMP";
    case WIFI_CIPHER_TYPE_AES_CMAC128:
        return "AES-CMAC128";
    default:
        return "OTHER";
    }
}

static const char *network_service_second_channel_to_string(wifi_second_chan_t second_channel)
{
    switch (second_channel) {
    case WIFI_SECOND_CHAN_NONE:
        return "NONE";
    case WIFI_SECOND_CHAN_ABOVE:
        return "ABOVE";
    case WIFI_SECOND_CHAN_BELOW:
        return "BELOW";
    default:
        return "UNKNOWN";
    }
}

static void network_service_reset_runtime_fields_locked(void)
{
    network_service_copy_text(s_snapshot.connected_ssid, sizeof(s_snapshot.connected_ssid), "-");
    network_service_copy_text(s_snapshot.sta_mac, sizeof(s_snapshot.sta_mac), "-");
    network_service_copy_text(s_snapshot.bssid, sizeof(s_snapshot.bssid), "-");
    network_service_copy_text(s_snapshot.ip, sizeof(s_snapshot.ip), "-");
    network_service_copy_text(s_snapshot.netmask, sizeof(s_snapshot.netmask), "-");
    network_service_copy_text(s_snapshot.gateway, sizeof(s_snapshot.gateway), "-");
    network_service_copy_text(s_snapshot.dns_main, sizeof(s_snapshot.dns_main), "-");
    network_service_copy_text(s_snapshot.dns_backup, sizeof(s_snapshot.dns_backup), "-");
    network_service_copy_text(s_snapshot.second_channel, sizeof(s_snapshot.second_channel), "-");
    network_service_copy_text(s_snapshot.auth_mode, sizeof(s_snapshot.auth_mode), "-");
    network_service_copy_text(s_snapshot.pairwise_cipher, sizeof(s_snapshot.pairwise_cipher), "-");
    network_service_copy_text(s_snapshot.group_cipher, sizeof(s_snapshot.group_cipher), "-");
    s_snapshot.rssi = 0;
    s_snapshot.primary_channel = 0;
    s_snapshot.ip_ready = false;
}

static void network_service_set_state_locked(network_service_state_t state, const char *state_text, const char *status_text)
{
    s_snapshot.state = state;
    network_service_copy_text(s_snapshot.state_text, sizeof(s_snapshot.state_text), state_text);
    network_service_copy_text(s_snapshot.status_text, sizeof(s_snapshot.status_text), status_text);
}

static void network_service_bootstrap_state(void)
{
    if (s_state_lock != NULL) {
        return;
    }

    s_state_lock = xSemaphoreCreateMutex();
    if (s_state_lock == NULL) {
        return;
    }

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.credentials_ready = (strlen(CONFIG_AI_MONITOR_WIFI_SSID) > 0);
    s_snapshot.password_configured = (strlen(CONFIG_AI_MONITOR_WIFI_PASSWORD) > 0);
    s_snapshot.configured_password_length = (uint8_t)strlen(CONFIG_AI_MONITOR_WIFI_PASSWORD);
    network_service_copy_text(s_snapshot.configured_ssid, sizeof(s_snapshot.configured_ssid), CONFIG_AI_MONITOR_WIFI_SSID);
    network_service_copy_text(s_snapshot.hostname, sizeof(s_snapshot.hostname), CONFIG_AI_MONITOR_WIFI_HOSTNAME);
    network_service_reset_runtime_fields_locked();

    if (s_snapshot.credentials_ready) {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_IDLE, "IDLE", "Wi-Fi stack not started yet.");
    } else {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_UNCONFIGURED, "UNCONFIGURED", "Set Wi-Fi SSID in menuconfig.");
    }
    xSemaphoreGive(s_state_lock);
}

/**
 * @brief 调用前必须持有 s_state_lock。
 *
 * 该函数会跨 Wi-Fi 驱动和 netif 读取运行时状态；如果不在同一把锁里刷新，
 * 事件回调与 UI 定时刷新会交叉写入，导致屏幕短暂出现混合态字段。
 */
static void network_service_refresh_runtime_fields_locked(void)
{
    if (!s_wifi_started) {
        return;
    }

    uint8_t sta_mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_STA, sta_mac) == ESP_OK) {
        network_service_format_mac(s_snapshot.sta_mac, sizeof(s_snapshot.sta_mac), sta_mac);
    }

    if (s_wifi_netif != NULL) {
        const char *hostname = NULL;
        if ((esp_netif_get_hostname(s_wifi_netif, &hostname) == ESP_OK) && (hostname != NULL)) {
            network_service_copy_text(s_snapshot.hostname, sizeof(s_snapshot.hostname), hostname);
        }

        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_get_ip_info(s_wifi_netif, &ip_info) == ESP_OK) {
            network_service_format_ipv4(s_snapshot.ip, sizeof(s_snapshot.ip), &ip_info.ip);
            network_service_format_ipv4(s_snapshot.netmask, sizeof(s_snapshot.netmask), &ip_info.netmask);
            network_service_format_ipv4(s_snapshot.gateway, sizeof(s_snapshot.gateway), &ip_info.gw);
            s_snapshot.ip_ready = (ip_info.ip.addr != 0);
        }

        esp_netif_dns_info_t dns_main = {0};
        if (esp_netif_get_dns_info(s_wifi_netif, ESP_NETIF_DNS_MAIN, &dns_main) == ESP_OK) {
            network_service_format_dns(s_snapshot.dns_main, sizeof(s_snapshot.dns_main), &dns_main);
        }

        esp_netif_dns_info_t dns_backup = {0};
        if (esp_netif_get_dns_info(s_wifi_netif, ESP_NETIF_DNS_BACKUP, &dns_backup) == ESP_OK) {
            network_service_format_dns(s_snapshot.dns_backup, sizeof(s_snapshot.dns_backup), &dns_backup);
        }
    }

    wifi_ap_record_t ap_record = {0};
    if (esp_wifi_sta_get_ap_info(&ap_record) == ESP_OK) {
        network_service_copy_text(s_snapshot.connected_ssid,
                                  sizeof(s_snapshot.connected_ssid),
                                  (const char *)ap_record.ssid);
        network_service_format_mac(s_snapshot.bssid, sizeof(s_snapshot.bssid), ap_record.bssid);
        s_snapshot.rssi = ap_record.rssi;
        s_snapshot.primary_channel = ap_record.primary;
        network_service_copy_text(s_snapshot.second_channel,
                                  sizeof(s_snapshot.second_channel),
                                  network_service_second_channel_to_string(ap_record.second));
        network_service_copy_text(s_snapshot.auth_mode,
                                  sizeof(s_snapshot.auth_mode),
                                  network_service_auth_mode_to_string(ap_record.authmode));
        network_service_copy_text(s_snapshot.pairwise_cipher,
                                  sizeof(s_snapshot.pairwise_cipher),
                                  network_service_cipher_to_string(ap_record.pairwise_cipher));
        network_service_copy_text(s_snapshot.group_cipher,
                                  sizeof(s_snapshot.group_cipher),
                                  network_service_cipher_to_string(ap_record.group_cipher));
    }
}

static esp_err_t network_service_init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    return err;
}

static void network_service_wifi_event_handler(void *arg,
                                               esp_event_base_t event_base,
                                               int32_t event_id,
                                               void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == WIFI_EVENT_STA_START) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTING,
                                         "CONNECTING",
                                         "Station started. Connecting to configured AP.");
        xSemaphoreGive(s_state_lock);
        ESP_ERROR_CHECK(esp_wifi_connect());
        return;
    }

    if (event_id == WIFI_EVENT_STA_CONNECTED) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTING,
                                         "ASSOCIATED",
                                         "Associated with AP. Waiting for DHCP.");
        xSemaphoreGive(s_state_lock);
        return;
    }

    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected =
            (const wifi_event_sta_disconnected_t *)event_data;

        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        s_snapshot.reconnect_attempts += 1;
        s_snapshot.last_disconnect_reason = disconnected->reason;
        network_service_reset_runtime_fields_locked();
        network_service_set_state_locked(NETWORK_SERVICE_STATE_DISCONNECTED,
                                         "DISCONNECTED",
                                         "Link dropped. Auto reconnect scheduled.");
        xSemaphoreGive(s_state_lock);

        ESP_LOGW(TAG, "Disconnected from AP, reason=%u. Retrying.", disconnected->reason);
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

static void network_service_ip_event_handler(void *arg,
                                             esp_event_base_t event_base,
                                             int32_t event_id,
                                             void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_id;

    const ip_event_got_ip_t *got_ip = (const ip_event_got_ip_t *)event_data;

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTED,
                                     "CONNECTED",
                                     "DHCP lease acquired. Refreshing detail view.");
    network_service_format_ipv4(s_snapshot.ip, sizeof(s_snapshot.ip), &got_ip->ip_info.ip);
    network_service_format_ipv4(s_snapshot.netmask, sizeof(s_snapshot.netmask), &got_ip->ip_info.netmask);
    network_service_format_ipv4(s_snapshot.gateway, sizeof(s_snapshot.gateway), &got_ip->ip_info.gw);
    s_snapshot.ip_ready = true;
    network_service_refresh_runtime_fields_locked();
    xSemaphoreGive(s_state_lock);

    ESP_LOGI(TAG, "Connected to %s, IP=" IPSTR, s_snapshot.configured_ssid, IP2STR(&got_ip->ip_info.ip));
}

esp_err_t network_service_start(void)
{
    network_service_bootstrap_state();

    if (s_state_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    if (s_wifi_started) {
        xSemaphoreGive(s_state_lock);
        return ESP_OK;
    }
    xSemaphoreGive(s_state_lock);

    if (!s_snapshot.credentials_ready) {
        ESP_LOGW(TAG, "Wi-Fi SSID is empty. Configure it in menuconfig.");
        return ESP_OK;
    }

    wifi_config_t wifi_config = {0};
    if (strlen(CONFIG_AI_MONITOR_WIFI_SSID) >= sizeof(wifi_config.sta.ssid)) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        network_service_set_state_locked(NETWORK_SERVICE_STATE_UNCONFIGURED,
                                         "INVALID SSID",
                                         "SSID exceeds 32-byte Wi-Fi limit.");
        xSemaphoreGive(s_state_lock);
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(CONFIG_AI_MONITOR_WIFI_PASSWORD) >= sizeof(wifi_config.sta.password)) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
        network_service_set_state_locked(NETWORK_SERVICE_STATE_UNCONFIGURED,
                                         "INVALID PASS",
                                         "Password exceeds 64-byte Wi-Fi limit.");
        xSemaphoreGive(s_state_lock);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = network_service_init_nvs();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_loop_create_default();
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
        return err;
    }

    s_wifi_netif = esp_netif_create_default_wifi_sta();
    if (s_wifi_netif == NULL) {
        return ESP_FAIL;
    }

    if (strlen(CONFIG_AI_MONITOR_WIFI_HOSTNAME) > 0) {
        ESP_ERROR_CHECK(esp_netif_set_hostname(s_wifi_netif, CONFIG_AI_MONITOR_WIFI_HOSTNAME));
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_service_wifi_event_handler, NULL),
                        TAG,
                        "wifi handler register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_service_ip_event_handler, NULL),
                        TAG,
                        "ip handler register failed");

    network_service_copy_text((char *)wifi_config.sta.ssid,
                              sizeof(wifi_config.sta.ssid),
                              CONFIG_AI_MONITOR_WIFI_SSID);
    network_service_copy_text((char *)wifi_config.sta.password,
                              sizeof(wifi_config.sta.password),
                              CONFIG_AI_MONITOR_WIFI_PASSWORD);
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "esp_wifi_set_mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "esp_wifi_set_config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    s_wifi_started = true;
    network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTING,
                                     "CONNECTING",
                                     "Wi-Fi stack ready. Waiting for station start event.");
    xSemaphoreGive(s_state_lock);

    return ESP_OK;
}

void network_service_get_snapshot(network_service_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    network_service_bootstrap_state();
    if (s_state_lock == NULL) {
        memset(out, 0, sizeof(*out));
        return;
    }

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    network_service_refresh_runtime_fields_locked();
    memcpy(out, &s_snapshot, sizeof(*out));
    xSemaphoreGive(s_state_lock);
}
