<!-- generated-by: gsd-doc-writer -->
# esp32_ai_monitor

基于 `ESP-IDF` 的 `ESP32-P4` 板上监控终端，面向 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`。当前固件提供主监控仪表盘、本地配置门户、Wi-Fi / 门户状态管理、远端 provider 轮询和 BOOT 按钮配置入口。

![ESP32 AI Monitor Live Dashboard](./docs/images/dashboard-live.jpg)

## 项目定位

- 板子负责 UI、触摸交互、联网、状态展示、配置入口和轻量控制。
- 真正的 AI / 监控后端运行在 PC、本地服务器、NAS 或云端。
- 当前主线是“板上监控屏 + 本地配置门户 + provider 状态展示”，不是在板上直接跑重型 AI Agent。

## 当前已实现能力

- 基于 `waveshare/esp32_p4_wifi6_touch_lcd_4b` BSP 启动显示链路。
- 使用 `LVGL + TinyTTF` 渲染主监控屏。
- 通过 `ESP-Hosted + esp_wifi_remote` 使用板载 `ESP32-C6` 无线链路。
- 通过 `app_config_service` 维护统一运行时配置模型。
- 通过 NVS 保存运行时配置覆盖。
- 支持 Wi-Fi、企业认证、门户元数据、fallback 配置热点、provider 端点和刷新周期。
- 通过 `provider_service` 轮询远端 provider，并展示余额、使用比例、delta 和状态文本。
- 通过 `config_web_service` 提供板上本地配置网页和 REST API。
- 通过 `board_input_service` 监听上方 BOOT 按钮长按，进入配置 AP 路径。

## 运行时架构

当前 `app_main()` 的启动顺序是：

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`
5. `board_input_service_start()`

核心组件：

- `components/app_config_service`
  - 统一运行时配置模型、默认值装配、校验与 NVS 持久化。
- `components/network_service`
  - STA、企业认证、门户状态与 SoftAP 回退。
- `components/provider_service`
  - 外部 provider 轮询、状态快照、delta 与小时统计。
- `components/config_web_service`
  - 板上配置网页、本地 REST API 和门户代理。
- `components/ui_service`
  - BSP + LVGL 主监控屏。
- `components/board_input_service`
  - BOOT 按钮长按检测和配置 AP 入口。

注意：UI 主实现文件是 `components/ui_service/monitor_dashboard_screen.c`，但公开启动符号仍沿用 `wifi_info_screen_start()`。

## 目录结构

- `main/`
  - 应用入口与顶层组件依赖声明。
- `components/`
  - 项目服务组件和 BSP override 组件。
- `docs/`
  - 项目说明、架构、配置、开发、测试和 API 文档。
- `.planning/codebase/`
  - GSD 代码库映射资料。
- `.planning/research/`
  - 板卡、ESP-Hosted 和 bring-up 研究记录。
- `sdkconfig.defaults`
  - 可提交的持久配置基线。
- `partitions_32mb_singleapp.csv`
  - 当前 32MB 单应用分区表。

## 安装与准备

前置条件：

- `ESP-IDF v6.0.1`
- `esp32p4` target
- 一块 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`
- 可用 UART 烧录端口
- 优先可用的 ESP-IDF MCP 项目资源

克隆仓库：

```powershell
git clone https://github.com/Minghou-Lei/esp32_ai_monitor.git
cd esp32_ai_monitor
```

检查工程状态时优先读取 MCP：

- `project://config`
- `project://status`
- `project://devices`

CLI 回退命令：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
idf.py -C "E:\esp32_ai_monitor" build
idf.py -C "E:\esp32_ai_monitor" -p <PORT> flash monitor
```

## 本地配置入口

配置门户由 `config_web_service` 提供。主要接口：

- `GET /api/config`
- `POST /api/config`
- `GET /api/status`
- `POST /api/portal/complete`
- `POST /api/restart`
- `GET /portal/open`
- `ANY /portal/proxy*`
- `GET /*`

物理入口：

- 长按上方 BOOT 按钮约 2 秒，触发配置 AP 路径。

## 关键配置基线

当前仓库硬约束：

- `target = esp32p4`
- `32MB flash`
- 自定义分区表 `partitions_32mb_singleapp.csv`
- `PSRAM`
- `LVGL + TinyTTF + CLIB malloc`
- `ESP-Hosted + esp_wifi_remote`

这些配置主要由 `sdkconfig.defaults` 表达。`sdkconfig` 是本机生效态，已被 `.gitignore` 忽略，不能作为可提交设计基线。

## 敏感信息约定

项目涉及这些敏感字段：

- Wi-Fi 密码
- 企业认证凭据
- 门户用户名 / 密码
- provider token
- provider management key
- provider 用户头值

约束：

- 不把实际值写入文档。
- 不把本机敏感项复制进 `sdkconfig.defaults`。
- 不提交 `sdkconfig`、日志、构建目录或本机状态路径。
- 提交前扫描生成文档和 diff。

## 验证现实

当前仓库没有单元测试、组件测试或 CI。低风险文档变更以文档事实核对、secret scan 和 `git diff --check` 为主要验证。固件行为变更至少需要构建；涉及显示、Wi-Fi、provider、配置门户或 BOOT 按钮时需要上板验证。

## 相关文档

- [docs/GETTING-STARTED.md](./docs/GETTING-STARTED.md)
- [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)
- [docs/API.md](./docs/API.md)
- [docs/CONFIGURATION.md](./docs/CONFIGURATION.md)
- [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md)
- [docs/TESTING.md](./docs/TESTING.md)
- [docs/ai-agent-monitor-proposal.md](./docs/ai-agent-monitor-proposal.md)
