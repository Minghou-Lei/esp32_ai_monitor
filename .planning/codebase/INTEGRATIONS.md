---
last_mapped_commit: 24911b142360e77d719d9db5cfc54443770da247
mapped_at: 2026-05-17
---

# INTEGRATIONS

## 运行时内部集成

### `main` -> `ui_service`

入口 `main/main.c` 在 `app_main()` 里首先调用：

- `wifi_info_screen_start()`

这条集成链路负责：

- 启动 `Waveshare BSP`
- 初始化 `LVGL`
- 打开背光
- 构建页面对象树
- 启动页面定时刷新

### `main` -> `network_service`

随后 `app_main()` 调用：

- `network_service_start()`

这条链路负责把网络运行时带起来，并把状态暴露给 UI 读取。

### `ui_service` -> `network_service`

`components/ui_service/wifi_info_screen.c` 周期性调用：

- `network_service_get_snapshot(&s_snapshot_cache)`

当前 UI 不直接控制 `esp_wifi`，而是完全依赖快照接口。这是当前仓库里最清晰的模块边界之一：

- 网络层生产状态
- UI 层消费状态

## BSP 与显示集成

当前显示链路优先复用：

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `waveshare/esp_lcd_st7703`
- `espressif/esp_lvgl_port`

其中 `ui_service` 对 BSP 的实际依赖包括：

- `bsp_display_start_with_config()`
- 背光控制
- `LVGL` 端口初始化

这意味着当前项目没有自建裸显示初始化层，而是把显示 bring-up 绑定在官方板级抽象之上。

## 字体与资源集成

`ui_service` 通过：

- `target_add_binary_data(${COMPONENT_LIB} "assets/jnr_sb_font.ttf" BINARY RENAME_TO jnr_sb_font_ttf)`

把字体资源打包进最终固件，然后在运行时通过 `TinyTTF` 创建字体对象。

这条链路把以下层面串起来：

- `CMake` 资源打包
- 链接后的二进制符号
- `LVGL TinyTTF`
- 运行时 `PSRAM` / heap 行为

因此字体问题不是“纯 UI 视觉问题”，而是显示、内存和调度的交叉集成点。

## Wi-Fi Hosted 集成

当前仓库的联网能力依赖以下组合：

- `espressif/esp_hosted`
- `espressif/esp_wifi_remote`
- 板载 `ESP32-C6`
- `SDIO` host interface

相关配置落在：

- `sdkconfig.defaults`
- `sdkconfig`

当前关键集成约束：

- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`

这条集成链路意味着：

- 看到联网异常时，先检查 Hosted / Remote 路线是否跑偏
- 不要默认把问题归因到本地 `esp_wifi` 业务代码

## 项目内 override 集成

当前 `main/idf_component.yml` 把多个第三方依赖指向仓库内 override：

- `../components/espressif__esp_codec_dev`
- `../components/waveshare__esp_lcd_st7703`
- `../components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

与之对应，`dependencies.lock` 已把这些依赖解析成：

- `type: local`
- `path: components\\...`

这层集成的风险和价值都很高：

- 价值：能在 `ESP-IDF v6.0.1` 下及时修兼容问题
- 风险：需要持续跟踪上游组件形状，避免仓库内长期漂移

## 配置与业务参数集成

业务配置通过 `components/network_service/Kconfig.projbuild` 暴露给 `menuconfig`：

- `AI_MONITOR_WIFI_SSID`
- `AI_MONITOR_WIFI_PASSWORD`
- `AI_MONITOR_WIFI_HOSTNAME`
- `AI_MONITOR_UI_REFRESH_MS`

运行时消费关系是：

- `network_service` 读取 `SSID` / 密码 / 主机名
- `ui_service` 读取 `CONFIG_AI_MONITOR_UI_REFRESH_MS` 驱动刷新频率

这使得当前仓库已经有一条完整的：

- `Kconfig` -> `sdkconfig` -> 组件运行时

集成链路。

## ESP-IDF 工具链集成

仓库 `AGENTS.md` 已明确要求工程动作优先走 `ESP-IDF MCP`。当前会话验证结果是：

- `project://config` 可读
- `project://devices` 已注册为资源
- `project://status` 本次读取超时

因此当前最准确的工具集成结论是：

- MCP 已挂载并部分可用
- 重资源读取可能受工具超时窗口影响
- 不能因为一次超时就把 MCP 判死
- 也不能省略 CLI 回退路径

当前稳定 CLI 路线仍然是：

- 激活 `ESP-IDF` 环境
- 再执行 `idf.py`

## 外部系统集成现状

当前真正落地的外部集成还比较少：

- `Waveshare BSP`
- `ESP-IDF` 组件注册表
- `ESP-IDF MCP`
- GitHub 远程仓库

这些能力还没有真正接入：

- 后端 `HTTP polling`
- `WebSocket`
- `MQTT`
- 监控告警服务
- 云端 Agent 状态接口

所以仓库目前更像：

- 已把板级显示和网络诊断链路打通
- 还没把业务后端链路接上
