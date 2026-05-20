/**
 * @file    config_web_service.h
 * @brief   板上配置网页服务入口。
 *
 * 本组件通过 `esp_http_server` 暴露本地配置页，使用户可以通过设备当前可达的
 * `STA` 或 `SoftAP` IP 调整 Wi-Fi、portal 和 provider 配置。
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_web_service_start(void);

#ifdef __cplusplus
}
#endif
