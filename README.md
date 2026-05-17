# esp32_ai_monitor

![ESP32 AI Monitor Dashboard](./docs/images/dashboard-live.jpg)

> A touch-enabled ESP32-P4 monitor for AI / quota / network status, built for the Waveshare 4-inch round display board.  
> 一个跑在 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B` 上的触摸监控终端，用来展示 AI / 配额 / 网络状态，而不是在板子上直接跑完整 Agent。

中文 | [English](#english)

## 中文

### 这是什么

这是一个基于 `ESP-IDF` 的板端监控终端项目，目标硬件是 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`。

它解决的问题很直接：

- 让一块带触摸屏的 `ESP32-P4` 板子变成桌面监控屏
- 实时显示额度、剩余百分比、倒计时、增量消耗、小时消耗
- 同时提供本地配置入口，方便现场改 Wi‑Fi、Provider 和刷新策略

这个仓库的目标不是“在板子上直接运行完整 AI Agent”。  
更准确地说，它是一个本地状态看板 + 配置门户：

- 板子负责 UI、联网、状态可视化、简单控制
- 真正的 AI / 监控后端运行在 PC、本地服务器、NAS 或云端

### 现在能做什么

当前版本已经能稳定跑出一套可用的板端主界面，包含：

- 当前剩余额度
- 到期倒计时 `T-HH:MM:SS`
- `Avail %` 剩余额度百分比
- `DELTA` 自上次成功抓取以来的金额变化
- `$ / Hour` 最近一小时消耗
- 底部网络 / Provider / 诊断状态区

同时，项目已经具备这些工程能力：

- 基于官方 `waveshare` `BSP` 跑通显示、背光和触摸
- 基于 `LVGL + TinyTTF` 渲染主监控屏
- 基于 `ESP-Hosted + esp_wifi_remote` 跑通 `ESP32-P4 + ESP32-C6` 无线链路
- 提供统一配置模型，覆盖 Wi‑Fi、企业认证、门户、Provider、UI 刷新间隔
- 提供本地配置网页和 REST 接口
- 支持 Provider 轮询、状态归一化和板端展示

### 为什么这个仓库值得看

和“点亮屏幕”演示不一样，这个项目已经不只是 bring-up demo。

它更接近一个真实可用的嵌入式监控终端骨架：

- 有明确的产品定位
- 有可运行的主界面
- 有网络接入与回退逻辑
- 有 Provider 数据拉取和状态归一化
- 有本地配置网页
- 有文档化的架构、配置和测试约束

如果你要做的是：

- AI / API 配额监控屏
- 家庭服务器 / NAS 状态屏
- 本地服务运行状态面板
- 嵌入式触摸配置终端

这个仓库已经是一个不错的起点。

### 当前架构

当前工作树主要拆成 5 层：

1. 启动编排层
   `main/main.c`
2. 配置中心层
   `components/app_config_service`
3. 网络接入层
   `components/network_service`
4. Provider 轮询层
   `components/provider_service`
5. 本地交互层
   `components/config_web_service`
   `components/ui_service`

当前 `app_main()` 的启动顺序是：

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`

说明：

- 主 UI 实现已经在 `components/ui_service/monitor_dashboard_screen.c`
- 公开入口名仍保留 `wifi_info_screen_start()`
- 这是命名过渡态，不影响当前运行

### 硬件和技术路线

当前基线不是 Arduino，也不是 PlatformIO / MicroPython 路线，而是：

- `ESP-IDF v6.0.1`
- `esp32p4`
- `32MB flash`
- 自定义分区表
- `PSRAM`
- `LVGL + TinyTTF + CLIB malloc`
- `ESP-Hosted + esp_wifi_remote`

这块板子的无线能力要这样理解：

- `ESP32-P4` 负责主控、显示和业务逻辑
- 板载 `ESP32-C6` 负责无线协处理

所以不要把它当成“P4 原生本地 Wi‑Fi 板型”来开发，也不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED`。

### 目录一览

- `main/`
  应用入口与组件依赖声明
- `components/app_config_service/`
  统一运行时配置模型、校验和 `NVS` 持久化
- `components/network_service/`
  `STA`、企业认证、门户状态、`SoftAP` 回退
- `components/provider_service/`
  Provider 轮询、状态快照、delta 与小时统计
- `components/config_web_service/`
  板上配置网页与本地 REST 接口
- `components/ui_service/`
  `BSP + LVGL` 主监控屏
- `docs/`
  架构、配置、开发与测试文档

### 快速开始

如果你已经有 `ESP-IDF` 环境，最短路径是：

```powershell
idf.py reconfigure
```

```powershell
idf.py build
```

```powershell
idf.py -p <PORT> flash monitor
```

建议：

- 优先使用板载 `USB TO UART` 口做烧录和串口观察
- 不要默认用 `USB OTG` 当烧录口
- 改了显示、字体、Provider、网络或配置逻辑后，最终都以上板回归为准

### 当前验证方式

这个仓库目前还没有：

- 单元测试
- 组件测试
- CI

所以当前验证闭环主要靠：

1. `idf.py build`
2. 真机 `flash`
3. 串口启动日志
4. 屏幕实拍观察
5. 配置网页手工回归

### 敏感信息约定

仓库设计上已经会接触到这些敏感字段：

- Wi‑Fi 密码
- 企业认证凭据
- 门户用户名 / 密码
- Provider token
- Provider 用户头值

因此这份 README 和仓库文档遵守这些原则：

