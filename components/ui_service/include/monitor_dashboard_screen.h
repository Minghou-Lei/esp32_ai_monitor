/**
 * @file    monitor_dashboard_screen.h
 * @brief   监控终端总览页初始化接口。
 *
 * 本组件负责初始化板级显示、创建 LVGL 监控总览页，并周期性刷新网络、
 * provider 与门户状态快照。
 * 本组件不直接管理 Wi-Fi 驱动生命周期。
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动监控总览屏幕。
 *
 * @note 该函数会初始化 BSP 显示与 LVGL 对象树，并创建周期性刷新定时器。
 *       建议在 app_main() 启动阶段调用一次。
 */
esp_err_t monitor_dashboard_screen_start(void);

#ifdef __cplusplus
}
#endif
