/**
 * @file    provider_service.c
 * @brief   单活监控 provider 的轮询与状态快照实现。
 *
 * 当前实现只真正支持 AQI provider，但所有凭据、端点和展示名称都从运行时配置
 * 读取，避免把服务商假设硬编码进模块边界。
 */

#include "provider_service.h"
#include "app_config_service.h"

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "network_service.h"

static const char *TAG = "provider_service";
static const char *PROVIDER_SERVICE_ENDPOINT_PATH_FALLBACK = "/api/subscription/self";
static const uint32_t PROVIDER_SERVICE_HTTP_TIMEOUT_MS = 10000;
static const size_t PROVIDER_SERVICE_RESPONSE_BUFFER_SIZE = 8192;
static const double PROVIDER_SERVICE_QUOTA_PER_UNIT = 500000.0;
static const int32_t PROVIDER_SERVICE_TIMEZONE_OFFSET_SECONDS = 8 * 3600;

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
    bool truncated;
    char date_header[64];
} provider_service_http_buffer_t;

typedef struct {
    bool valid;
    int32_t id;
    int64_t total_raw;
    int64_t used_raw;
    int64_t end_time_raw;
    uint8_t used_percent;
    char status[PROVIDER_SERVICE_TEXT_LEN];
} provider_service_raw_item_t;

typedef struct {
    bool valid;
    int32_t subscription_id;
    int64_t unix_seconds;
    int64_t used_raw;
} provider_service_history_entry_t;

#define PROVIDER_SERVICE_HISTORY_CAPACITY 80U

static SemaphoreHandle_t s_state_lock;
static TaskHandle_t s_poll_task;
static bool s_service_started;
static provider_service_snapshot_t s_snapshot;
static app_config_t s_config;
static bool s_last_success_valid;
static int32_t s_last_success_subscription_id;
static int64_t s_last_success_used_raw;
static int64_t s_last_success_unix_seconds;
static provider_service_history_entry_t s_hourly_history[PROVIDER_SERVICE_HISTORY_CAPACITY];

static bool provider_service_parse_http_date(const char *date_header, int64_t *unix_seconds)
{
    if ((date_header == NULL) || (unix_seconds == NULL)) {
        return false;
    }

    char weekday[4] = {0};
    char month_text[4] = {0};
    int day = 0;
    int year = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (sscanf(date_header,
               "%3s, %d %3s %d %d:%d:%d GMT",
               weekday,
               &day,
               month_text,
               &year,
               &hour,
               &minute,
               &second)
        != 7) {
        return false;
    }

    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };

    int month = -1;
    for (int i = 0; i < 12; ++i) {
        if (strcmp(month_text, months[i]) == 0) {
            month = i + 1;
            break;
        }
    }
    if (month < 1) {
        return false;
    }

    struct tm tm_info = {
        .tm_sec = second,
        .tm_min = minute,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month - 1,
        .tm_year = year - 1900,
    };

    time_t unix_time = mktime(&tm_info);
    if (unix_time < 0) {
        return false;
    }

    *unix_seconds = (int64_t)unix_time;
    return true;
}

static void provider_service_copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0U)) {
        return;
    }

    snprintf(dst, dst_size, "%s", (src != NULL) ? src : "-");
}

static void provider_service_set_state_text(provider_service_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    switch (snapshot->state) {
    case PROVIDER_SERVICE_STATE_FETCHING:
        provider_service_copy_text(snapshot->state_text,
                                           sizeof(snapshot->state_text),
                                           "FETCHING");
        break;
    case PROVIDER_SERVICE_STATE_READY:
        provider_service_copy_text(snapshot->state_text,
                                           sizeof(snapshot->state_text),
                                           "READY");
        break;
    case PROVIDER_SERVICE_STATE_ERROR:
        provider_service_copy_text(snapshot->state_text,
                                           sizeof(snapshot->state_text),
                                           "ERROR");
        break;
    case PROVIDER_SERVICE_STATE_IDLE:
    default:
        provider_service_copy_text(snapshot->state_text,
                                           sizeof(snapshot->state_text),
                                           "IDLE");
        break;
    }
}

