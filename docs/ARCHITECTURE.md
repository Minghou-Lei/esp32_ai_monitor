<!-- generated-by: gsd-doc-writer -->
# ARCHITECTURE

## 系统概览

`esp32_ai_monitor` 是运行在 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B` 上的板端监控终端。当前固件的职责是把屏幕、网络、配置门户和远端 provider 状态组织成一个可上板验证的终端，而不是在 `ESP32-P4` 上运行完整 AI Agent。

当前运行时已经包括：

- `BSP + LVGL` 主监控屏。
- 统一运行时配置模型。
- `STA` / 企业认证 / 门户状态 / fallback `SoftAP`。
- 远端 provider HTTP polling。
- 板上本地配置网页和 REST API。
- BOOT 按钮长按配置入口。

## 启动编排

`main/main.c` 保持薄入口，只负责启动服务并记录启动错误。当前顺序：

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`
5. `board_input_service_start()`

这条顺序的含义：

- 先点亮屏幕，确保上电后有可见反馈。
- 再启动网络，让 UI 能显示连接与门户状态。
- 再启动 provider 轮询，让远端状态进入快照。
- 再启动本地配置网页。
- 最后启动物理按钮轮询，给现场配置留入口。

## 组件关系

```text
app_main()
  ├─ wifi_info_screen_start()
  ├─ network_service_start()
  ├─ provider_service_start()
  ├─ config_web_service_start()
  └─ board_input_service_start()

app_config_service
  ├─ network_service
  ├─ provider_service
  ├─ config_web_service
  └─ ui_service

network_service ── snapshot ──► ui_service
provider_service ─ snapshot ──► ui_service
network_service ── snapshot ──► config_web_service
provider_service ─ snapshot ──► config_web_service
board_input_service ─ command ─► network_service
```

## 配置中心

`components/app_config_service` 是运行时配置的单一入口。

它负责：

- 从 `sdkconfig` 符号装配默认值。
- 从 NVS 读取运行时覆盖。
- 保存新的配置 blob。
- 校验字段长度、必要值和 AP 密码规则。
- 提供字符串与枚举之间的转换。

当前 `app_config_t` 覆盖：

- `wifi`
  - SSID、密码、主机名、认证模式、企业认证字段、门户字段。
- `config_ap`
  - 启用状态、SSID、密码。
- `provider`
  - 类型、显示名、base URL、endpoint path、access token、management key、用户头、刷新周期。
- `ui_refresh_interval_ms`

新增配置字段应先进入 `app_config_service`，再让网络、provider、Web 和 UI 层消费。

## 网络接入层

`components/network_service` 负责无线接入和门户状态。

当前能力：

- 初始化 NVS、事件循环、netif 和 Wi-Fi。
- 根据运行时配置启动 `STA`。
- 支持开放网络、WPA2-PSK 和 WPA2-Enterprise。
- 维护门户状态：disabled、waiting、required、completed。
- 支持 fallback 配置 AP。
- 暴露 `network_service_snapshot_t`。

快照字段包括：

- 当前状态和状态文本。
- 凭据是否就绪、IP 是否就绪、SoftAP 是否 active。
- SSID、STA MAC、AP MAC、BSSID。
- IPv4、netmask、gateway、DNS。
- RSSI、主信道、辅助信道。
- portal URL、认证模式和 cipher 文本。

## Provider 轮询层

`components/provider_service` 负责远端 provider polling 和数据归一化。

当前实现：

- 从 `app_config_service` 读取 provider 配置。
- 等待网络可用后发起 HTTP 请求。
- 解析 provider 响应。
- 维护抓取次数、成功次数、失败次数、HTTP 状态、最近抓取时间。
- 输出最多两个 `provider_service_item_t`。
- 计算 delta、小时消费金额和使用比例。

当前真实 provider 类型是 `APP_CONFIG_PROVIDER_AQI`。接口已经预留 provider 抽象，但不要把它描述成已经实现多个 provider。

## 本地配置网页

`components/config_web_service` 提供板上 HTTP 配置面。

当前路由：

- `GET /api/config`
- `POST /api/config`
- `GET /api/status`
- `POST /api/portal/complete`
- `POST /api/restart`
- `GET /portal/open`
- `ANY /portal/proxy*`
- `GET /*`

它同时承担：

- 嵌入式 HTML 配置页。
- 配置 JSON 读写。
- 状态 JSON 输出。
- 门户代理。
- 保存配置后的重启调度。

## UI 与显示层

`components/ui_service` 基于官方 BSP 和 LVGL 创建主监控屏。

当前事实：

- `monitor_dashboard_screen.c` 是主实现。
- 对外入口仍是 `wifi_info_screen_start()`。
- 使用内嵌 `components/ui_service/assets/jnr_sb_font.ttf`。
- 读取 `network_service_snapshot_t` 和 `provider_service_snapshot_t`。
- 渲染余额、USED 百分比、DELTA、小时消费、网络状态和 provider 状态。

显示链路依赖 `PSRAM`、`LVGL TinyTTF` 和 `CONFIG_LV_USE_CLIB_MALLOC=y`。

## 物理输入层

`components/board_input_service` 监听上方 BOOT 按钮。

当前行为：

- 轮询 GPIO 35。
- 短按忽略。
- 长按约 2 秒后请求进入配置 AP 路径。

这个组件不直接改配置，也不直接创建 UI，只把物理动作转换成网络服务命令。

## 板级与运行时约束

- 使用 `ESP-IDF v6.0.1`。
- target 是 `esp32p4`。
- 板载无线按 `ESP32-P4 host + ESP32-C6 coprocessor` 理解。
- 无线链路走 `ESP-Hosted + esp_wifi_remote`。
- 不启用 `CONFIG_ESP_HOST_WIFI_ENABLED`。
- 点屏、触摸和背光优先走 Waveshare BSP。
- `sdkconfig.defaults` 是可提交基线，`sdkconfig` 是本机状态。

## 结构性风险

- `config_web_service.c` 已经同时承载 HTML、REST、代理和重启调度，后续继续扩展前应考虑拆分。
- `wifi_info_screen_start()` 是遗留 API 名，容易误导为 Wi-Fi-only 页面。
- provider 抽象边界已经存在，但当前实现仍以 AQI provider 为主。
- 本地配置 API 涉及敏感字段，生产化前需要评估 readback 脱敏或认证。
