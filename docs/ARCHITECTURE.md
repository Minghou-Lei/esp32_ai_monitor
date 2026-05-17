<!-- generated-by: gsd-doc-writer -->
# ARCHITECTURE

## 系统概览

当前固件是一个运行在 `ESP32-P4` 上的本地监控终端。它把板级显示、Wi-Fi / 企业网接入、远端 provider 状态拉取和本地配置网页组合成一个统一运行时：用户既可以在屏幕上看状态，也可以通过设备当前可达的 IP 打开本地配置页调整运行参数。

## 组件关系

```text
app_main()
  ├─ wifi_info_screen_start()
  ├─ network_service_start()
  ├─ provider_service_start()
  └─ config_web_service_start()

app_config_service
  ├─ network_service
  ├─ provider_service
  ├─ config_web_service
  └─ ui_service

network_service ── snapshot ──► ui_service
provider_service ─ snapshot ──► ui_service
network_service ── snapshot ──► config_web_service
provider_service ─ snapshot ──► config_web_service
```

## 启动编排

入口文件是 `main/main.c`。

启动顺序固定为：

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`

这样安排的目的很明确：

- 先点亮主屏，设备上电后立刻给出可见反馈
- 再启动网络状态机，让仪表盘进入可诊断态
- 再启动 provider 轮询，把远端业务状态折叠进本地快照
- 最后开放板上配置入口，给首次配网和参数修正留出口

## 配置中心

配置中心位于：

- `components/app_config_service/include/app_config_service.h`
- `components/app_config_service/app_config_service.c`

它负责：

- 从 `sdkconfig.defaults` / `sdkconfig` 装配编译期默认值
- 维护统一的 `app_config_t`
- 校验配置字段约束
- 把运行时覆盖写入 NVS
- 在需要时恢复默认配置

当前配置模型至少覆盖：

- Wi-Fi 基本接入参数
- WPA2-Enterprise 凭据
- 门户元数据
- fallback 配置热点
- provider 端点、鉴权与展示参数
- UI / provider 刷新周期

## 网络接入层

网络服务位于：

- `components/network_service/include/network_service.h`
- `components/network_service/network_service.c`

它承担的不是简单的 `STA` 连网，而是完整的接入状态机：

- 初始化 NVS、默认事件循环和默认 netif
- 启动 `Wi-Fi Station`
- 根据配置决定是否启用 fallback `SoftAP`
- 支持 `WPA2-PSK` 与 `WPA2-Enterprise`
- 维护门户状态
- 聚合统一的 `network_service_snapshot_t`

当前重要状态包括：

- `NETWORK_SERVICE_STATE_UNCONFIGURED`
- `NETWORK_SERVICE_STATE_IDLE`
- `NETWORK_SERVICE_STATE_CONNECTING`
- `NETWORK_SERVICE_STATE_CONNECTED`
- `NETWORK_SERVICE_STATE_DISCONNECTED`
- `NETWORK_SERVICE_STATE_PORTAL_REQUIRED`
- `NETWORK_SERVICE_STATE_CONFIG_AP`

门户状态通过 `components/network_service/network_service.c` 中的 `network_service_mark_portal_complete()` 推进。

## Provider 轮询层

Provider 服务位于：

- `components/provider_service/include/provider_service.h`
- `components/provider_service/provider_service.c`

它当前已经做成通用 provider 形状，但真实实现只有 `AQI` provider 首版。职责包括：

- 从统一配置读取 `base_url`、`endpoint_path`、access token 和自定义用户头
- 使用 `esp_http_client` 轮询远端接口
- 维护抓取次数、成功次数、失败次数、最近 HTTP 状态
- 计算用量 delta 与小时级统计
- 导出 `provider_service_snapshot_t`

Provider 轮询任务在 `components/provider_service/provider_service.c` 中运行，按照 `refresh_interval_ms` 周期刷新，也支持手动刷新请求。

## 板上配置网页

配置网页服务位于：

- `components/config_web_service/include/config_web_service.h`
- `components/config_web_service/config_web_service.c`

它通过 `esp_http_server` 暴露一个单文件 HTML 页面和少量 REST 接口：

- `GET /`
- `GET /api/config`
- `POST /api/config`
- `GET /api/status`
- `POST /api/portal/complete`
- `POST /api/restart`

这层的设计目标不是“富网页应用”，而是：

- 单文件
- 易抓包
- 在 bring-up 阶段也足够稳
- 所有保存动作都复用 `app_config_service` 的校验逻辑

## UI 与显示层

UI 实现位于 `components/ui_service/monitor_dashboard_screen.c`，但公开入口头文件仍是 `components/ui_service/include/wifi_info_screen.h`。

当前主屏已经不是旧的 Wi-Fi 详情页，而是 AQI 风格主监控屏，展示内容包括：

- 主余额卡片
- USED 百分比
- DELTA 信息
- 小时消费金额
- 底部网络 / provider / 诊断状态文本

屏幕刷新依赖 `network_service_snapshot_t` 与 `provider_service_snapshot_t`，不直接管理 Wi-Fi 驱动生命周期。

## 板级与运行时约束

当前架构建立在这些前提上：

- `ESP-IDF v6.0.1`
- `esp32p4`
- `32MB flash`
- `PSRAM`
- `LVGL + TinyTTF + CLIB malloc`
- `ESP-Hosted + esp_wifi_remote`
- `waveshare/esp32_p4_wifi6_touch_lcd_4b` BSP

无线能力的正确理解仍然是：

- `ESP32-P4` 负责主控、UI 和业务逻辑
- 板载 `ESP32-C6` 提供无线能力
- 主工程通过 Hosted / Remote 路线使用 Wi-Fi

因此不要把本项目按“P4 原生本地 Wi-Fi”去推导。

## 结构性风险

当前最值得持续盯住的结构问题有两类：

1. UI 的公开 API 名与实现语义已经错位
2. `network_service` 与 `provider_service` 在组件层形成了双向依赖

这两点都不阻止当前固件继续演进，但后续扩模块时必须显式处理，不能再继续放大耦合。
