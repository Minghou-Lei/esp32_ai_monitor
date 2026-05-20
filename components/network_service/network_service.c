/**
 * @file    network_service.c
 * @brief   Wi-Fi 接入、首配 AP 回退与公司门户状态管理实现。
 *
 * 本文件统一管理监控终端的无线接入路径：优先按运行时配置加入公司 / 家庭网络，
 * 必要时启用本地配置热点，并把企业认证与门户放行状态折叠成单一快照供 UI 与
 * 配置网页复用。
 */

#include "network_service.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_config_service.h"
#include "esp_check.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/ip4_addr.h"
#include "provider_service.h"

static const char *TAG = "network_service";
static const char *NETWORK_SERVICE_CONFIG_AP_FALLBACK_SSID = "ESP32-AI-Monitor-Setup";
static SemaphoreHandle_t s_state_lock;
static esp_netif_t *s_wifi_sta_netif;
static esp_netif_t *s_wifi_ap_netif;
static bool s_wifi_started;
static bool s_sta_handlers_registered;
static app_config_t s_config;
static network_service_snapshot_t s_snapshot;

static void network_service_copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0U)) {
        return;
    }

    snprintf(dst, dst_size, "%s", (src != NULL) ? src : "-");
}

static void network_service_format_mac(char *buffer, size_t buffer_size, const uint8_t *mac)
{
    if ((buffer == NULL) || (buffer_size == 0U)) {
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
    if ((buffer == NULL) || (buffer_size == 0U)) {
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

static void network_service_reset_runtime_fields_locked(bool keep_ap_fields)
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
    s_snapshot.ip_ready = false;
    s_snapshot.rssi = 0;
    s_snapshot.primary_channel = 0;

    if (!keep_ap_fields) {
        network_service_copy_text(s_snapshot.ap_mac, sizeof(s_snapshot.ap_mac), "-");
        network_service_copy_text(s_snapshot.ap_ip, sizeof(s_snapshot.ap_ip), "-");
        s_snapshot.softap_active = false;
    }
}

static void network_service_set_state_locked(network_service_state_t state, const char *state_text, const char *status_text)
{
    s_snapshot.state = state;
    network_service_copy_text(s_snapshot.state_text, sizeof(s_snapshot.state_text), state_text);
    network_service_copy_text(s_snapshot.status_text, sizeof(s_snapshot.status_text), status_text);
}

static wifi_mode_t network_service_get_non_ap_mode_locked(void)
{
    return s_snapshot.credentials_ready ? WIFI_MODE_STA : WIFI_MODE_NULL;
}

static void network_service_apply_config_locked(const app_config_t *config)
{
    s_config = *config;

    s_snapshot.mode = NETWORK_SERVICE_MODE_STA_ONLY;
    s_snapshot.credentials_ready = (s_config.wifi.ssid[0] != '\0');
    s_snapshot.password_configured = (s_config.wifi.password[0] != '\0')
                                     || (s_config.wifi.eap_password[0] != '\0');
    s_snapshot.enterprise_auth_enabled = (s_config.wifi.security == APP_CONFIG_WIFI_SECURITY_WPA2_ENTERPRISE);
    s_snapshot.portal_enabled = s_config.wifi.portal_enabled;
    s_snapshot.portal_state = s_config.wifi.portal_enabled ? NETWORK_SERVICE_PORTAL_STATE_WAITING
                                                           : NETWORK_SERVICE_PORTAL_STATE_DISABLED;
    s_snapshot.configured_password_length = (uint8_t)strlen(s_config.wifi.password);
    network_service_copy_text(s_snapshot.configured_ssid,
                              sizeof(s_snapshot.configured_ssid),
                              s_config.wifi.ssid);
    network_service_copy_text(s_snapshot.hostname,
                              sizeof(s_snapshot.hostname),
                              s_config.wifi.hostname);
    network_service_copy_text(s_snapshot.config_ap_ssid,
                              sizeof(s_snapshot.config_ap_ssid),
                              s_config.config_ap.ssid);
    network_service_copy_text(s_snapshot.config_ap_password,
                              sizeof(s_snapshot.config_ap_password),
                              s_config.config_ap.password);
    network_service_copy_text(s_snapshot.portal_url,
                              sizeof(s_snapshot.portal_url),
                              s_config.wifi.portal_url);
    network_service_reset_runtime_fields_locked(false);

    if (s_snapshot.credentials_ready) {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_IDLE, "IDLE", "Wi-Fi stack not started yet.");
    } else {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_UNCONFIGURED,
                                         "UNCONFIGURED",
                                         "Hold BOOT for 2 seconds to toggle setup Wi-Fi.");
    }
}

