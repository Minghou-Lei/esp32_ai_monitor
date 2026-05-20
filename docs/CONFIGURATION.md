<!-- generated-by: gsd-doc-writer -->
# CONFIGURATION

## 配置分层

当前项目配置分为四层：

- `sdkconfig.defaults`
  - 可提交、可复用的固件配置基线。
- `sdkconfig`
  - 当前机器的生效态，已被 `.gitignore` 忽略。
- `components/app_config_service/Kconfig.projbuild`
  - 应用级运行时默认值入口。
- NVS
  - 板上运行时覆盖，由 `app_config_service` 持久化。

不要把 `sdkconfig` 中的本机值复制到 `sdkconfig.defaults` 或文档。

## 固件基线

`sdkconfig.defaults` 当前表达的关键硬约束：

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

这些配置共同定义：

- `ESP32-P4` target。
- `32MB` flash。
- 自定义单应用分区表。
- `PSRAM`。
- Hosted Wi-Fi Remote。
- LVGL TinyTTF 和 CLIB allocator。

## 分区表

当前分区表：

- `partitions_32mb_singleapp.csv`

用途：

- `nvs`
- `phy_init`
- 单个较大的 `factory` app 分区

这是当前阶段的单应用路线。后续如果引入 OTA、大型图片资源、日志缓存或更大的字体资产，需要重新审查分区策略。

## 运行时配置模型

配置结构定义在：

- `components/app_config_service/include/app_config_service.h`

持久化实现位于：

- `components/app_config_service/app_config_service.c`

当前 `app_config_t` 包括：

- `wifi`
  - `ssid`
  - `password`
  - `hostname`
  - `security`
  - `eap_identity`
  - `eap_username`
  - `eap_password`
  - `portal_enabled`
  - `portal_url`
  - `portal_username`
  - `portal_password`
- `config_ap`
  - `enabled`
  - `ssid`
  - `password`
- `provider`
  - `kind`
  - `display_name`
  - `base_url`
  - `endpoint_path`
  - `access_token`
  - `management_key`
  - `user_header_name`
  - `user_header_value`
  - `refresh_interval_ms`
- `ui_refresh_interval_ms`

## Kconfig 默认值

应用级 Kconfig 默认值在：

- `components/app_config_service/Kconfig.projbuild`

关键配置域：

- Wi-Fi 接入参数。
- 企业认证参数。
- 门户 URL 和门户账号字段。
- fallback 配置 AP。
- provider endpoint、token、management key 和用户头。
- provider 刷新周期。
- UI 刷新周期。

默认值可以用于 bring-up，但不应保存真实凭据。

## 本地配置 API

配置网页和 API 由：

- `components/config_web_service/config_web_service.c`

提供的配置相关接口：

- `GET /api/config`
  - 返回当前运行时配置。
- `POST /api/config`
  - 接收表单参数，校验后写入 NVS，并调度重启。
- `GET /api/status`
  - 返回网络、门户和 provider 状态摘要。
- `POST /api/portal/complete`
  - 标记门户流程完成。
- `POST /api/restart`
  - 请求设备重启。

门户相关接口：

- `GET /portal/open`
- `ANY /portal/proxy*`
- `GET /*`

## 配置 AP

配置 AP 由 `network_service` 管理，入口包括：

- 无有效 Wi-Fi 配置时的 fallback 行为。
- `board_input_service` 监听到 BOOT 按钮长按后的配置入口。

当前 BOOT 按钮路径：

- GPIO 35。
- 约 2 秒长按。
- 短按忽略。

## 敏感信息

敏感字段：

- Wi-Fi 密码。
- 企业认证身份、用户名、密码。
- 门户用户名、密码。
- provider access token。
- provider management key。
- provider user header value。

处理规则：

- `sdkconfig` 不提交。
- 文档不记录真实值。
- 日志不输出凭据和 token。
- 生成文档和提交 diff 必须做 secret scan。
- 后续生产化前应评估 `/api/config` 的 secret readback 脱敏。

## 修改配置后的动作

修改 `sdkconfig.defaults`、分区、Hosted、Wi-Fi Remote、PSRAM、LVGL 或 Kconfig 默认值后，推荐顺序：

1. 审查 diff，确认没有私有值。
2. 通过 MCP 或 CLI 运行 `reconfigure`。
3. 运行 `build`。
4. 涉及显示、网络、provider 或 BOOT 按钮时，上板验证。

CLI 回退命令：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
idf.py -C "E:\esp32_ai_monitor" build
idf.py -C "E:\esp32_ai_monitor" -p <PORT> flash monitor
```
