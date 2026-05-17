---
last_mapped_commit: f4a155a1d23a3aa8ca4e7cb568217b35c1d5a510
mapped_at: 2026-05-17
---

# STACK

## 主技术栈

当前项目的主技术栈是：

- 语言
  - `C`
- 构建系统
  - `CMake`
  - `Ninja`
- SDK
  - `ESP-IDF v6.0.1`
- 目标芯片
  - `esp32p4`
- UI
  - `LVGL 9.x`
  - `espressif/esp_lvgl_port`
- 板级支持
  - `waveshare/esp32_p4_wifi6_touch_lcd_4b`
  - `waveshare/esp_lcd_st7703`
- 无线链路
  - `espressif/esp_hosted`
  - `espressif/esp_wifi_remote`
- 本地网页
  - `esp_http_server`
- 外部轮询
  - `esp_http_client`
- 持久化
  - `NVS`

## 当前关键配置方向

从 `sdkconfig.defaults` 可确认的关键方向包括：

- `CONFIG_IDF_TARGET="esp32p4"`
- `CONFIG_ESPTOOLPY_FLASHSIZE="32MB"`
- `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_32mb_singleapp.csv"`
- `CONFIG_SPIRAM=y`
- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

这说明当前固件明确依赖：

- `32MB flash`
- 自定义分区表
- `PSRAM`
- Hosted Wi-Fi Remote 路线
- `TinyTTF + CLIB malloc`

## Board Support 组合

当前显示与触摸路径仍然建立在官方 `BSP` 组合之上：

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `waveshare/esp_lcd_st7703`
- `espressif/esp_lcd_touch_gt911`
- `espressif/esp_lvgl_port`

仓库内保留项目级 override 的理由不是改方向，而是保住这条官方路线在当前 SDK 版本上的可用性。

## Hosted / Wi-Fi Remote 组合

当前无线链路使用：

- `espressif/esp_hosted`
- `espressif/esp_wifi_remote`
- `espressif/wifi_remote_over_eppp`
- `espressif/eppp_link`

其设计含义是：

- `P4` 不承担本地原生 Wi-Fi 控制器角色
- 板载 `C6` 是协处理器
- 故障排查要优先按 Hosted / Remote 路线理解

## 运行时服务栈

当前业务服务栈可以概括为：

- `app_config_service`
  - 配置中心
- `network_service`
  - 网络接入与门户状态
- `provider_service`
  - 外部数据轮询
- `config_web_service`
  - 本地配置页与 REST 接口
- `ui_service`
  - 板上主监控视图

这套服务栈已经把运行期可调配置作为一等能力，而不是只依赖编译期常量。

## UI 技术约束

当前 UI 技术上依赖：

- `LVGL`
- `TinyTTF`
- `PSRAM`
- 仅变更文本时更新 `label`

这不是样式偏好，而是稳定性要求。页面复杂度已经超过“随便几个 label 刷新”的阶段。

## HTTP 技术约束

当前本地配置网页选择了最保守、最可抓包的方案：

- 服务端：`esp_http_server`
- 页面：单文件内联 HTML/CSS/JS
- 接口：少量 `GET` / `POST`

这意味着配置入口更偏运维控制台，而不是完整前后端应用。