static esp_err_t network_service_ensure_base_services(void)
{
    if (s_state_lock != NULL) {
        return ESP_OK;
    }

    s_state_lock = xSemaphoreCreateMutex();
    if (s_state_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_loop_create_default();
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
        return err;
    }

    return ESP_OK;
}

static esp_netif_t *network_service_create_ap_netif_once(void)
{
    if (s_wifi_ap_netif != NULL) {
        return s_wifi_ap_netif;
    }

    s_wifi_ap_netif = esp_netif_create_default_wifi_ap();
    return s_wifi_ap_netif;
}

/**
 * @brief 在持锁状态下刷新 netif 运行时信息，避免 UI 读取到 STA / AP 混合态。
 *
 * `STA` 的 DHCP、企业认证和 `SoftAP` 的本地网段由不同事件驱动更新；如果 UI 在
 * 刷新期间看到半更新结构，就会把“内网已连通但门户未放行”误渲染成普通断网。
 */
static void network_service_refresh_runtime_fields_locked(void)
{
    if (!s_wifi_started) {
        return;
    }

    uint8_t mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        network_service_format_mac(s_snapshot.sta_mac, sizeof(s_snapshot.sta_mac), mac);
    }
    if (esp_wifi_get_mac(WIFI_IF_AP, mac) == ESP_OK) {
        network_service_format_mac(s_snapshot.ap_mac, sizeof(s_snapshot.ap_mac), mac);
    }

    if (s_wifi_sta_netif != NULL) {
        const char *hostname = NULL;
        if ((esp_netif_get_hostname(s_wifi_sta_netif, &hostname) == ESP_OK) && (hostname != NULL)) {
            network_service_copy_text(s_snapshot.hostname, sizeof(s_snapshot.hostname), hostname);
        }

        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_get_ip_info(s_wifi_sta_netif, &ip_info) == ESP_OK) {
            network_service_format_ipv4(s_snapshot.ip, sizeof(s_snapshot.ip), &ip_info.ip);
            network_service_format_ipv4(s_snapshot.netmask, sizeof(s_snapshot.netmask), &ip_info.netmask);
            network_service_format_ipv4(s_snapshot.gateway, sizeof(s_snapshot.gateway), &ip_info.gw);
            s_snapshot.ip_ready = (ip_info.ip.addr != 0);
        }

        esp_netif_dns_info_t dns_main = {0};
        if (esp_netif_get_dns_info(s_wifi_sta_netif, ESP_NETIF_DNS_MAIN, &dns_main) == ESP_OK) {
            network_service_format_dns(s_snapshot.dns_main, sizeof(s_snapshot.dns_main), &dns_main);
        }

        esp_netif_dns_info_t dns_backup = {0};
        if (esp_netif_get_dns_info(s_wifi_sta_netif, ESP_NETIF_DNS_BACKUP, &dns_backup) == ESP_OK) {
            network_service_format_dns(s_snapshot.dns_backup, sizeof(s_snapshot.dns_backup), &dns_backup);
        }
    }

    if (s_wifi_ap_netif != NULL) {
        esp_netif_ip_info_t ap_info = {0};
        if (esp_netif_get_ip_info(s_wifi_ap_netif, &ap_info) == ESP_OK) {
            network_service_format_ipv4(s_snapshot.ap_ip, sizeof(s_snapshot.ap_ip), &ap_info.ip);
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

static esp_err_t network_service_apply_enterprise_credentials(const app_config_wifi_t *wifi)
{
    const char *identity = (wifi->eap_identity[0] != '\0') ? wifi->eap_identity : wifi->eap_username;
    ESP_RETURN_ON_ERROR(esp_eap_client_set_identity((const unsigned char *)identity, (int)strlen(identity)),
                        TAG,
                        "esp_eap_client_set_identity failed");
    ESP_RETURN_ON_ERROR(esp_eap_client_set_username((const unsigned char *)wifi->eap_username,
                                                    (int)strlen(wifi->eap_username)),
                        TAG,
                        "esp_eap_client_set_username failed");
    ESP_RETURN_ON_ERROR(esp_eap_client_set_password((const unsigned char *)wifi->eap_password,
                                                    (int)strlen(wifi->eap_password)),
                        TAG,
                        "esp_eap_client_set_password failed");
    ESP_RETURN_ON_ERROR(esp_wifi_sta_enterprise_enable(), TAG, "esp_wifi_sta_enterprise_enable failed");
    return ESP_OK;
}

static void network_service_clear_enterprise_credentials(void)
{
    /*
     * 当前 ESP-Hosted + C6 固件组合在未启用企业认证时，对 EAP clear RPC 的响应不稳定；
     * 非企业网络下直接跳过这些清理，避免启动阶段卡死在不必要的远端调用上。
     */
}

static esp_err_t network_service_start_config_ap_locked(void)
{
    const char *ap_ssid = (s_config.config_ap.ssid[0] != '\0') ? s_config.config_ap.ssid
                                                              : NETWORK_SERVICE_CONFIG_AP_FALLBACK_SSID;

    wifi_config_t ap_config = {0};
    network_service_copy_text((char *)ap_config.ap.ssid,
                              sizeof(ap_config.ap.ssid),
                              ap_ssid);
    network_service_copy_text(s_snapshot.config_ap_ssid,
                              sizeof(s_snapshot.config_ap_ssid),
                              ap_ssid);
    network_service_copy_text(s_snapshot.config_ap_password,
                              sizeof(s_snapshot.config_ap_password),
                              s_config.config_ap.password);
    network_service_copy_text((char *)ap_config.ap.password,
                              sizeof(ap_config.ap.password),
                              s_config.config_ap.password);
    ap_config.ap.ssid_len = (uint8_t)strlen(ap_ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = (s_config.config_ap.password[0] != '\0') ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    if (network_service_create_ap_netif_once() == NULL) {
        return ESP_FAIL;
    }
    esp_err_t err = esp_wifi_set_mode(s_snapshot.credentials_ready ? WIFI_MODE_APSTA : WIFI_MODE_AP);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (err != ESP_OK) {
        return err;
    }
    s_snapshot.softap_active = true;
    s_snapshot.mode = NETWORK_SERVICE_MODE_APSTA_FALLBACK;
    ESP_LOGI(TAG,
             "Config AP active: ssid=%s password_length=%u",
             s_snapshot.config_ap_ssid,
             (unsigned)strlen(s_snapshot.config_ap_password));
    return ESP_OK;
}

static esp_err_t network_service_stop_config_ap_locked(void)
{
    if (!s_snapshot.softap_active) {
        return ESP_OK;
    }

    esp_err_t err = esp_wifi_set_mode(network_service_get_non_ap_mode_locked());
    if (err != ESP_OK) {
        return err;
    }

    s_snapshot.softap_active = false;
    s_snapshot.mode = NETWORK_SERVICE_MODE_STA_ONLY;
    network_service_copy_text(s_snapshot.ap_ip, sizeof(s_snapshot.ap_ip), "-");

    if (s_snapshot.credentials_ready && s_snapshot.ip_ready) {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTED,
                                         "CONNECTED",
                                         "Config AP closed. Station network ready.");
    } else if (s_snapshot.credentials_ready) {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTING,
                                         "CONNECTING",
                                         "Config AP closed. Reconnecting station.");
    } else {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_UNCONFIGURED,
                                         "UNCONFIGURED",
                                         "Config AP closed. Hold BOOT to reopen setup.");
    }

    ESP_LOGI(TAG, "Config AP closed by button toggle");
    return ESP_OK;
}

