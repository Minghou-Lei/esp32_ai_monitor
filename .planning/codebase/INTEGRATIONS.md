---
last_mapped_commit: f4a155a1d23a3aa8ca4e7cb568217b35c1d5a510
mapped_at: 2026-05-17
---

# INTEGRATIONS

## 板级集成

当前工程和板卡的集成点主要在：

- `waveshare__esp32_p4_wifi6_touch_lcd_4b`
- `waveshare__esp_lcd_st7703`
- `espressif__esp_codec_dev`

当前项目仍优先复用 `BSP` 和官方组件，而不是自己维护整套裸驱动初始化序列。

## Hosted 无线集成

无线链路集成点包括：

- `espressif__esp_hosted`
- `espressif__esp_wifi_remote`
- `espressif__wifi_remote_over_eppp`
- 板载 `ESP32-C6`

对上层业务来说，这条集成链路体现在：

- `network_service` 使用 `esp_wifi` / `esp_netif` 公开接口
- 实际无线实现则由 Hosted / Remote 路线承接
- 因此排查时不能把行为简单等同于原生 `esp_wifi` 板型

## 配置系统集成

当前仓库已经完成一条比较清晰的配置流：

1. `Kconfig.projbuild`
   - 提供首启动默认值
2. `app_config_service`
   - 组装默认值、执行校验、持久化到 `NVS`
3. `config_web_service`
   - 通过网页和 REST 接口暴露配置读写
4. `network_service` / `provider_service` / `ui_service`
   - 统一从 `app_config_service` 读取运行态配置

这条链路把“构建期默认值”和“运行期配置”做了明显分层。

## 本地网页集成

当前配置网页与系统内部服务的集成点如下：

- `GET /api/config`
  - 读取 `app_config_service` 的当前配置
- `POST /api/config`
  - 走统一校验并保存配置
- `GET /api/status`
  - 聚合网络与 provider 快照
- `POST /api/portal/complete`
  - 推进 `network_service` 的门户完成状态
- `POST /api/restart`
  - 触发系统重启

页面本身是内嵌 HTML，不依赖外置静态资源服务。

## UI 与服务集成

当前板上主屏与后端服务的集成模式很直接：

- UI 不自己维护网络状态机
- UI 不自己维护 provider 抓取逻辑
- UI 只消费：
  - `network_service_snapshot_t`
  - `provider_service_snapshot_t`

这使得页面层可以专注于：

- 可读性
- 状态折叠
- 渲染性能

而不把驱动 / HTTP / 持久化混进 `LVGL` 层。

## Provider 外部接口集成

当前 `provider_service` 已经和外部监控接口做了首版集成：

- 通过 `base_url + endpoint_path` 组合目标地址
- 使用 bearer token / access token
- 使用 provider 特定的用户头
- 当前默认端点为 `/api/subscription/self`

虽然目前只实现 `AQI` provider，但边界已经通用化，后续可在不推倒 `app_config_service` 的前提下扩展其他 provider。

## 组件间关键耦合

当前最值得记录的内部集成关系有两类。

第一类是健康的扇出：

- `app_config_service` 被多个组件共同依赖
- `network_service` 与 `provider_service` 为 UI 和网页共享快照

第二类是潜在风险：

- `network_service` 的 `REQUIRES` 中包含 `provider_service`
- `provider_service` 的 `REQUIRES` 中包含 `network_service`

这代表当前存在双向组件依赖。即使当前能构建，也应在后续演进中优先考虑如何解耦。