static void provider_service_set_status(provider_service_snapshot_t *snapshot,
                                                provider_service_state_t state,
                                                const char *status_text)
{
    if (snapshot == NULL) {
        return;
    }

    snapshot->state = state;
    provider_service_set_state_text(snapshot);
    provider_service_copy_text(snapshot->status_text,
                                       sizeof(snapshot->status_text),
                                       status_text);
}

static void provider_service_clear_items(provider_service_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    memset(snapshot->items, 0, sizeof(snapshot->items));
    snapshot->active_count = 0U;
}

static void provider_service_init_snapshot(void)
{
    app_config_get(&s_config);
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.provider_kind = s_config.provider.kind;
    provider_service_copy_text(s_snapshot.provider_name, sizeof(s_snapshot.provider_name), s_config.provider.display_name);
    s_snapshot.refresh_interval_ms = s_config.provider.refresh_interval_ms;
    s_snapshot.last_http_status = -1;
    s_snapshot.last_fetch_monotonic_us = 0;
    s_snapshot.credentials_ready = (s_config.provider.access_token[0] != '\0')
                                   && (s_config.provider.user_header_value[0] != '\0');
    if ((s_config.provider.kind != APP_CONFIG_PROVIDER_AQI) || (s_config.provider.base_url[0] == '\0')) {
        provider_service_set_status(&s_snapshot,
                                    PROVIDER_SERVICE_STATE_UNSUPPORTED,
                                    "Configured provider is not implemented yet");
        return;
    }

    provider_service_set_status(&s_snapshot,
                                PROVIDER_SERVICE_STATE_IDLE,
                                s_snapshot.credentials_ready ? "Waiting for first poll"
                                                             : "Provider token or user header is not configured");
}

