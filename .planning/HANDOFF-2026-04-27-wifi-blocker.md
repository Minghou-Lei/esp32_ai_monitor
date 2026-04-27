## Handoff

项目：`E:\esp32_ai_monitor`

日期：`2026-04-27`

当前状态：

- 屏幕已经正常点亮。
- `PSRAM` 已经成功初始化，日志出现：
  - `Found 32MB PSRAM device`
  - `SPI SRAM memory test OK`
- `Waveshare BSP` 的显示初始化已经成功，日志出现：
  - `Display initialized`
- 运行期不会再因为 `MIPI DPI framebuffer` 申请失败而黑屏重启。
- 当前新的主阻塞点已经从“显示内存不足”转移到“Wi-Fi 启动失败”。

本轮已确认有效的改动：

- 增加了 `esp_wifi_remote` 和 `esp_hosted` 依赖，用于 `ESP32-P4` 作为 host、板载 `ESP32-C6` 作为无线协处理器的两芯片方案。
- 修正了 `esp_hosted` host 侧若干头文件缺少 `esp_wifi_he.h` 导致的 `TWT` 相关类型编译错误。
- 调整了显示启动路径：
  - 使用 `bsp_display_start_with_config(...)`
  - 将 `LVGL` draw buffer 改为放入 `PSRAM`
  - 首帧文本刷新改为异步定时器触发，避免 `main` 任务栈溢出
- `app_main()` 现在会记录显示/网络启动错误，而不是直接因为 `ESP_ERROR_CHECK` 中止。

当前运行日志结论：

- 显示相关日志正常：
  - `ESP32_P4_4B: Display initialized`
  - `GT911: TouchPad_ID:0x39,0x31,0x31`
  - `ESP32_P4_4B: Setting LCD backlight: 100%`
- Wi-Fi 相关阻塞日志：
  - `W (2542) wifi_init: Failed to unregister Rx callbacks`
  - `E (2542) wifi_init: Failed to deinit Wi-Fi driver (0x3001)`
  - `E (2552) wifi_init: Failed to deinit Wi-Fi (0x3001)`
  - `E (2552) network_service: network_service_start(413): esp_wifi_init failed`
  - `E (2562) app_main: Failed to start Wi-Fi service: ESP_ERR_INVALID_ARG`

当前最可能的问题方向：

1. `network_service_start()` 仍然按“本地直连 Wi-Fi 驱动”的时序在初始化：
   - `esp_netif_init()`
   - `esp_event_loop_create_default()`
   - `esp_netif_create_default_wifi_sta()`
   - `esp_wifi_init()`
   - `esp_wifi_set_mode(WIFI_MODE_STA)`
   - `esp_wifi_set_config(...)`
   - `esp_wifi_start()`
2. 但当前工程已经切到 `ESP-Hosted + esp_wifi_remote` 路线，板载无线不是 `ESP32-P4` 原生 Wi-Fi。
3. 因此下一轮需要核对：
   - 当前 `ESP-Hosted` / `Wi-Fi Remote` 配置下，`esp_wifi_init()` 前是否还需要显式等待 transport ready
   - `esp_netif_create_default_wifi_sta()` 与 host Wi-Fi 模式是否兼容
   - 是否应改为参考官方 `wifi/iperf`、`esp_hosted` host 示例的初始化顺序
   - 当前 `esp_wifi_deinit failed (0x3001)` 是否意味着前一轮驱动状态脏、或 hosted remote glue 未完全 ready

建议下一轮直接排查的文件：

- [network_service.c](E:/esp32_ai_monitor/components/network_service/network_service.c)
- [main.c](E:/esp32_ai_monitor/main/main.c)
- [idf_component.yml](E:/esp32_ai_monitor/main/idf_component.yml)
- [esp_hosted_api.c](E:/esp32_ai_monitor/managed_components/espressif__esp_hosted/host/api/src/esp_hosted_api.c)
- [esp_wifi_remote example iperf](C:/esp/v5.5.4/esp-idf/examples/wifi/iperf/main/iperf_example_main.c)

建议下一轮优先做的事：

1. 对比官方 `ESP32-P4` Wi-Fi hosted 示例初始化顺序，确认 `network_service_start()` 是否少了 transport-ready / hosted-ready 检查。
2. 在 `network_service_start()` 里把每一步拆得更细，分别打印：
   - `esp_netif_init`
   - `esp_event_loop_create_default`
   - `esp_netif_create_default_wifi_sta`
   - `esp_wifi_init`
   - `esp_wifi_set_mode`
   - `esp_wifi_set_config`
   - `esp_wifi_start`
3. 如果 `esp_wifi_init` 仍失败，继续确认：
   - 当前 `SDK Configuration editor` 中 `Host WiFi Enable`
   - `ESP-Hosted config -> Choose the Co-processor to use = ESP32-C6`
   - `Wi-Fi Remote -> choose slave target = esp32c6`
4. 不要回退当前显示修复。显示链路已经正常。

当前仓库内与本轮问题直接相关的文件：

- [main/main.c](E:/esp32_ai_monitor/main/main.c)
- [main/idf_component.yml](E:/esp32_ai_monitor/main/idf_component.yml)
- [components/network_service/network_service.c](E:/esp32_ai_monitor/components/network_service/network_service.c)
- [components/network_service/include/network_service.h](E:/esp32_ai_monitor/components/network_service/include/network_service.h)
- [components/ui_service/wifi_info_screen.c](E:/esp32_ai_monitor/components/ui_service/wifi_info_screen.c)
- [components/ui_service/include/wifi_info_screen.h](E:/esp32_ai_monitor/components/ui_service/include/wifi_info_screen.h)
- [sdkconfig.defaults](E:/esp32_ai_monitor/sdkconfig.defaults)

一句话总结：

显示链路已经通，`PSRAM` 已生效，当前需要把 `Wi-Fi` 初始化从“普通 `esp_wifi` 直连思路”收敛到“`ESP32-P4 host + ESP32-C6 hosted/remote` 正确时序”。
