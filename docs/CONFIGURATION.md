# CONFIGURATION

## 配置文件分层

当前项目的配置分层已经比较明确：

- `sdkconfig.defaults`
  - 可提交、可复用的持久配置基线
- `sdkconfig`
  - 当前机器的生效态
- `partitions_32mb_singleapp.csv`
  - 自定义分区表
- `.vscode/settings.json`
  - 当前工作区的本地开发设置

## 当前持久基线

`sdkconfig.defaults` 当前已经表达了这些关键意图：

- target：`esp32p4`
- flash size：`32MB`
- 自定义分区表：`partitions_32mb_singleapp.csv`
- 开启 `PSRAM`
- Hosted Wi-Fi 路线
- `LVGL TinyTTF`
- `LVGL CLIB malloc`

关键项包括：

- `CONFIG_IDF_TARGET="esp32p4"`
- `CONFIG_ESPTOOLPY_FLASHSIZE="32MB"`
- `CONFIG_PARTITION_TABLE_CUSTOM=y`
- `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_32mb_singleapp.csv"`
- `CONFIG_SPIRAM=y`
- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

## 当前生效态

`sdkconfig` 当前确认的关键事实：

- target 为 `esp32p4`
- flash size 为 `32MB`
- 分区表指向 `partitions_32mb_singleapp.csv`
- 已启用 Hosted / Wi-Fi Remote
- 当前 `LVGL` allocator 已切到 `CLIB malloc`
- 可能包含本机 `Wi-Fi` 凭据和主机名配置

注意：

- `sdkconfig` 可能被 `reconfigure` 或 `menuconfig` 重写
- 它不能替代 `sdkconfig.defaults`
- 评审时要先检查是否混入本机敏感配置

## 分区配置

当前自定义分区表文件：

- `partitions_32mb_singleapp.csv`

当前定义仍是单应用路线，适合当前阶段：

- `nvs`
- `phy_init`
- 单个较大的 `factory` app 分区

如果后续引入：

- 更大字体
- 更多图片资源
- OTA
- 更复杂日志缓存

需要重新审查分区策略。

## Kconfig 业务参数

当前 `components/network_service/Kconfig.projbuild` 已暴露：

- `CONFIG_AI_MONITOR_WIFI_SSID`
- `CONFIG_AI_MONITOR_WIFI_PASSWORD`
- `CONFIG_AI_MONITOR_WIFI_HOSTNAME`
- `CONFIG_AI_MONITOR_UI_REFRESH_MS`

这些参数当前的消费关系是：

- `network_service`
  - 使用 `SSID`、密码、主机名
- `ui_service`
  - 使用刷新周期参数驱动界面刷新

## Hosted Wi-Fi 相关约束

当前项目不是本地 `esp_wifi` 直驱板载无线的假设，而是：

- `ESP32-P4 host`
- `ESP32-C6` 协处理无线
- `ESP-Hosted + esp_wifi_remote`

因此必须坚持：

- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`

如果这里跑偏，日志里容易出现：

- `OS adapter function version error`
- `Failed to unregister Rx callbacks`
- `esp_wifi_init failed`

## UI / 内存相关约束

当前 UI 运行路径有这些重要配置结论：

- `PSRAM` 是显示路径前置条件
- `TinyTTF` 会放大首帧内存压力
- `CLIB malloc` 是当前更稳的 `LVGL` allocator 方案

这意味着：

- 不要默认 `LVGL` builtin 小内存池足够
- 改字体、改 allocator、改显示缓冲时，都要重新评估首帧风险

## 组件依赖与 override

当前 `main/idf_component.yml` 已声明这些关键依赖：

- `espressif/usb`
- `espressif/esp_codec_dev`
- `waveshare/esp_lcd_st7703`
- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `espressif/esp_wifi_remote`
- `espressif/esp_hosted`

其中这些依赖当前通过仓库内 override 使用：

- `components/espressif__esp_codec_dev`
- `components/waveshare__esp_lcd_st7703`
- `components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

变更这些依赖后，至少要同步检查：

- `main/idf_component.yml`
- `dependencies.lock`
- `sdkconfig.defaults`

## 本机工作区设置

当前 `.vscode/settings.json` 记录的本机开发设置包括：

- `idf.currentSetup = C:\esp\v6.0.1\esp-idf`
- `idf.portWin = COM7`
- `idf.openOcdConfigs = board/esp32p4-builtin.cfg`
- `idf.flashType = UART`

这些设置是当前工作区的本机辅助信息，不应替代仓库级设计文档。

## 推荐配置变更流程

当你修改这些内容时：

- 组件依赖
- 分区
- `sdkconfig.defaults`
- Hosted Wi-Fi
- 显示 / `LVGL` 相关选项

推荐流程是：

1. 审查 `sdkconfig.defaults`
2. `reconfigure`
3. `build`
4. 如涉及显示或联网关键路径，再上板回归