static void provider_service_publish_snapshot(
    const provider_service_snapshot_t *snapshot)
{
    if ((snapshot == NULL) || (s_state_lock == NULL)) {
        return;
    }

    if (xSemaphoreTake(s_state_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire Provider state lock for publish");
        return;
    }

    s_snapshot = *snapshot;
    xSemaphoreGive(s_state_lock);
}

static void provider_service_capture_snapshot(provider_service_snapshot_t *out)
{
    if ((out == NULL) || (s_state_lock == NULL)) {
        return;
    }

    if (xSemaphoreTake(s_state_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
        memset(out, 0, sizeof(*out));
        provider_service_set_status(out,
                                            PROVIDER_SERVICE_STATE_ERROR,
                                            "Provider snapshot lock timeout");
        return;
    }

    *out = s_snapshot;
    xSemaphoreGive(s_state_lock);
}

static void provider_service_format_money(char *buffer,
                                                  size_t buffer_size,
                                                  int64_t raw_amount)
{
    if ((buffer == NULL) || (buffer_size == 0U)) {
        return;
    }

    double amount = (double)raw_amount / PROVIDER_SERVICE_QUOTA_PER_UNIT;
    snprintf(buffer, buffer_size, "$%.2f", amount);
}

static void provider_service_format_token_delta(char *buffer,
                                                        size_t buffer_size,
                                                        int64_t raw_amount)
{
    if ((buffer == NULL) || (buffer_size == 0U)) {
        return;
    }

    snprintf(buffer, buffer_size, "%" PRId64, raw_amount);
}

static void provider_service_format_percent_delta(char *buffer,
                                                          size_t buffer_size,
                                                          int64_t delta_raw,
                                                          int64_t total_raw)
{
    if ((buffer == NULL) || (buffer_size == 0U)) {
        return;
    }

    if (total_raw <= 0) {
        provider_service_copy_text(buffer, buffer_size, "0.0000%");
        return;
    }

    double percent = ((double)delta_raw * 100.0) / (double)total_raw;
    snprintf(buffer, buffer_size, "%.4f%%", percent);
}

static void provider_service_format_time(char *buffer,
                                                 size_t buffer_size,
                                                 int64_t unix_seconds)
{
    if ((buffer == NULL) || (buffer_size == 0U)) {
        return;
    }

    if (unix_seconds <= 0) {
        provider_service_copy_text(buffer, buffer_size, "-");
        return;
    }

    time_t local_epoch = (time_t)(unix_seconds + PROVIDER_SERVICE_TIMEZONE_OFFSET_SECONDS);
    struct tm time_info = {0};
    if (gmtime_r(&local_epoch, &time_info) == NULL) {
        provider_service_copy_text(buffer, buffer_size, "-");
        return;
    }

    strftime(buffer, buffer_size, "%Y/%m/%d %H:%M:%S", &time_info);
}

static const char *provider_service_find_key(const char *start,
                                                     const char *limit,
                                                     const char *key)
{
    if ((start == NULL) || (limit == NULL) || (key == NULL)) {
        return NULL;
    }

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *cursor = start;
    size_t pattern_len = strlen(pattern);
    while ((cursor != NULL) && (cursor < limit)) {
        cursor = strstr(cursor, pattern);
        if ((cursor == NULL) || (cursor >= limit)) {
            return NULL;
        }
        const char *after = cursor + pattern_len;
        while ((after < limit) && isspace((unsigned char)*after)) {
            after++;
        }
        if ((after < limit) && (*after == ':')) {
            return after + 1;
        }
        cursor = after;
    }

    return NULL;
}

static const char *provider_service_skip_ws(const char *cursor, const char *limit)
{
    while ((cursor != NULL) && (cursor < limit) && isspace((unsigned char)*cursor)) {
        cursor++;
    }
    return cursor;
}

static bool provider_service_parse_string_field(const char *object_start,
                                                        const char *object_limit,
                                                        const char *key,
                                                        char *out,
                                                        size_t out_size)
{
    const char *value = provider_service_find_key(object_start, object_limit, key);
    if (value == NULL) {
        return false;
    }

    value = provider_service_skip_ws(value, object_limit);
    if ((value == NULL) || (value >= object_limit) || (*value != '"')) {
        return false;
    }
    value++;

    const char *end = value;
    while ((end < object_limit) && (*end != '"')) {
        if ((*end == '\\') && ((end + 1) < object_limit)) {
            end += 2;
            continue;
        }
        end++;
    }
    if (end >= object_limit) {
        return false;
    }

    size_t length = (size_t)(end - value);
    if (length >= out_size) {
        length = out_size - 1U;
    }
    memcpy(out, value, length);
    out[length] = '\0';
    return true;
}

static bool provider_service_parse_int64_field(const char *object_start,
                                                       const char *object_limit,
                                                       const char *key,
                                                       int64_t *out)
{
    if (out == NULL) {
        return false;
    }

    const char *value = provider_service_find_key(object_start, object_limit, key);
    if (value == NULL) {
        return false;
    }

    value = provider_service_skip_ws(value, object_limit);
    if ((value == NULL) || (value >= object_limit)) {
        return false;
    }

    char *parse_end = NULL;
    long long parsed = strtoll(value, &parse_end, 10);
    if ((parse_end == value) || (parse_end == NULL) || (parse_end > object_limit)) {
        return false;
    }

    *out = (int64_t)parsed;
    return true;
}

static const char *provider_service_find_matching_brace(const char *cursor)
{
    if ((cursor == NULL) || (*cursor != '{')) {
        return NULL;
    }

    int depth = 0;
    bool in_string = false;
    for (const char *it = cursor; *it != '\0'; ++it) {
        if (*it == '"' && ((it == cursor) || (*(it - 1) != '\\'))) {
            in_string = !in_string;
            continue;
        }

        if (in_string) {
            continue;
        }

        if (*it == '{') {
            depth++;
        } else if (*it == '}') {
            depth--;
            if (depth == 0) {
                return it;
            }
        }
    }

    return NULL;
}

static void provider_service_update_delta_fields(
    provider_service_snapshot_t *snapshot,
    bool current_valid,
    int32_t current_subscription_id,
    int64_t current_used_raw)
{
    if (snapshot == NULL) {
        return;
    }

    snapshot->last_success_interval_seconds = 0U;
    snapshot->delta_used_raw = 0;
    provider_service_copy_text(snapshot->delta_used_tokens,
                                       sizeof(snapshot->delta_used_tokens),
                                       "0");
    provider_service_copy_text(snapshot->delta_used_amount,
                                       sizeof(snapshot->delta_used_amount),
                                       "$0.00");
    provider_service_copy_text(snapshot->delta_used_percent,
                                       sizeof(snapshot->delta_used_percent),
                                       "0.0000%");
    provider_service_copy_text(snapshot->hourly_used_amount,
                                       sizeof(snapshot->hourly_used_amount),
                                       "$0.00");

    if (!current_valid) {
        s_last_success_valid = false;
        return;
    }

    if (s_last_success_valid && (current_subscription_id == s_last_success_subscription_id)
        && (snapshot->last_fetch_unix_seconds > s_last_success_unix_seconds)
        && (current_used_raw >= s_last_success_used_raw)) {
        snapshot->last_success_interval_seconds =
            (uint32_t)(snapshot->last_fetch_unix_seconds - s_last_success_unix_seconds);
        snapshot->delta_used_raw = current_used_raw - s_last_success_used_raw;
        provider_service_format_token_delta(snapshot->delta_used_tokens,
                                                    sizeof(snapshot->delta_used_tokens),
                                                    snapshot->delta_used_raw);
        provider_service_format_money(snapshot->delta_used_amount,
                                              sizeof(snapshot->delta_used_amount),
                                              snapshot->delta_used_raw);
    }

    s_last_success_valid = true;
    s_last_success_subscription_id = current_subscription_id;
    s_last_success_used_raw = current_used_raw;
    s_last_success_unix_seconds = snapshot->last_fetch_unix_seconds;
}

static void provider_service_record_hourly_history(int32_t subscription_id,
                                                           int64_t unix_seconds,
                                                           int64_t used_raw)
{
    size_t slot = PROVIDER_SERVICE_HISTORY_CAPACITY;
    for (size_t i = 0; i < PROVIDER_SERVICE_HISTORY_CAPACITY; ++i) {
        if (!s_hourly_history[i].valid) {
            slot = i;
            break;
        }
    }

    if (slot == PROVIDER_SERVICE_HISTORY_CAPACITY) {
        memmove(&s_hourly_history[0],
                &s_hourly_history[1],
                sizeof(s_hourly_history[0]) * (PROVIDER_SERVICE_HISTORY_CAPACITY - 1U));
        slot = PROVIDER_SERVICE_HISTORY_CAPACITY - 1U;
    }

    s_hourly_history[slot] = (provider_service_history_entry_t){
        .valid = true,
        .subscription_id = subscription_id,
        .unix_seconds = unix_seconds,
        .used_raw = used_raw,
    };
}

static void provider_service_update_hourly_amount(provider_service_snapshot_t *snapshot,
                                                          bool current_valid,
                                                          int32_t current_subscription_id,
                                                          int64_t current_used_raw,
                                                          int64_t current_unix_seconds)
{
    if ((snapshot == NULL) || !current_valid || (current_unix_seconds <= 0)) {
        return;
    }

    provider_service_record_hourly_history(current_subscription_id,
                                                   current_unix_seconds,
                                                   current_used_raw);

    int64_t best_used_raw = current_used_raw;
    int64_t cutoff = current_unix_seconds - 3600LL;
    for (size_t i = 0; i < PROVIDER_SERVICE_HISTORY_CAPACITY; ++i) {
        if (!s_hourly_history[i].valid) {
            continue;
        }
        if (s_hourly_history[i].subscription_id != current_subscription_id) {
            continue;
        }
        if (s_hourly_history[i].unix_seconds < cutoff) {
            continue;
        }
        if (s_hourly_history[i].used_raw < best_used_raw) {
            best_used_raw = s_hourly_history[i].used_raw;
        }
    }

    int64_t hourly_delta = current_used_raw - best_used_raw;
    if (hourly_delta < 0) {
        hourly_delta = 0;
    }
    provider_service_format_money(snapshot->hourly_used_amount,
                                          sizeof(snapshot->hourly_used_amount),
                                          hourly_delta);
}

static esp_err_t provider_service_parse_response(
    const char *json_text,
    provider_service_snapshot_t *snapshot)
{
    if ((json_text == NULL) || (snapshot == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *subscriptions_key = strstr(json_text, "\"all_subscriptions\"");
    if (subscriptions_key == NULL) {
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_ERROR,
                                            "Provider response missing subscription list");
        return ESP_FAIL;
    }

    const char *cursor = strchr(subscriptions_key, '[');
    if (cursor == NULL) {
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_ERROR,
                                            "Provider response missing subscription array");
        return ESP_FAIL;
    }

    provider_service_clear_items(snapshot);
    size_t stored_items = 0U;
    bool primary_valid = false;
    int32_t primary_subscription_id = -1;
    int64_t primary_used_raw = 0;
    int64_t primary_total_raw = 0;

    while ((cursor = strstr(cursor, "\"subscription\"")) != NULL) {
        const char *object_start = strchr(cursor, '{');
        if (object_start == NULL) {
            break;
        }

        const char *object_end = provider_service_find_matching_brace(object_start);
        if (object_end == NULL) {
            break;
        }

        provider_service_raw_item_t candidate = {0};
        int64_t total_raw = 0;
        int64_t used_raw = 0;
        int64_t end_time_raw = 0;
        int64_t id_value = -1;

        if (!provider_service_parse_string_field(object_start,
                                                         object_end,
                                                         "status",
                                                         candidate.status,
                                                         sizeof(candidate.status))) {
            cursor = object_end + 1;
            continue;
        }

        if (strcmp(candidate.status, "active") != 0) {
            cursor = object_end + 1;
            continue;
        }

        snapshot->active_count++;
        candidate.valid = true;
        (void)provider_service_parse_int64_field(object_start, object_end, "id", &id_value);
        (void)provider_service_parse_int64_field(object_start,
                                                         object_end,
                                                         "amount_total",
                                                         &total_raw);
        (void)provider_service_parse_int64_field(object_start,
                                                         object_end,
                                                         "amount_used",
                                                         &used_raw);
        (void)provider_service_parse_int64_field(object_start,
                                                         object_end,
                                                         "end_time",
                                                         &end_time_raw);

        candidate.id = (int32_t)id_value;
        candidate.total_raw = total_raw;
        candidate.used_raw = used_raw;
        candidate.end_time_raw = end_time_raw;
        if (!primary_valid) {
            primary_valid = true;
            primary_subscription_id = candidate.id;
            primary_used_raw = candidate.used_raw;
            primary_total_raw = candidate.total_raw;
        }

        if (candidate.total_raw > 0) {
            double ratio = ((double)candidate.used_raw * 100.0) / (double)candidate.total_raw;
            if (ratio < 0.0) {
                ratio = 0.0;
            } else if (ratio > 100.0) {
                ratio = 100.0;
            }
            candidate.used_percent = (uint8_t)lround(ratio);
        }

        if (stored_items < PROVIDER_SERVICE_MAX_ITEMS) {
            provider_service_item_t *item = &snapshot->items[stored_items++];
            int64_t remaining_raw = candidate.total_raw - candidate.used_raw;
            if (remaining_raw < 0) {
                remaining_raw = 0;
            }

            memset(item, 0, sizeof(*item));
            item->valid = true;
            item->id = candidate.id;
            item->end_time_unix_seconds = candidate.end_time_raw;
            item->used_percent = candidate.used_percent;
            provider_service_copy_text(item->status, sizeof(item->status), candidate.status);
            provider_service_format_money(item->total_amount,
                                                  sizeof(item->total_amount),
                                                  candidate.total_raw);
            provider_service_format_money(item->remaining_amount,
                                                  sizeof(item->remaining_amount),
                                                  remaining_raw);
            provider_service_format_time(item->end_time,
                                                 sizeof(item->end_time),
                                                 candidate.end_time_raw);
        }
        cursor = object_end + 1;
    }

    provider_service_update_delta_fields(snapshot,
                                                 primary_valid,
                                                 primary_subscription_id,
                                                 primary_used_raw);
    provider_service_update_hourly_amount(snapshot,
                                                  primary_valid,
                                                  primary_subscription_id,
                                                  primary_used_raw,
                                                  snapshot->last_fetch_unix_seconds);
    provider_service_format_percent_delta(snapshot->delta_used_percent,
                                                  sizeof(snapshot->delta_used_percent),
                                                  snapshot->delta_used_raw,
                                                  primary_total_raw);

    if (snapshot->active_count == 0U) {
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_READY,
                                            "No active Provider subscriptions");
    } else {
        char message[sizeof(snapshot->status_text)];
        snprintf(message,
                 sizeof(message),
                 "Loaded %" PRIu32 " active subscription(s)",
                 snapshot->active_count);
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_READY,
                                            message);
    }

    return ESP_OK;
}

