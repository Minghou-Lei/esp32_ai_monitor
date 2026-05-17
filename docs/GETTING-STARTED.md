# GETTING-STARTED

## 目标硬件

当前目标板卡是：

- `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`

开发重点是把它当成“带触摸屏的网络监控终端”，而不是本地重推理设备。

## 前置条件

开始前至少确认：

- 已安装 `ESP-IDF v6.0.1`
- 当前 target 为 `esp32p4`
- 可以解析 `main/idf_component.yml` 中声明的依赖
- 本地 override 目录存在

当前工作区的本机示例路径记录在 `.vscode/settings.json`：

- `idf.currentSetup = C:\esp\v6.0.1\esp-idf`
- 默认串口 `COM7`

如果你在另一台机器上工作，不需要复用这些绝对路径，但需要保证等价的 `ESP-IDF v6.0.1` 环境。

## 获取依赖

当前项目依赖通过 `ESP-IDF` 组件管理器解析。

主要依赖包括：

- `espressif/esp_hosted`
- `espressif/esp_wifi_remote`
- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `waveshare/esp_lcd_st7703`
- `espressif/esp_lvgl_port`
- `lvgl/lvgl`

其中三项当前通过本地 override 使用：

- `components/espressif__esp_codec_dev`
- `components/waveshare__esp_lcd_st7703`
- `components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

## 配置基线

当前推荐先接受仓库中的 `sdkconfig.defaults` 作为起点。它已经包含：

- `32MB` flash
- 自定义分区表
- `PSRAM`
- Hosted / Wi-Fi Remote
- `LVGL TinyTTF`
- `CLIB malloc`

不要把 `sdkconfig` 当成唯一基线，它是当前机器的生效态。

## 构建

按仓库约定，工程动作优先使用 `ESP-IDF MCP`。如果你是在终端手动执行，Windows / PowerShell 下可用：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
```

```powershell
idf.py -C "E:\esp32_ai_monitor" build
```

构建成功后，关键产物会出现在：

- `build/esp32_ai_monitor.elf`
- `build/esp32_ai_monitor.bin`
- `build/esp32_ai_monitor.map`

## 烧录与串口

当前工作区默认串口配置为 `COM7`，手动烧录命令为：

```powershell
idf.py -C "E:\esp32_ai_monitor" -p COM7 flash monitor
```

优先使用板上的 `USB TO UART` 口进行烧录和串口日志观察，不要默认用 `USB OTG`。

## 首次验收建议

第一次把工程跑起来时，优先确认：

1. 屏幕点亮
2. 背光正常
3. 页面对象树创建成功
4. 首帧没有因 `TinyTTF` / allocator / `PSRAM` 路径复位
5. `Wi-Fi` 能进入连接流程
6. 页面能刷新 `SSID`、`IP`、`DNS`、`RSSI`、信道等字段

## 常见误区

- 不要把这块板当成“P4 本地自带 Wi-Fi”的普通板卡
- 不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 不要默认 `LVGL` 默认内存池足够承载 `TinyTTF`
- 不要把本机 `Wi-Fi` 凭据直接沉淀进 `sdkconfig.defaults`

## 下一步阅读

- [ARCHITECTURE.md](/E:/esp32_ai_monitor/docs/ARCHITECTURE.md)
- [CONFIGURATION.md](/E:/esp32_ai_monitor/docs/CONFIGURATION.md)
- [DEVELOPMENT.md](/E:/esp32_ai_monitor/docs/DEVELOPMENT.md)
- [TESTING.md](/E:/esp32_ai_monitor/docs/TESTING.md)
