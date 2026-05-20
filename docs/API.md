<!-- generated-by: gsd-doc-writer -->
# API

## 概览

本文件描述板上本地 HTTP API。实现位于：

- `components/config_web_service/config_web_service.c`

当前 API 面向本地配置和诊断，不是公网管理接口。

## 认证

当前本地 API 没有实现用户认证。它假设访问者已经处于可信本地网络、fallback 配置 AP 或现场调试环境。

涉及敏感配置的接口在生产化前应增加认证或 secret readback 脱敏。

## 路由列表

| Method | Path | 用途 |
| --- | --- | --- |
| `GET` | `/api/config` | 读取当前运行时配置 |
| `POST` | `/api/config` | 保存运行时配置并调度重启 |
| `GET` | `/api/status` | 读取网络、门户和 provider 状态摘要 |
| `POST` | `/api/portal/complete` | 标记门户流程完成 |
| `POST` | `/api/restart` | 请求设备重启 |
| `GET` | `/portal/open` | 打开或跳转到配置的门户地址 |
| `ANY` | `/portal/proxy*` | 门户代理 |
| `GET` | `/*` | 配置网页 / captive fallback |

## `GET /api/config`

读取当前运行时配置。

返回内容包括：

- Wi-Fi 基本配置。
- 企业认证字段。
- 门户配置。
- fallback 配置 AP。
- provider 配置。
- UI / provider 刷新间隔。

敏感字段风险：

- Wi-Fi 密码。
- 企业认证密码。
- 门户密码。
- provider token。
- provider management key。
- provider user header value。

当前接口用于本地配置体验，文档和日志不得记录真实返回值。

## `POST /api/config`

保存运行时配置。

处理流程：

1. 读取请求体。
2. 解析表单字段。
3. 填充 `app_config_t`。
4. 调用 `app_config_validate()`。
5. 调用 `app_config_save()` 写入 NVS。
6. 调度设备重启。

成功响应：

```text
Config saved. Device restarting to apply Wi-Fi / provider changes.
```

常见失败：

- 请求体过大。
- 表单字段无效。
- 配置校验失败。
- NVS 保存失败。
- 重启调度失败。

## `GET /api/status`

读取设备当前状态摘要。

返回 JSON 字段：

- `network_state`
- `network_ip`
- `portal_state`
- `provider_state`
- `provider_status`

这些字段来自：

- `network_service_get_snapshot()`
- `provider_service_get_snapshot()`

用途：

- 配置网页轮询显示。
- 本地诊断。
- 验证 Wi-Fi、门户和 provider 状态是否一致。

## `POST /api/portal/complete`

标记门户流程完成。

成功响应：

```text
Portal marked as completed.
```

如果当前门户状态不支持完成操作，返回：

- HTTP `409 Conflict`
- 响应文本：`Portal state is not active.`

## `POST /api/restart`

请求设备重启。

成功响应：

```text
Restarting...
```

该接口先发送响应，再调度重启。

## `GET /portal/open`

用于打开配置的门户 URL。

用途：

- 公司或网络门户登录。
- 配合 portal state 指引用户完成放行。

具体行为取决于当前 `app_config_t.wifi.portal_url` 和实现中的代理逻辑。

## `ANY /portal/proxy*`

门户代理接口。

用途：

- 代理门户请求。
- 保存或转发 cookie。
- 重写必要的 URL。

实现包含 body 和 URL 长度限制。不要把它当作通用公网反向代理。

## `GET /*`

默认 HTML / captive fallback 路由。

用途：

- 返回板上配置页面。
- 捕获未匹配路径。

## 错误语义

当前 API 以简洁文本错误为主。常见状态：

- `400 Bad Request`
  - 请求体或配置参数无效。
- `409 Conflict`
  - 门户状态不允许当前操作。
- `500` 等 ESP-IDF HTTP server 默认错误
  - 分配失败、保存失败或内部操作失败。

## 安全注意

- 当前 API 未认证。
- `/api/config` 涉及 secret readback。
- provider token、management key、Wi-Fi 密码和门户密码必须视为敏感。
- 不要把 API 响应样例中的真实值提交到仓库。