- 不写本机绝对路径
- 不写当前串口号
- 不写实际 Wi‑Fi / 门户 / Provider 凭据
- 不把本机敏感项回写到 `sdkconfig.defaults`

### 进一步阅读

- [docs/GETTING-STARTED.md](./docs/GETTING-STARTED.md)
- [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)
- [docs/CONFIGURATION.md](./docs/CONFIGURATION.md)
- [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md)
- [docs/TESTING.md](./docs/TESTING.md)

---

## English

### What This Repo Is

This is an `ESP-IDF` project for the `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`.

Its purpose is simple:

- turn an `ESP32-P4` touch display board into a desk-side monitoring screen
- show live balance, remaining quota, expiry countdown, delta usage, and hourly spend
- provide a local configuration entry point for Wi‑Fi, provider settings, and refresh strategy

This repository is not trying to run a full AI agent on the board itself.  
It is better described as a local status dashboard plus configuration portal:

- the board handles UI, networking, visualization, and small control actions
- the real AI / monitoring backend runs on a PC, local server, NAS, or cloud system

### What It Already Does

The current firmware already runs a usable on-device dashboard with:

- current remaining balance
- expiry countdown in `T-HH:MM:SS`
- `Avail %` remaining quota percentage
- `DELTA` amount change since the last successful fetch
- `$ / Hour` spending over the latest one-hour window
- a bottom diagnostic strip for network / provider / runtime state

The project also already includes:

- display, backlight, and touch bring-up through the official `waveshare` `BSP`
- a dashboard rendered with `LVGL + TinyTTF`
- wireless connectivity via `ESP-Hosted + esp_wifi_remote` on the `ESP32-P4 + ESP32-C6` split design
- a unified runtime config model for Wi‑Fi, enterprise auth, portal settings, provider settings, and UI refresh timing
- a local configuration web UI and REST endpoints
- provider polling and normalized board-side status presentation

### Why This Repo Matters

This is no longer just a screen bring-up demo.

It is already a practical embedded monitoring-terminal skeleton with:

- a clear product direction
- a running dashboard UI
- real network access and fallback logic
- provider-side polling and normalized snapshots
- a local configuration web portal
- architecture, configuration, development, and testing documentation

If you want to build:

- an AI / API quota monitor
- a home server or NAS dashboard
- a local service status screen
- an embedded touch-first configuration terminal

this repo is already a strong starting point.

### Current Architecture

The codebase is split into five main runtime layers:

1. Boot orchestration
   `main/main.c`
2. Configuration center
   `components/app_config_service`
3. Network access
   `components/network_service`
4. Provider polling
   `components/provider_service`
5. Local interaction
   `components/config_web_service`
   `components/ui_service`

Current `app_main()` startup order:

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`

Notes:

- the main UI implementation already lives in `components/ui_service/monitor_dashboard_screen.c`
- the public entry name still keeps the older `wifi_info_screen_start()` symbol
- this is only a naming transition, not a runtime problem

### Hardware and Technical Baseline

This project is intentionally built around:

- `ESP-IDF v6.0.1`
- `esp32p4`
- `32MB flash`
- a custom partition table
- `PSRAM`
- `LVGL + TinyTTF + CLIB malloc`
- `ESP-Hosted + esp_wifi_remote`

The wireless architecture must be understood correctly:

- `ESP32-P4` handles the main application, display, and business logic
- the onboard `ESP32-C6` acts as the wireless coprocessor

So this should not be treated like a single-chip native P4 Wi‑Fi project, and `CONFIG_ESP_HOST_WIFI_ENABLED` should stay disabled.

### Repo Layout

- `main/`
  application entry point and component dependency declarations
- `components/app_config_service/`
  unified runtime configuration, validation, and `NVS` persistence
- `components/network_service/`
  `STA`, enterprise auth, portal state, and `SoftAP` fallback
- `components/provider_service/`
  provider polling, normalized snapshots, delta, and hourly stats
- `components/config_web_service/`
  on-device configuration web UI and local REST APIs
- `components/ui_service/`
  `BSP + LVGL` dashboard UI
- `docs/`
  architecture, configuration, development, and testing documents

### Quick Start

If your `ESP-IDF` environment is ready, the shortest path is:

```powershell
idf.py reconfigure
```

```powershell
idf.py build
```

```powershell
idf.py -p <PORT> flash monitor
```

Recommendations:

- use the onboard `USB TO UART` port for flashing and serial logs
- do not assume `USB OTG` is the flashing port
- for display, fonts, provider, networking, or config changes, real hardware verification is still the final gate

### Current Validation Model

This repo does not yet have:

- unit tests
- component tests
- CI

So the current validation loop mainly depends on:

1. `idf.py build`
2. real hardware flashing
3. boot log inspection
4. visual dashboard verification
5. manual config-web regression

### Sensitive Data Rules

The project design already involves potentially sensitive runtime fields:

- Wi‑Fi passwords
- enterprise auth credentials
- portal usernames / passwords
- provider tokens
- provider-specific user header values

So this README and the repo docs follow these rules:

- no machine-specific absolute paths
- no hard-coded serial port numbers
- no real Wi‑Fi / portal / provider credentials
- no copying machine-local sensitive values back into `sdkconfig.defaults`

### More Docs

- [docs/GETTING-STARTED.md](./docs/GETTING-STARTED.md)
- [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)
- [docs/CONFIGURATION.md](./docs/CONFIGURATION.md)
- [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md)
- [docs/TESTING.md](./docs/TESTING.md)
