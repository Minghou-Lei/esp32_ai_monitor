# ARCHITECTURE

## 产品定位

本项目面向 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`，当前推荐架构不是“板上直接运行完整 AI Agent”，而是：

- 板子负责触摸交互、联网、状态展示和简单控制
- 真正的 AI Agent 在 PC、本地服务器、NAS 或云端运行
- 固件优先做“监控终端”和“诊断控制台”

这意味着当前仓库的目标是先做：

- 稳定的板级显示
- 可靠的网络状态
- 可维护的页面状态管理
- 后续可接入的后端状态拉取与控制动作

## 当前固件层次

### 1. 启动入口

入口是 `main/main.c`，当前只做两件事：

1. `wifi_info_screen_start()`
2. `network_service_start()`

`app_main()` 保持轻量是当前仓库的核心约束之一。

### 2. 网络服务层

`components/network_service` 负责：

- 初始化 `NVS`
- 初始化默认事件循环
- 创建默认 `STA` `netif`
- 设置主机名
- 启动 `Wi-Fi Station`
- 处理连接 / 断开 / 获取 IP 事件
- 聚合 `SSID`、`BSSID`、`IP`、`DNS`、`MAC`、`RSSI`、信道和认证信息

对外暴露：

- `network_service_start()`
- `network_service_get_snapshot()`

它不创建任何 UI，只提供状态快照。

### 3. UI / 显示层

`components/ui_service` 负责：

- 启动 `Waveshare BSP`
- 打开背光
- 初始化 `LVGL`
- 从嵌入式 `TTF` 资源创建字体
- 创建 Wi-Fi 诊断页对象树
- 定时拉取网络快照并刷新页面

当前 UI 已经不是简单的单块文本页，而是由：

- 主状态面板
- 辅助状态面板
- 数字展示区
- 详情文本区

组成的诊断型页面。

## 当前外部依赖结构

### `ESP-IDF` 与工具链

- `ESP-IDF v6.0.1`
- `esp32p4`
- `CMake + Ninja`
- `riscv32-esp-elf-gcc`

### Board Support / UI

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `waveshare/esp_lcd_st7703`
- `espressif/esp_lvgl_port`
- `lvgl/lvgl`

### Wireless / Hosted

- `espressif/esp_hosted`
- `espressif/esp_wifi_remote`
- 板载 `ESP32-C6`

## Hosted Wi-Fi 设计结论

这块板的联网链路应理解为：

- `ESP32-P4` 负责主控与 UI
- `ESP32-C6` 提供 `Wi-Fi 6 / BLE`
- 主工程通过 `ESP-Hosted + esp_wifi_remote` 使用无线

所以当前架构约束是：

- 不要把无线能力当成 `P4` 原生片上 Wi-Fi
- 不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 排查联网问题时先检查 Hosted / Remote 路线是否跑偏

## 配置与内存架构结论

当前 `sdkconfig.defaults` 已经把这些设计意图固定下来：

- `32MB flash`
- 自定义分区表
- `PSRAM`
- Hosted / Wi-Fi Remote
- `LVGL TinyTTF`
- `CLIB malloc`

这意味着当前显示路径依赖：

- `PSRAM`
- `TinyTTF`
- `CLIB malloc`

而不是最小内存池配置。

## 当前缺失的产品层

当前仓库已经完成：

- 显示 bring-up
- Wi-Fi 状态采集
- 诊断型页面

但还没有完成：

- `backend_client`
- `agent_state`
- 后端 `HTTP polling`
- 告警 / 控制动作
- 多页面监控仪表盘

所以当前更准确的阶段判断是：

- 已跨过空工程阶段
- 仍处于板级 bring-up 和 Wi-Fi 诊断页阶段

## 推荐后续扩展顺序

1. 保持当前 Wi-Fi 详情页作为诊断页
2. 增加最小 `backend_client`
3. 引入监控状态模型
4. 在当前诊断页之上增加总览页
5. 再接入告警和控制入口
