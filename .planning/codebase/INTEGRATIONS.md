# Integrations

日期：2026-04-27

## 现状

当前仓库还没有实现任何运行时外部集成。

尚不存在以下内容：

- 外部 HTTP API
- WebSocket
- MQTT Broker
- 数据库
- 鉴权服务
- 云存储
- 第三方日志平台

## 已存在的开发期集成

虽然业务集成为空，但开发链路已经隐含依赖以下外部平台和资料：

### SDK 与工具链

- `ESP-IDF`
- `idf.py`
- `CMake`
- `esp32p4` 交叉编译工具链
- `VSCode` 的 `ESP-IDF` 插件与 `SDK Configuration Editor`
- 当前工作区的 `.vscode/settings.json` 已记录 `idf.currentSetup = C:\esp\v5.5.4\esp-idf`
- 在 Windows 终端中，若 `idf.py` 不在当前会话的 `PATH`，应先执行上述 `ESP-IDF` 根目录下的 `export.ps1`
- `SDK Configuration Editor` 修改的是当前 `sdkconfig`，需要结合仓库根 `sdkconfig.defaults` 和 `idf.py reconfigure` 才能形成可提交、可复现的配置基线

### 板级软件资源

- Waveshare 官方板级 `BSP`：`waveshare/esp32_p4_wifi6_touch_lcd_4b`
- 唯一依赖来源：Espressif Components `waveshare` 命名空间搜索页
- 地址：<https://components.espressif.com/components?q=namespace:waveshare>
- 不再把 GitHub 仓库或单个组件详情页作为本仓库的依赖来源基准

### 官方文档与硬件资料

- 板卡主页与开发页面
- 资料页中的主板原理图 PDF
- 资料页中的子板原理图 PDF
- `ESP32-P4` 数据手册
- `ESP32-P4` 技术参考手册

## 板级接口视角的“平台集成”

从硬件平台角度，后续会涉及这些集成点：

- `MIPI DSI` 显示链路
- `GT911` 触摸输入
- `ESP32-C6` 协处理器提供 `Wi-Fi 6 / BLE`
- 麦克风与扬声器音频链路
- `MIPI CSI` 摄像头链路
- `Micro SD`
- `Ethernet`

注意：

- 这些是硬件平台能力，不代表当前仓库已经实现了这些软件集成。
- 现阶段最优先的是显示、触摸和联网，不是把所有外设一次接齐。

## 面向 AI 监控终端的建议业务集成顺序

1. `HTTP polling` 拉取状态
2. 简单控制接口，例如重连、刷新、确认告警
3. 稳定后再考虑 `WebSocket`
4. 只有后端已明确使用时再接 `MQTT`

## 未决集成问题

- 上位 `AI Agent` 运行在本地还是云端，当前仓库尚未固化
- `Wi-Fi` 与 `Ethernet` 的优先级尚未在代码中体现
- `ESP32-C6` 固件与联网路径是否需要额外适配，当前未知