static esp_err_t provider_service_http_event_handler(esp_http_client_event_t *event)
{
    if ((event == NULL) || (event->user_data == NULL)) {
        return ESP_OK;
    }

    provider_service_http_buffer_t *buffer =
        (provider_service_http_buffer_t *)event->user_data;

    if ((event->event_id == HTTP_EVENT_ON_HEADER) && (event->header_key != NULL)
        && (event->header_value != NULL) && (strcasecmp(event->header_key, "Date") == 0)) {
        snprintf(buffer->date_header,
                 sizeof(buffer->date_header),
                 "%s",
                 event->header_value);
        return ESP_OK;
    }

    if ((event->event_id != HTTP_EVENT_ON_DATA) || (event->data == NULL) || (event->data_len <= 0)) {
        return ESP_OK;
    }

    if (buffer->truncated || (buffer->buffer == NULL) || (buffer->capacity == 0U)) {
        return ESP_OK;
    }

    size_t remaining = buffer->capacity - buffer->length - 1U;
    size_t copy_len = (size_t)event->data_len;
    if (copy_len > remaining) {
        copy_len = remaining;
        buffer->truncated = true;
    }

    if (copy_len > 0U) {
        memcpy(buffer->buffer + buffer->length, event->data, copy_len);
        buffer->length += copy_len;
        buffer->buffer[buffer->length] = '\0';
    }

    return ESP_OK;
}

