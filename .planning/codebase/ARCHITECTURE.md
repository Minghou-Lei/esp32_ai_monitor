---
last_mapped_commit: f4a155a1d23a3aa8ca4e7cb568217b35c1d5a510
mapped_at: 2026-05-17
---

# ARCHITECTURE

## 当前运行时结构

当前工作树已经不再是“板级显示 + Wi-Fi 详情页”的单链路原型，而是一个围绕统一运行时配置展开的监控终端骨架：

- `main/main.c`
  - 只负责启动编排
- `components/app_config_service`
  - 统一维护 Wi-Fi、门户、配置热点、provider 和 UI 刷新配置
- `components/network_service`
  - 负责 Wi-Fi 接入、企业认证、门户状态与 `SoftAP` 回退
- `components/provider_service`
  - 负责外部 provider 轮询、状态归一化和增量统计
- `components/config_web_service`
  - 负责板上配置网页与本地 REST 接口
- `components/ui_service`
  - 负责基于 `BSP + LVGL` 的主监控屏渲染

这套结构符合仓库要求的“薄入口 + 组件化拆分”，业务逻辑没有重新堆回 `main.c`。

## 启动编排

当前 `app_main()` 的启动顺序是：

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`

注意：

- `ui_service` 当前实现文件已经切到 `monitor_dashboard_screen.c`
- 但对外入口名仍沿用 `wifi_info_screen_start()`
- 文档和后续重构都要把“实现已换、公开 API 名未换”视为当前工作树事实

这条启动链反映了当前产品目标：

- 先把主屏点亮，保证设备上电后有可见反馈
- 再启动网络链路，让页面进入可诊断状态
- 再挂载 provider 轮询，让远端监控数据进入本地状态模型
- 最后启动配置网页，给首次配网和参数修正留出口

## 配置中心层

`components/app_config_service` 是当前架构的中心点。它维护一份统一的 `app_config_t`，供 UI、网络和配置网页共同消费，避免多个组件各自保存一份私有副本。

当前配置模型至少覆盖这些域：

- `wifi`
  - 基本接入参数
  - 企业认证参数
  - 门户元数据
- `config_ap`
  - `enabled`
  - `ssid`
  - `password`
- `provider`
  - `kind`
  - `display_name`
  - `base_url`
  - `endpoint_path`
  - 鉴权与身份相关参数
  - `refresh_interval_ms`
- `ui_refresh_interval_ms`

默认值来源是 `sdkconfig.defaults` / `sdkconfig`，运行时覆盖通过 `NVS` 持久化。配置保存前还会走统一校验。

## 网络接入层

`components/network_service` 当前承担的不是简单 `STA` 连网，而是“监控终端接入状态机”：

- 根据运行时配置启动 `STA`
- 支持 `WPA2-PSK` 与 `WPA2-Enterprise`
- 维护公司门户附加状态
- 在需要时开启本地配置热点
- 对外导出统一 `network_service_snapshot_t`

当前状态机的关键枚举包括：

- `NETWORK_SERVICE_MODE_STA_ONLY`
- `NETWORK_SERVICE_MODE_APSTA_FALLBACK`
- `NETWORK_SERVICE_STATE_UNCONFIGURED`
- `NETWORK_SERVICE_STATE_IDLE`
- `NETWORK_SERVICE_STATE_CONNECTING`
- `NETWORK_SERVICE_STATE_CONNECTED`
- `NETWORK_SERVICE_STATE_DISCONNECTED`
- `NETWORK_SERVICE_STATE_PORTAL_REQUIRED`
- `NETWORK_SERVICE_STATE_CONFIG_AP`

这说明网络层已经开始服务“首配、企业网、门户、诊断”这条完整路径，而不是只处理单一家庭 Wi-Fi。

## Provider 轮询层

`components/provider_service` 负责把外部监控源折叠成一个板上可消费的快照。当前接口已经明显做成了通用 provider 形状：

- `provider_service_start()`
- `provider_service_get_snapshot()`
- `provider_service_request_refresh()`

当前真正实现的 provider 只有 `APP_CONFIG_PROVIDER_AQI`，但模块边界已经不再暴露 AQI 专有命名。它会：

- 从统一配置读取 `base_url`、`endpoint_path`、token 和用户头
- 通过 `esp_http_client` 轮询远端接口
- 解析订阅列表
- 维护抓取次数、成功次数、失败次数、最近 HTTP 状态
- 计算“自上次成功以来”的增量
- 维护按小时统计的历史窗口

当前默认端点回退值是 `/api/subscription/self`。

## 配置网页层

`components/config_web_service` 把“设备可配置”从串口 / `menuconfig` 前移到了板上本地网页。当前已注册的接口包括：

- `GET /`
  - 返回单文件 HTML 配置页
- `GET /api/config`
  - 读取当前配置快照
- `POST /api/config`
  - 校验并保存配置
- `GET /api/status`
  - 返回网络与 provider 运行状态
- `POST /api/portal/complete`
  - 把门户状态标记为已完成
- `POST /api/restart`
  - 触发设备重启

它的定位很明确：

- 把首次配网和参数修正从固件编译期搬到运行期
- 所有保存动作仍走 `app_config_service` 的统一校验逻辑
- 页面保持单文件、接口保持少量，便于抓包与 bring-up 阶段调试

## UI 与显示层

`components/ui_service` 当前已经演化成“板上主监控屏”，但代码里仍保留 `wifi_info_screen_*` 命名。当前实现要点：

- 通过 `Waveshare BSP` 启动显示
- 使用内嵌 `TinyTTF` 字体
- 创建主状态行、次状态行和底部详情区
- 同时展示网络、门户和 provider 状态
- 支持 provider 手动刷新
- 首次刷新使用较短定时，随后切换到配置中的 UI 刷新间隔
- 只在文本变化时更新 `lv_label`

从字段命名看，当前主屏关注点已经是：

- `network_status`
- `portal_status`
- `provider_status`
- `badge_label`
- `badge_subtitle`
- `numeric_value_label`
- `details_label`

这和“设备可配置、网络可诊断、provider 可观察”的产品方向一致。

## 依赖关系特征

当前最重要的依赖关系不是某个第三方库，而是项目内部围绕 `app_config_service` 形成的扇出：

- `network_service` 依赖 `app_config_service`
- `provider_service` 依赖 `app_config_service`
- `config_web_service` 依赖 `app_config_service`
- `ui_service` 依赖 `app_config_service`

同时还出现了一个需要长期盯住的耦合面：

- `network_service` 依赖 `provider_service`
- `provider_service` 依赖 `network_service`

当前工程能解析这组组件关系，但这已经是后续继续演进时需要优先关注的边界风险。

## 板级与 Hosted 约束

当前体系仍建立在这些硬约束上：

- `ESP-IDF v6.0.1`
- `esp32p4`
- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `ESP-Hosted + esp_wifi_remote`
- `PSRAM`
- `LVGL + TinyTTF + CLIB malloc`

其中无线链路的正确理解仍然是：

- `ESP32-P4` 做主控与显示
- 板载 `ESP32-C6` 提供无线协处理
- 主工程走 Hosted / Remote 路线，而不是本地原生 Wi-Fi 直驱

## 当前阶段判断

截至这次映射，项目更准确的阶段是：

- 已完成：板级显示、统一配置模型、网络接入状态机、provider 轮询骨架、本地配置网页、主监控屏
- 未完成：后端多 provider 适配、更完整的控制动作、多页面导航、自动化测试和 CI

所以它已经跨过早期单功能诊断原型阶段，但还没有进入“完整 AI 监控终端产品化”阶段。
