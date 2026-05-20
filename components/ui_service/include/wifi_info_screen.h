/**
 * @file    wifi_info_screen.h
 * @brief   AQI 主监控屏幕初始化接口。
 *
 * 本组件负责恢复并维护板上的主显示界面：AQI 余额卡片、USED 百分比、
 * DELTA 信息和底部状态区。
 * 配置网页仍通过 HTTP 提供，不再占用板上主视图。
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_info_screen_start(void);

#ifdef __cplusplus
}
#endif