static esp_err_t provider_service_fetch_once(provider_service_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    app_config_get(&s_config);
    snapshot->provider_kind = s_config.provider.kind;
    provider_service_copy_text(snapshot->provider_name, sizeof(snapshot->provider_name), s_config.provider.display_name);
    snapshot->refresh_interval_ms = s_config.provider.refresh_interval_ms;
    snapshot->credentials_ready = (s_config.provider.access_token[0] != '\0')
                                  && (s_config.provider.user_header_value[0] != '\0');

    if (s_config.provider.kind != APP_CONFIG_PROVIDER_AQI) {
        provider_service_set_status(snapshot,
                                    PROVIDER_SERVICE_STATE_UNSUPPORTED,
                                    "Configured provider is not implemented yet");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (!snapshot->credentials_ready) {
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_IDLE,
                                            "Provider token or user header is not configured");
        return ESP_OK;
    }

    network_service_snapshot_t network_snapshot = {0};
    network_service_get_snapshot(&network_snapshot);
    snapshot->network_ready = network_snapshot.ip_ready
                              && (network_snapshot.portal_state != NETWORK_SERVICE_PORTAL_STATE_REQUIRED);
    if (!snapshot->network_ready) {
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_IDLE,
                                            "Waiting for Wi-Fi / portal readiness");
        return ESP_OK;
    }

    char *response_buffer = calloc(1U, PROVIDER_SERVICE_RESPONSE_BUFFER_SIZE);
    if (response_buffer == NULL) {
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_ERROR,
                                            "Provider response buffer allocation failed");
        return ESP_ERR_NO_MEM;
    }

    char endpoint_url[256];
    snprintf(endpoint_url,
             sizeof(endpoint_url),
             "%s%s",
             s_config.provider.base_url,
             (s_config.provider.endpoint_path[0] != '\0') ? s_config.provider.endpoint_path
                                                          : PROVIDER_SERVICE_ENDPOINT_PATH_FALLBACK);

    char auth_header[192];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_config.provider.access_token);
    int64_t request_monotonic_us = esp_timer_get_time();

    provider_service_http_buffer_t buffer = {
        .buffer = response_buffer,
        .length = 0U,
        .capacity = PROVIDER_SERVICE_RESPONSE_BUFFER_SIZE,
        .truncated = false,
    };

    esp_http_client_config_t config = {
        .url = endpoint_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = PROVIDER_SERVICE_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = provider_service_http_event_handler,
        .user_data = &buffer,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(response_buffer);
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_ERROR,
                                            "Provider HTTP client init failed");
        return ESP_FAIL;
    }

    snapshot->fetch_count++;
    provider_service_set_status(snapshot,
                                        PROVIDER_SERVICE_STATE_FETCHING,
                                        "Fetching provider subscriptions");

    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client,
                               (s_config.provider.user_header_name[0] != '\0')
                                   ? s_config.provider.user_header_name
                                   : "New-Api-User",
                               s_config.provider.user_header_value);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "User-Agent", "ESP32-AI-Monitor/1.0");

    esp_err_t err = esp_http_client_perform(client);
    snapshot->last_http_status = esp_http_client_get_status_code(client);
    snapshot->last_fetch_monotonic_us = request_monotonic_us;

    if ((err == ESP_OK)
        && (buffer.date_header[0] != '\0')
        && provider_service_parse_http_date(buffer.date_header, &snapshot->last_fetch_unix_seconds)) {
        /* use server time as authoritative base */
    } else {
        snapshot->last_fetch_unix_seconds = 0;
    }
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        snapshot->failure_count++;
        char message[sizeof(snapshot->status_text)];
        snprintf(message, sizeof(message), "Provider request failed: %s", esp_err_to_name(err));
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_ERROR,
                                            message);
        free(response_buffer);
        return err;
    }

    if (buffer.truncated) {
        snapshot->failure_count++;
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_ERROR,
                                            "Provider response truncated");
        free(response_buffer);
        return ESP_ERR_INVALID_SIZE;
    }

    if (snapshot->last_http_status != 200) {
        snapshot->failure_count++;
        char message[sizeof(snapshot->status_text)];
        snprintf(message,
                 sizeof(message),
                 "Provider HTTP %ld",
                 (long)snapshot->last_http_status);
        provider_service_set_status(snapshot,
                                            PROVIDER_SERVICE_STATE_ERROR,
                                            message);
        free(response_buffer);
        return ESP_FAIL;
    }

    err = provider_service_parse_response(response_buffer, snapshot);
    if (err == ESP_OK) {
        snapshot->success_count++;
    } else {
        snapshot->failure_count++;
    }

    free(response_buffer);
    return err;
}

