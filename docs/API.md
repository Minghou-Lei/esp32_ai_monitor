<!-- generated-by: gsd-doc-writer -->
# API

## 概览

当前仓库没有面向公网的后端 API；它提供的是板上本地配置门户接口，由 `components/config_web_service/config_web_service.c` 通过 `esp_http_server` 暴露。

这套接口的目标是：

- 让首配和维护阶段不依赖重新编译固件
- 让当前运行态可通过 HTTP 直接观察
- 保持接口数量少、便于抓包和诊断

## 认证

当前本地配置 API 没有独立鉴权层。访问控制依赖设备当前所处网络环境：

- 如果设备在 `STA` 网络下，接口跟随设备当前 IP 可达
- 如果设备启用了 fallback 配置热点，接口跟随热点侧 IP 可达

这意味着当前接口更适合 bring-up、局域网调试和受控运维场景，而不是开放网络环境。

## 端点列表

| Method | Path | Description | Auth Required |
|--------|------|-------------|---------------|
| `GET` | `/` | 返回单文件 HTML 配置页 | No |
| `GET` | `/api/config` | 读取当前运行时配置快照 | No |
| `POST` | `/api/config` | 校验并保存配置到 NVS | No |
| `GET` | `/api/status` | 读取网络与 provider 当前状态 | No |
| `POST` | `/api/portal/complete` | 将门户状态标记为已完成 | No |
| `POST` | `/api/restart` | 延迟触发设备重启 | No |

## 配置读取接口

### `GET /api/config`

返回值是一个 JSON 对象，包含当前运行时配置字段，例如：

```json
{
  "wifi_ssid": "example-ssid",
  "wifi_password": "******",
  "wifi_hostname": "esp32-monitor",
  "wifi_security": "wpa2-psk",
  "wifi_eap_identity": "",
  "wifi_eap_username": "",
  "wifi_eap_password": "",
  "wifi_portal_enabled": false,
  "wifi_portal_url": "",
  "wifi_portal_username": "",
  "wifi_portal_password": "",
  "config_ap_enabled": true,
  "config_ap_ssid": "monitor-config",
  "config_ap_password": "********",
  "provider_kind": "aqi",
  "provider_display_name": "AQI",
  "provider_base_url": "https://example.invalid",
  "provider_endpoint_path": "/api/subscription/self",
  "provider_access_token": "******",
  "provider_management_key": "******",
  "provider_user_header_name": "New-Api-User",
  "provider_user_header_value": "******",
  "provider_refresh_interval_ms": 300000,
  "ui_refresh_interval_ms": 1000
}
```

注意：

- 文档中的值使用占位符，不代表仓库内任何真实敏感信息
- 当前实现会把敏感字段原样序列化出来，因此本地部署时要控制访问面

## 配置保存接口

### `POST /api/config`

请求体类型：

```text
application/x-www-form-urlencoded
```

当前处理的表单字段包括：

- `wifi_ssid`
- `wifi_password`
- `wifi_hostname`
- `wifi_security`
- `wifi_eap_identity`
- `wifi_eap_username`
- `wifi_eap_password`
- `wifi_portal_enabled`
- `wifi_portal_url`
- `wifi_portal_username`
- `wifi_portal_password`
- `config_ap_enabled`
- `config_ap_ssid`
- `config_ap_password`
- `provider_kind`
- `provider_display_name`
- `provider_base_url`
- `provider_endpoint_path`
- `provider_access_token`
- `provider_management_key`
- `provider_user_header_name`
- `provider_user_header_value`
- `provider_refresh_interval_ms`
- `ui_refresh_interval_ms`

示例请求：

```text
wifi_ssid=corp-wifi&wifi_password=secret&wifi_hostname=esp32-monitor&provider_kind=aqi&provider_base_url=https%3A%2F%2Fexample.invalid&provider_endpoint_path=%2Fapi%2Fsubscription%2Fself&provider_access_token=token&provider_user_header_name=New-Api-User&provider_user_header_value=user&provider_refresh_interval_ms=300000&ui_refresh_interval_ms=1000
```

成功响应：

```text
Config saved. Restart device to apply Wi-Fi / provider changes.
```

校验失败时：

- HTTP 状态：`400 Bad Request`
- 响应体：`app_config_validate()` 生成的人类可读错误文本

## 状态读取接口

### `GET /api/status`

当前返回的状态 JSON 形状为：

```json
{
  "network_state": "connected",
  "network_ip": "192.168.1.10",
  "portal_state": "completed",
  "provider_state": "ready",
  "provider_status": "Subscription data updated"
}
```

字段来源：

- `network_state` - `network_service_snapshot_t.state_text`
- `network_ip` - `network_service_snapshot_t.ip`
- `portal_state` - 门户状态枚举折叠后的字符串：`disabled` / `waiting` / `required` / `completed`
- `provider_state` - `provider_service_snapshot_t.state_text`
- `provider_status` - `provider_service_snapshot_t.status_text`

## 门户完成接口

### `POST /api/portal/complete`

用途：

- 手动把门户状态推进为已完成

成功响应：

```text
Portal marked as completed.
```

失败响应：

- HTTP 状态：`409 Conflict`
- 响应体：

```text
Portal state is not active.
```

## 重启接口

### `POST /api/restart`

行为：

- 先返回响应
- 再通过一次性 `esp_timer` 在约 `400 ms` 后触发 `esp_restart()`

成功响应：

```text
Restarting...
```

## 错误语义

当前接口层没有统一 JSON 错误包络。错误响应主要是：

- `400 Bad Request`
  - 配置校验失败
- `409 Conflict`
  - 门户状态不允许推进
- `200 OK`
  - 成功类文本响应和 JSON 响应

更底层的 NVS、内存或 HTTP server 失败会通过 `ESP_RETURN_ON_ERROR` 路径返回 `esp_err_t` 语义，并通过设备日志暴露更多上下文。

## 与远端 provider 的关系

板上 API 只负责本地配置与状态读取，不转发 provider 数据源本身的原始接口。真正的远端请求由 `provider_service` 发起，其关键行为包括：

- 通过 `base_url + endpoint_path` 拼接请求 URL
- 设置 `Authorization: Bearer <token>`
- 设置可配置的用户头，默认为 `New-Api-User`
- 设置 `Accept: application/json`
- 设置 `User-Agent: ESP32-AI-Monitor/1.0`

这部分是设备对外请求，不属于设备对内的配置 API。
