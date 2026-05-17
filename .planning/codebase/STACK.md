---
last_mapped_commit: 24911b142360e77d719d9db5cfc54443770da247
mapped_at: 2026-05-17
---

# STACK

## 语言与构建系统

- 语言：`C`
- 构建系统：`CMake` + `Ninja`
- 固件框架：`ESP-IDF v6.0.1`
- 目标芯片：`esp32p4`
- 工具链：`riscv32-esp-elf-gcc`

当前工程不是 Arduino、PlatformIO 或 MicroPython 路线，而是完整的 `ESP-IDF` 工程。

## 工程入口与组件注册

- 根工程入口：`CMakeLists.txt`
- 应用入口：`main/main.c`
- 主组件注册：`main/CMakeLists.txt`
- 依赖声明：`main/idf_component.yml`

业务组件的注册方式是标准 `idf_component_register(...)`：

- `main`
  - `REQUIRES network_service ui_service`
- `network_service`
  - `REQUIRES esp_event esp_netif esp_wifi lwip nvs_flash`
- `ui_service`
  - `REQUIRES network_service waveshare__esp32_p4_wifi6_touch_lcd_4b`

## UI / 显示技术栈

- `LVGL`
  - 来自 `lvgl/lvgl`
  - 当前锁定版本：`9.5.0`
- `espressif/esp_lvgl_port`
  - 当前锁定版本：`2.8.0`
- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
  - 当前依赖声明：`^1.0.1`
  - 通过项目内 override 使用
- `waveshare/esp_lcd_st7703`
  - 当前依赖声明：`1.0.5`
  - 通过项目内 override 使用
- 字体路径
  - `components/ui_service/assets/jnr_sb_font.ttf`
- 运行时字体方案
  - `TinyTTF`

内存相关栈约束已经写进配置：

- `CONFIG_SPIRAM=y`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

这说明当前显示路径默认依赖：

- `PSRAM`
- `CLIB malloc`
- `TinyTTF`

而不是最小化 `LVGL` 内存池路线。

## 网络与无线技术栈

- `esp_event`
- `esp_netif`
- `esp_wifi`
- `lwIP`
- `nvs_flash`

但这个项目对无线的真实理解不是“P4 本地直驱 Wi-Fi”，而是：

- `ESP32-P4` 作为主控 host
- 板载 `ESP32-C6` 作为无线协处理器
- `ESP-Hosted + esp_wifi_remote` 为主线

当前关键依赖版本：

- `espressif/esp_wifi_remote`
  - `1.5.1`
- `espressif/esp_hosted`
  - `2.12.7`
- `espressif/usb`
  - `1.4.0`

当前配置也明确了这一路线：

- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`

## 组件管理器与 override 栈

当前项目同时使用：

- `managed_components/`
  - 保存托管依赖解析结果
- `dependencies.lock`
  - 保存锁文件
- `main/idf_component.yml`
  - 保存版本约束与 override_path

其中当前最值得注意的是本地 override 组件：

- `components/espressif__esp_codec_dev`
- `components/waveshare__esp_lcd_st7703`
- `components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

`dependencies.lock` 已反映出：

- `espressif/esp_codec_dev` 现在是 `type: local`
- `waveshare` 相关依赖使用本地路径

说明这个仓库已经不是“纯 registry 消费者”，而是有一层受控的本地兼容补丁栈。

## 配置栈

持久设计基线在 `sdkconfig.defaults`，当前关键项包括：

- `32MB flash`
- `partitions_32mb_singleapp.csv`
- `PSRAM`
- Hosted / Wi-Fi Remote
- `LWIP` 大缓冲
- `LVGL TinyTTF`
- `CLIB malloc`

当前暴露给业务开发的 `Kconfig` 参数位于
`components/network_service/Kconfig.projbuild`：

- `CONFIG_AI_MONITOR_WIFI_SSID`
- `CONFIG_AI_MONITOR_WIFI_PASSWORD`
- `CONFIG_AI_MONITOR_WIFI_HOSTNAME`
- `CONFIG_AI_MONITOR_UI_REFRESH_MS`

## 开发环境与工具栈

当前工作区记录的开发环境要点：

- `ESP-IDF` 根路径：`C:\esp\v6.0.1\esp-idf`
- OpenOCD 配置：`board/esp32p4-builtin.cfg`
- 默认串口：`COM7`
- `clangd` 已接 `build/compile_commands.json`

当前会话还验证了：

- `ESP-IDF MCP` 资源可见
  - `project://config`
  - `project://devices`
- `project://config` 可正常读取
- `project://status` 本次读取在工具窗口内超时

因此工具栈的真实结论是：

- MCP 可用，但长耗时读取或操作要防超时
- CLI 仍然是必须保留的显式回退路径

## 当前产品层栈

如果按产品能力分层，当前只完成到这里：

- 板级显示
- 字体资产
- Wi-Fi 状态采集
- 诊断型 UI

尚未进入：

- 后端 `HTTP polling`
- 任务状态模型
- 告警 / 控制协议
- 持久设置存储
- 自动化测试 / CI
