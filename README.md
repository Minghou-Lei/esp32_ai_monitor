# esp32_ai_monitor

![ESP32 AI Monitor Dashboard](./docs/images/dashboard-live.jpg)

这是一个跑在 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B` 上的 `ESP-IDF` 项目。

先把话说清楚：它现在不是「把完整 AI Agent 塞进 ESP32 里跑」。它更像一块桌面状态屏。板子负责点亮屏幕、连网、显示状态；真正的 AI 服务、额度查询、告警策略和控制逻辑，可以放在 PC、本地服务器、NAS 或云端。

当前代码已经完成的是板级基础能力：屏幕能起来，`LVGL` 页面能刷新，Wi-Fi Station 能连接并把网络细节显示到屏幕上。后面的 AI 额度监控、Provider 轮询、配置网页、告警和控制动作，还属于下一阶段要补的产品功能。

## 现在能做什么

- 启动 Waveshare BSP 显示链路，创建 `LVGL` 界面。
- 通过 `ESP-Hosted + esp_wifi_remote` 使用板载 `ESP32-C6` 协处理器联网。
- 从 `menuconfig` 读取 Wi-Fi SSID、密码、主机名和 UI 刷新间隔。
- 在屏幕上显示 Wi-Fi 状态、SSID、主机名、STA MAC、BSSID、IPv4、网关、DNS、RSSI、信道、认证方式和加密方式。
- 监听 Wi-Fi / IP 事件，维护一份给 UI 使用的状态快照。

## 硬件和技术栈

目标硬件：

- `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`
- 主控：`ESP32-P4`
- 无线协处理器：板载 `ESP32-C6`
- 推荐使用板上的 `USB TO UART` 口烧录和看日志

工程基线：

- `ESP-IDF` 项目，不是 Arduino、PlatformIO 或 MicroPython 项目
- target：`esp32p4`
- flash：`32MB`
- 分区表：`partitions_32mb_singleapp.csv`
- 使用 `PSRAM`
- UI：`BSP + LVGL + TinyTTF`
- 无线：`ESP-Hosted + esp_wifi_remote`

注意：这块板不是「P4 原生带 Wi-Fi」。P4 负责主业务，C6 负责无线链路，所以不要随手打开 `CONFIG_ESP_HOST_WIFI_ENABLED`。

## 快速开始

准备好 `ESP-IDF` 环境后，在仓库根目录执行：

```powershell
idf.py set-target esp32p4
idf.py menuconfig
```

在 `AI Monitor Configuration` 里填：

- `Wi-Fi SSID`
- `Wi-Fi password`
- `Wi-Fi hostname`
- `Wi-Fi detail refresh interval (ms)`

然后构建、烧录、看日志：

```powershell
idf.py reconfigure
idf.py build
idf.py -p <PORT> flash monitor
```

`<PORT>` 换成你机器上的实际串口，比如 `COM5`。优先用板子的 `USB TO UART`，不要默认把 `USB OTG` 当成烧录口。

## 代码结构

```text
main/
  app_main() 入口，只负责启动 UI 和网络服务

components/ui_service/
  基于 BSP + LVGL 的 Wi-Fi 详情屏

components/network_service/
  Wi-Fi Station 生命周期、连接事件、重连和状态快照

components/waveshare__esp32_p4_wifi6_touch_lcd_4b/
components/waveshare__esp_lcd_st7703/
components/espressif__esp_codec_dev/
  本仓库锁定的板级和外设组件

docs/
  架构、配置、开发、测试和后续产品方向文档
```

当前启动顺序很简单：

1. `wifi_info_screen_start()`
2. `network_service_start()`

UI 会先创建出来，然后定时读取 `network_service_get_snapshot()` 的结果刷新屏幕。

## 常见坑

- 不要把 Wi-Fi 密码、Provider token 之类的真实敏感信息提交到仓库。
- `sdkconfig.defaults` 是可提交的基线；`sdkconfig` 更像本机当前配置，改动前要确认是不是机器私有状态。
- 显示、字体、PSRAM 和 LVGL 分配器是连在一起的。遇到首帧崩溃，不要只盯 Wi-Fi。
- 改无线链路前先确认问题在 P4 侧还是 C6 协处理器侧，不要无故重刷 C6 固件。
- 涉及屏幕、网络或配置行为的改动，最终还是要上板验证。

## 验证方式

仓库目前没有单元测试和 CI。最小验证流程是：

```powershell
idf.py reconfigure
idf.py build
idf.py -p <PORT> flash monitor
```

上板后至少确认：

- 屏幕能亮，页面没有反复重启。
- 未配置 Wi-Fi 时能显示明确的未配置状态。
- 配好 Wi-Fi 后能显示连接状态、IP、DNS、RSSI 等字段。
- 断网和重连时状态文本会更新。

## 后续文档

- [Getting Started](./docs/GETTING-STARTED.md)
- [Architecture](./docs/ARCHITECTURE.md)
- [Configuration](./docs/CONFIGURATION.md)
- [Development](./docs/DEVELOPMENT.md)
- [Testing](./docs/TESTING.md)
- [AI Agent Monitor Proposal](./docs/ai-agent-monitor-proposal.md)