static void network_service_wifi_event_handler(void *arg,
                                               esp_event_base_t event_base,
                                               int32_t event_id,
                                               void *event_data)
{
    (void)arg;
    (void)event_base;

    xSemaphoreTake(s_state_lock, portMAX_DELAY);

    if (event_id == WIFI_EVENT_STA_START) {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTING,
                                         "CONNECTING",
                                         "Station started. Connecting to configured AP.");
        xSemaphoreGive(s_state_lock);
        ESP_ERROR_CHECK(esp_wifi_connect());
        return;
    }

    if (event_id == WIFI_EVENT_STA_CONNECTED) {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTING,
                                         "ASSOCIATED",
                                         "Associated with AP. Waiting for DHCP.");
        xSemaphoreGive(s_state_lock);
        return;
    }

    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected =
            (const wifi_event_sta_disconnected_t *)event_data;
        s_snapshot.reconnect_attempts += 1;
        s_snapshot.last_disconnect_reason = disconnected->reason;
        network_service_reset_runtime_fields_locked(s_snapshot.softap_active);

        if (!s_snapshot.credentials_ready) {
            network_service_set_state_locked(NETWORK_SERVICE_STATE_UNCONFIGURED,
                                             "UNCONFIGURED",
                                             "Hold BOOT for 2 seconds to toggle setup Wi-Fi.");
        } else {
            network_service_set_state_locked(NETWORK_SERVICE_STATE_DISCONNECTED,
                                             "DISCONNECTED",
                                             "Link dropped. Auto reconnect scheduled.");
        }
        xSemaphoreGive(s_state_lock);

        ESP_LOGW(TAG, "Disconnected from AP, reason=%u. Retrying.", disconnected->reason);
        ESP_ERROR_CHECK(esp_wifi_connect());
        return;
    }

    if (event_id == WIFI_EVENT_AP_START) {
        s_snapshot.softap_active = true;
        if (!s_snapshot.credentials_ready) {
            network_service_set_state_locked(NETWORK_SERVICE_STATE_CONFIG_AP,
                                             "CONFIG AP",
                                             "Config AP started. Open the local setup page.");
        }
    } else if (event_id == WIFI_EVENT_AP_STOP) {
        s_snapshot.softap_active = false;
    }

    xSemaphoreGive(s_state_lock);
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
    network_service_format_ipv4(s_snapshot.ip, sizeof(s_snapshot.ip), &got_ip->ip_info.ip);
    network_service_format_ipv4(s_snapshot.netmask, sizeof(s_snapshot.netmask), &got_ip->ip_info.netmask);
    network_service_format_ipv4(s_snapshot.gateway, sizeof(s_snapshot.gateway), &got_ip->ip_info.gw);
    s_snapshot.ip_ready = true;

    if (s_snapshot.portal_enabled && (s_snapshot.portal_state != NETWORK_SERVICE_PORTAL_STATE_COMPLETED)) {
        s_snapshot.portal_state = NETWORK_SERVICE_PORTAL_STATE_REQUIRED;
        network_service_set_state_locked(NETWORK_SERVICE_STATE_PORTAL_REQUIRED,
                                         "PORTAL",
                                         "Intranet connected. Complete OA/device registration before using external network.");
    } else {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTED,
                                         "CONNECTED",
                                         "DHCP lease acquired. Network ready.");
    }

    network_service_refresh_runtime_fields_locked();
    xSemaphoreGive(s_state_lock);

    ESP_LOGI(TAG, "Connected to %s, IP=" IPSTR, s_snapshot.configured_ssid, IP2STR(&got_ip->ip_info.ip));
    (void)provider_service_request_refresh();
}

