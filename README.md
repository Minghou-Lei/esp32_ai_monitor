# esp32_ai_monitor

中文 | [English](#english)

基于 `ESP-IDF` 的 `ESP32-P4` 监控终端项目，目标硬件为 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`。  
当前项目定位不是“在板子上直接运行完整 AI Agent”，而是构建一个带触摸屏的本地监控与配置终端：板子负责 UI、联网、状态展示、配置入口与少量控制动作，真正的 AI / 监控后端运行在 PC、本地服务器、NAS 或云端。

![ESP32 AI Monitor Live Dashboard](./docs/images/dashboard-live.jpg)

## 中文

### 项目定位

- 面向 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`
- 基于 `ESP-IDF v6.0.1`
- 当前以“板上监控屏 + 本地配置门户”为核心目标
- 优先保证 bring-up、状态可观测、配置可维护、现场可运维

### 当前已实现能力

- 基于 `waveshare` 官方 `BSP` 启动显示、背光与触摸链路
- 基于 `LVGL + TinyTTF` 渲染主监控屏
- 使用 `ESP-Hosted + esp_wifi_remote` 跑通 `ESP32-P4 + ESP32-C6` 无线链路
- 统一管理 Wi‑Fi、企业认证、门户、provider 与 UI 刷新配置
- 提供本地配置网页和 REST 接口
- 轮询外部 provider，并在板上展示额度、剩余、增量和小时消费等状态

### 当前主界面

当前板上主界面聚焦“监控总览 + 诊断入口”，主要展示：

- 当前可用额度
- 倒计时剩余时间
- `Avail %` 剩余额度百分比
- `DELTA` 自上次成功抓取以来的金额变化
- `$ / Hour` 最近一小时金额消耗
- 底部网络、provider 与诊断状态文本

### 运行时架构

当前工作树已经形成这几层运行时分工：

1. 启动编排层：`main/main.c`
2. 配置中心层：`components/app_config_service`
3. 网络接入层：`components/network_service`
4. Provider 轮询层：`components/provider_service`
5. 本地交互层：
   `components/config_web_service`
   `components/ui_service`

当前 `app_main()` 的启动顺序是：

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`

注意：`ui_service` 的主实现已经迁移到 `monitor_dashboard_screen.c`，但对外入口名仍沿用 `wifi_info_screen_start()`。

### 目录结构

- `main/`
  应用入口与组件依赖声明
- `components/app_config_service/`
  统一运行时配置模型、默认值装配与 `NVS` 持久化
- `components/network_service/`
  `STA`、企业认证、门户状态与 `SoftAP` 回退
- `components/provider_service/`
  外部 provider 轮询、状态快照、delta 与小时统计
- `components/config_web_service/`
  板上配置网页与本地 REST 接口
- `components/ui_service/`
  `BSP + LVGL` 主监控屏
- `docs/`
  项目说明、开发、测试与架构文档
- `.planning/`
  代码映射、研究记录与工程分析资料

### 硬约束与设计基线

当前仓库的关键基线包括：

- `target = esp32p4`
- `32MB flash`
- 自定义分区表 `partitions_32mb_singleapp.csv`
- `PSRAM`
- `LVGL + TinyTTF + CLIB malloc`
- `ESP-Hosted + esp_wifi_remote`

无线链路的正确理解是：

- `ESP32-P4` 负责主控、UI 与业务逻辑
- 板载 `ESP32-C6` 负责无线协处理

不要把该项目按“P4 本地原生 Wi‑Fi 板型”理解，也不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED`。

### 构建与烧录

按仓库约定，`ESP-IDF` 工程动作优先使用 MCP；手动在 PowerShell 中操作时，可在仓库根目录执行：

```powershell
idf.py reconfigure
```

```powershell
idf.py build
```

```powershell
idf.py -p <PORT> flash monitor
```

建议优先使用板载 `USB TO UART` 口进行烧录与串口调试，不要默认使用 `USB OTG`。

### 配置模型

当前配置按四层理解：

- `sdkconfig.defaults`
  可提交、可复用的构建期默认基线
- `sdkconfig`
  当前机器的生效态
- `NVS`
  运行时配置覆盖
- `config_web_service`
  运行时的人机配置入口

运行时统一配置目前覆盖：

- Wi‑Fi 接入参数
- 企业认证参数
- 门户元数据
- 配置热点参数
- Provider 端点与鉴权信息
- UI 刷新周期

### 当前验证方式

当前仓库还没有单元测试、组件测试或 CI，因此主要验证方式是：

1. `idf.py build`
2. 上板 `flash`
3. 串口启动日志检查
4. 屏幕实际显示检查
5. 配置网页与 provider 轮询手工回归

涉及显示、无线、配置网页或 provider 数据模型的改动，不应只看编译结果，最终仍要以上板回归为准。

### 敏感信息约定

仓库当前已经涉及多类敏感字段，例如：

- Wi‑Fi 密码
- 企业认证凭据
- 门户用户名 / 密码
- Provider token
- Provider 用户头值

因此：

- 不把本机实际值写入文档
- 不把敏感项写回 `sdkconfig.defaults`
- 评审 `sdkconfig`、日志和网页返回值时优先检查是否泄露

### 相关文档

- [docs/GETTING-STARTED.md](./docs/GETTING-STARTED.md)
- [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)
- [docs/CONFIGURATION.md](./docs/CONFIGURATION.md)
- [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md)
- [docs/TESTING.md](./docs/TESTING.md)

---

## English

### Project Positioning

This repository targets the `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B` and is built on `ESP-IDF v6.0.1`.

The current goal is not to run a full AI agent on the board itself. Instead, this project builds a local touch-enabled monitoring and configuration terminal:

- the board handles UI, networking, status visualization, configuration entry points, and small control actions
- the actual AI / monitoring backend runs on a PC, local server, NAS, or cloud environment
- the firmware prioritizes bring-up, observability, maintainability, and field operability

### Current Capabilities

- Display, backlight, and touch bring-up through the official `waveshare` `BSP`
- Main dashboard rendered with `LVGL + TinyTTF`
- Wireless connectivity through `ESP-Hosted + esp_wifi_remote` on the `ESP32-P4 + ESP32-C6` split architecture
- Unified runtime configuration for Wi‑Fi, enterprise auth, portal metadata, provider settings, and UI refresh intervals
- On-device configuration web UI and local REST endpoints
- External provider polling with board-side visualization for balance, remaining quota, delta usage, and hourly spending

### Current Dashboard

The current dashboard is designed as a compact monitoring overview plus diagnostic entry point. It focuses on:

- current remaining balance
- countdown-to-expiry
- `Avail %` remaining quota percentage
- `DELTA` amount change since the last successful fetch
- `$ / Hour` spending within the latest one-hour window
- bottom diagnostic text for network, provider, and runtime state

### Runtime Architecture

The current codebase is structured into these runtime layers:

1. Boot orchestration: `main/main.c`
2. Configuration center: `components/app_config_service`
3. Network access layer: `components/network_service`
4. Provider polling layer: `components/provider_service`
5. Local interaction layer:
   `components/config_web_service`
   `components/ui_service`

Current `app_main()` startup order:

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`

Note: the main UI implementation already lives in `monitor_dashboard_screen.c`, while the public entry point still keeps the legacy `wifi_info_screen_start()` name.

### Repository Layout

- `main/`
  Application entry point and component dependency declaration
- `components/app_config_service/`
  Unified runtime config model, default loading, and `NVS` persistence
- `components/network_service/`
  `STA`, enterprise auth, portal state, and `SoftAP` fallback
- `components/provider_service/`
  External provider polling, normalized snapshots, delta, and hourly stats
- `components/config_web_service/`
  On-device configuration web UI and local REST APIs
- `components/ui_service/`
  Main dashboard built with `BSP + LVGL`
- `docs/`
  Architecture, configuration, development, and testing documents
- `.planning/`
  Codebase mapping, research notes, and engineering analysis artifacts

### Hard Constraints and Baseline

The current project baseline includes:

- `target = esp32p4`
- `32MB flash`
- custom partition table `partitions_32mb_singleapp.csv`
- `PSRAM`
- `LVGL + TinyTTF + CLIB malloc`
- `ESP-Hosted + esp_wifi_remote`

The wireless architecture must be understood correctly:

- `ESP32-P4` handles the main application, UI, and business logic
- the onboard `ESP32-C6` acts as the wireless coprocessor

Do not treat this repository as a native single-chip P4 Wi‑Fi project, and do not enable `CONFIG_ESP_HOST_WIFI_ENABLED`.

### Build and Flash

Per repository convention, `ESP-IDF` engineering actions should prefer MCP first.  
When using PowerShell manually from the repo root, the common commands are:

```powershell
idf.py reconfigure
```

```powershell
idf.py build
```

```powershell
idf.py -p <PORT> flash monitor
```

Use the onboard `USB TO UART` port for flashing and serial logging by default. Do not assume `USB OTG` is the flashing path.

### Configuration Model

Configuration is currently layered as:

- `sdkconfig.defaults`
  committed and reusable build-time baseline
- `sdkconfig`
  effective state on the current machine
- `NVS`
  runtime overrides
- `config_web_service`
  runtime human-facing configuration entry point

The current runtime config model covers:

- Wi‑Fi access parameters
- enterprise authentication parameters
- portal metadata
- config AP parameters
- provider endpoint and auth settings
- UI refresh interval

### Current Validation Approach

The repository does not yet have unit tests, component tests, or CI. Validation is therefore centered on:

1. `idf.py build`
2. board flashing
3. serial boot log inspection
4. real screen verification
5. manual regression for the config web UI and provider polling path

For changes that affect display, wireless, config web flows, or provider data modeling, a successful build alone is not enough. Real hardware verification remains the final gate.

### Sensitive Data Rules

The repository already deals with sensitive runtime fields, such as:

- Wi‑Fi passwords
- enterprise authentication credentials
- portal usernames / passwords
- provider tokens
- provider-specific user header values

Therefore:

- do not write machine-specific real values into documentation
- do not copy sensitive values back into `sdkconfig.defaults`
- review `sdkconfig`, logs, and web responses for accidental leakage

### Related Documents

- [docs/GETTING-STARTED.md](./docs/GETTING-STARTED.md)
- [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)
- [docs/CONFIGURATION.md](./docs/CONFIGURATION.md)
- [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md)
- [docs/TESTING.md](./docs/TESTING.md)
