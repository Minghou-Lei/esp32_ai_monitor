# esp32_ai_monitor

基于 `ESP-IDF` 的 `ESP32-P4` 监控终端项目，目标硬件是 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`。

当前项目定位不是“在板子上跑完整 AI Agent”，而是做一个带触摸屏的网络监控终端：

- 板子负责 `UI`、状态展示、联网和简单控制入口
- 真正的 `AI Agent` 运行在 PC、本地服务器、NAS 或云端
- 固件优先完成板级 bring-up、Wi-Fi 状态、监控链路和控制台能力

## 当前状态

仓库已经不是空工程，当前工作树已经包含：

- `ESP-IDF` 根工程与 `esp32p4` target
- `waveshare` 官方 `BSP` 路线
- `ESP-Hosted + esp_wifi_remote` 无线配置
- `components/network_service`
  - 负责 `Wi-Fi Station` 生命周期与状态快照
- `components/ui_service`
  - 负责基于 `LVGL` 的 Wi-Fi 诊断页
- 自定义 `32MB` flash 分区表与 `PSRAM` 基线配置
- 项目内 override 组件
  - `espressif__esp_codec_dev`
  - `waveshare__esp_lcd_st7703`
  - `waveshare__esp32_p4_wifi6_touch_lcd_4b`

当前主入口 `main/main.c` 会先启动 `wifi_info_screen_start()`，再启动 `network_service_start()`。

## 当前已实现能力

- 启动板级显示
- 打开背光
- 载入内嵌 `TinyTTF` 字体
- 周期性显示 Wi-Fi 状态页
- 维护并展示这些字段：
  - 状态与状态文本
  - 已配置和已连接 `SSID`
  - 主机名
  - `STA MAC` / `BSSID`
  - `IPv4` / `Netmask` / `Gateway`
  - 主 / 备 `DNS`
  - 认证模式与加密方式
  - `RSSI` 与主信道

## 当前未完成能力

这些仍未落地：

- 后端 `HTTP polling`
- Agent 心跳协议
- 日志面板
- 告警与控制动作
- 多页面监控仪表盘
- 上位机 / 云端状态对接

## 代码结构

- `main/`
  - `main.c`：启动编排
  - `idf_component.yml`：托管依赖声明与 override 路径
- `components/network_service/`
  - Wi-Fi 状态采集与快照导出
- `components/ui_service/`
  - 板级显示初始化、`LVGL` 页面创建、字体资产打包与渲染
- `docs/`
  - 项目说明文档
- `.planning/`
  - 代码库映射与研究记录

## 关键依赖

来自当前 `dependencies.lock` 的关键版本与来源：

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
  - `1.0.1`
  - 当前通过项目内 override 使用
- `waveshare/esp_lcd_st7703`
  - `1.0.5`
  - 当前通过项目内 override 使用
- `espressif/esp_wifi_remote`
  - `1.5.1`
- `espressif/esp_hosted`
  - `2.12.7`
- `espressif/esp_lvgl_port`
  - `2.8.0`
- `lvgl/lvgl`
  - `9.5.0`
- `espressif/usb`
  - `1.4.0`

## 关键配置

当前 `sdkconfig.defaults` 表达的持久基线包括：

- `CONFIG_IDF_TARGET="esp32p4"`
- `CONFIG_ESPTOOLPY_FLASHSIZE="32MB"`
- `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_32mb_singleapp.csv"`
- `CONFIG_SPIRAM=y`
- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

这代表当前项目明确依赖：

- `PSRAM`
- Hosted / Wi-Fi Remote
- `LVGL + TinyTTF + CLIB malloc`

## 本机开发环境

当前工作区记录的本机示例环境：

- `ESP-IDF` 根路径：`C:\esp\v6.0.1\esp-idf`
- 默认串口：`COM7`
- OpenOCD 配置：`board/esp32p4-builtin.cfg`

如果你使用别的机器，只需要保证：

- `idf.py --version` 返回 `ESP-IDF v6.0.1`
- target 为 `esp32p4`
- 依赖解析后本地 override 路径仍然有效

## 构建与烧录

仓库 `AGENTS.md` 约定工程动作优先走 `ESP-IDF MCP`；如需手动命令，Windows / PowerShell 下可用：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
```

```powershell
idf.py -C "E:\esp32_ai_monitor" build
```

```powershell
idf.py -C "E:\esp32_ai_monitor" -p COM7 flash monitor
```

## 开发注意事项

- 不要把 `sdkconfig` 当成唯一持久来源
- 不要把本机 `Wi-Fi` 凭据写回 `sdkconfig.defaults`
- 不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 改显示、字体或 allocator 时，重新评估 `PSRAM + TinyTTF + CLIB malloc` 组合
- 改了组件依赖、分区或 `sdkconfig.defaults` 后，至少重新 `reconfigure + build`
- 涉及显示或联网关键路径时，最终以真实上板回归为准

## 文档导航

- [docs/ARCHITECTURE.md](/E:/esp32_ai_monitor/docs/ARCHITECTURE.md)
- [docs/GETTING-STARTED.md](/E:/esp32_ai_monitor/docs/GETTING-STARTED.md)
- [docs/DEVELOPMENT.md](/E:/esp32_ai_monitor/docs/DEVELOPMENT.md)
- [docs/TESTING.md](/E:/esp32_ai_monitor/docs/TESTING.md)
- [docs/CONFIGURATION.md](/E:/esp32_ai_monitor/docs/CONFIGURATION.md)
- [docs/ai-agent-monitor-proposal.md](/E:/esp32_ai_monitor/docs/ai-agent-monitor-proposal.md)
- [.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md](/E:/esp32_ai_monitor/.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md)