esp_err_t network_service_start(void)
{
    app_config_t config = {0};
    app_config_get(&config);

    char error_text[96];
    ESP_RETURN_ON_ERROR(app_config_validate(&config, error_text, sizeof(error_text)),
                        TAG,
                        "%s",
                        error_text);

    ESP_RETURN_ON_ERROR(network_service_ensure_base_services(), TAG, "base services init failed");

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    network_service_apply_config_locked(&config);
    if (s_wifi_started) {
        xSemaphoreGive(s_state_lock);
        return ESP_OK;
    }
    xSemaphoreGive(s_state_lock);

    s_wifi_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_wifi_sta_netif != NULL, ESP_FAIL, TAG, "wifi sta netif create failed");
    if (s_config.wifi.hostname[0] != '\0') {
        ESP_RETURN_ON_ERROR(esp_netif_set_hostname(s_wifi_sta_netif, s_config.wifi.hostname),
                            TAG,
                            "esp_netif_set_hostname failed");
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "esp_wifi_init failed");

    if (!s_sta_handlers_registered) {
        ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_service_wifi_event_handler, NULL),
                            TAG,
                            "wifi handler register failed");
        ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_service_ip_event_handler, NULL),
                            TAG,
                            "ip handler register failed");
        s_sta_handlers_registered = true;
    }

    wifi_config_t sta_config = {0};
    network_service_copy_text((char *)sta_config.sta.ssid,
                              sizeof(sta_config.sta.ssid),
                              s_config.wifi.ssid);
    network_service_copy_text((char *)sta_config.sta.password,
                              sizeof(sta_config.sta.password),
                              s_config.wifi.password);
    sta_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    sta_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    wifi_mode_t wifi_mode = network_service_get_non_ap_mode_locked();

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(wifi_mode), TAG, "esp_wifi_set_mode failed");
    if (s_snapshot.credentials_ready) {
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_config), TAG, "esp_wifi_set_config STA failed");
        if (s_config.wifi.security == APP_CONFIG_WIFI_SECURITY_WPA2_ENTERPRISE) {
            ESP_RETURN_ON_ERROR(network_service_apply_enterprise_credentials(&s_config.wifi),
                                TAG,
                                "enterprise credentials apply failed");
        } else {
            network_service_clear_enterprise_credentials();
        }
    }

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    s_wifi_started = true;
    if (s_snapshot.credentials_ready) {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTING,
                                         "CONNECTING",
                                         "Wi-Fi stack ready. Waiting for station start event.");
    } else {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_UNCONFIGURED,
                                         "UNCONFIGURED",
                                         "Hold BOOT for 2 seconds to toggle setup Wi-Fi.");
    }
    xSemaphoreGive(s_state_lock);

    return ESP_OK;
}