static void provider_service_poll_task(void *arg)
{
    (void)arg;

    provider_service_snapshot_t local_snapshot = {0};
    provider_service_capture_snapshot(&local_snapshot);

    while (true) {
        provider_service_snapshot_t next_snapshot = local_snapshot;
        (void)provider_service_fetch_once(&next_snapshot);
        provider_service_publish_snapshot(&next_snapshot);
        local_snapshot = next_snapshot;
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(next_snapshot.refresh_interval_ms));
    }
}

esp_err_t provider_service_start(void)
{
    if (s_service_started) {
        return ESP_OK;
    }

    s_state_lock = xSemaphoreCreateMutex();
    if (s_state_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    provider_service_init_snapshot();

    BaseType_t task_ok = xTaskCreate(provider_service_poll_task,
                                     "provider_subscription",
                                     8192,
                                     NULL,
                                     4,
                                     &s_poll_task);
    if (task_ok != pdPASS) {
        vSemaphoreDelete(s_state_lock);
        s_state_lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_service_started = true;
    ESP_LOGI(TAG,
             "Provider service started for %s with interval=%" PRIu32 " ms",
             s_snapshot.provider_name,
             s_snapshot.refresh_interval_ms);
    return ESP_OK;
}

void provider_service_get_snapshot(provider_service_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    provider_service_capture_snapshot(out);
}

esp_err_t provider_service_request_refresh(void)
{
    if (!s_service_started || (s_poll_task == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    provider_service_snapshot_t next_snapshot = {0};
    provider_service_capture_snapshot(&next_snapshot);
    provider_service_set_status(&next_snapshot,
                                        PROVIDER_SERVICE_STATE_FETCHING,
                                        "Refreshing Provider subscriptions");
    provider_service_publish_snapshot(&next_snapshot);
    xTaskNotifyGive(s_poll_task);
    return ESP_OK;
}
