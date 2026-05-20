/**
 * @file    provider_service.h
 * @brief   外部监控 provider 的轮询与状态快照接口。
 *
 * 本组件负责根据运行时配置启动单活 provider，并把结果整理成 UI 与配置页
 * 可直接消费的统一快照。当前仅落地 AQI provider，但公共接口不再暴露 AQI
 * 专有命名。
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_config_service.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROVIDER_SERVICE_TEXT_LEN         32
#define PROVIDER_SERVICE_STATUS_TEXT_LEN  96
#define PROVIDER_SERVICE_TIME_TEXT_LEN    24
#define PROVIDER_SERVICE_MONEY_TEXT_LEN   24
#define PROVIDER_SERVICE_MAX_ITEMS        2

typedef enum {
    PROVIDER_SERVICE_STATE_IDLE = 0,
    PROVIDER_SERVICE_STATE_FETCHING,
    PROVIDER_SERVICE_STATE_READY,
    PROVIDER_SERVICE_STATE_ERROR,
    PROVIDER_SERVICE_STATE_UNSUPPORTED,
} provider_service_state_t;

typedef struct {
    bool valid;
    int32_t id;
    int64_t end_time_unix_seconds;
    char label[PROVIDER_SERVICE_TEXT_LEN];
    char status[PROVIDER_SERVICE_TEXT_LEN];
    char end_time[PROVIDER_SERVICE_TIME_TEXT_LEN];
    char total_amount[PROVIDER_SERVICE_MONEY_TEXT_LEN];
    char remaining_amount[PROVIDER_SERVICE_MONEY_TEXT_LEN];
    uint8_t used_percent;
} provider_service_item_t;

typedef struct {
    provider_service_state_t state;
    app_config_provider_kind_t provider_kind;
    char provider_name[APP_CONFIG_PROVIDER_NAME_LEN];
    bool credentials_ready;
    bool network_ready;
    uint32_t refresh_interval_ms;
    uint32_t fetch_count;
    uint32_t success_count;
    uint32_t failure_count;
    uint32_t active_count;
    int32_t last_http_status;
    int64_t last_fetch_unix_seconds;
    int64_t last_fetch_monotonic_us;
    uint32_t last_success_interval_seconds;
    int64_t delta_used_raw;
    char delta_used_tokens[PROVIDER_SERVICE_TEXT_LEN];
    char delta_used_amount[PROVIDER_SERVICE_MONEY_TEXT_LEN];
    char delta_used_percent[PROVIDER_SERVICE_TEXT_LEN];
    char hourly_used_amount[PROVIDER_SERVICE_MONEY_TEXT_LEN];
    char state_text[PROVIDER_SERVICE_TEXT_LEN];
    char status_text[PROVIDER_SERVICE_STATUS_TEXT_LEN];
    provider_service_item_t items[PROVIDER_SERVICE_MAX_ITEMS];
} provider_service_snapshot_t;

esp_err_t provider_service_start(void);
void provider_service_get_snapshot(provider_service_snapshot_t *out);
esp_err_t provider_service_request_refresh(void);

#ifdef __cplusplus
}
#endif