void network_service_get_snapshot(network_service_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    if (network_service_ensure_base_services() != ESP_OK) {
        memset(out, 0, sizeof(*out));
        return;
    }

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    network_service_refresh_runtime_fields_locked();
    memcpy(out, &s_snapshot, sizeof(*out));
    xSemaphoreGive(s_state_lock);
}

esp_err_t network_service_toggle_config_ap(void)
{
    ESP_RETURN_ON_ERROR(network_service_ensure_base_services(), TAG, "base services init failed");
    ESP_RETURN_ON_FALSE(s_wifi_started, ESP_ERR_INVALID_STATE, TAG, "Wi-Fi service not started");

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    esp_err_t err = s_snapshot.softap_active ? network_service_stop_config_ap_locked()
                                             : network_service_start_config_ap_locked();
    if (err == ESP_OK) {
        if (s_snapshot.softap_active) {
            network_service_set_state_locked(NETWORK_SERVICE_STATE_CONFIG_AP,
                                             "CONFIG AP",
                                             "Config AP active. Hold BOOT again to close setup Wi-Fi.");
        }
    }
    xSemaphoreGive(s_state_lock);

    return err;
}

esp_err_t network_service_mark_portal_complete(void)
{
    ESP_RETURN_ON_FALSE(s_state_lock != NULL, ESP_ERR_INVALID_STATE, TAG, "network service not initialized");

    xSemaphoreTake(s_state_lock, portMAX_DELAY);
    if (!s_snapshot.portal_enabled) {
        xSemaphoreGive(s_state_lock);
        return ESP_ERR_INVALID_STATE;
    }

    s_snapshot.portal_state = NETWORK_SERVICE_PORTAL_STATE_COMPLETED;
    if (s_snapshot.ip_ready) {
        network_service_set_state_locked(NETWORK_SERVICE_STATE_CONNECTED,
                                         "CONNECTED",
                                         "Portal registration completed. External network should be available.");
    }
    xSemaphoreGive(s_state_lock);
    return ESP_OK;
}
