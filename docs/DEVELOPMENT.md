<!-- generated-by: gsd-doc-writer -->
# DEVELOPMENT

## 本地开发原则

- 保持 `main/main.c` 很薄。
- 新功能优先进入 `components/` 下的独立组件。
- 显示、触摸和背光优先使用 Waveshare BSP。
- 无线链路按 `ESP-Hosted + esp_wifi_remote` 处理。
- 运行时配置统一经过 `app_config_service`。
- 文档和代码都不要固化真实凭据、本机用户目录、日志路径或私有串口假设。

## 组件边界

### `main`

只做启动编排。当前 `app_main()` 启动：

1. UI
2. Network
3. Provider
4. Config web
5. Board input

不要把业务逻辑塞进 `main/main.c`。

### `app_config_service`

修改这些内容时进入这里：

- 配置 schema。
- Kconfig 默认值。
- NVS 读写。
- 配置校验。
- 字符串与枚举转换。

新增配置字段后，同步检查 `config_web_service`、`network_service`、`provider_service` 和文档。

### `network_service`

修改这些内容时进入这里：

- STA 连接。
- WPA2-PSK / WPA2-Enterprise。
- portal state。
- fallback 配置 AP。
- 网络快照字段。

不要把 Wi-Fi 状态直接写进 UI 或配置网页。

### `provider_service`

修改这些内容时进入这里：

- provider HTTP 请求。
- provider 响应解析。
- provider 状态机。
- delta 和小时统计。
- 新 provider 类型。

当前真实 provider 是 AQI。新增 provider 前应先明确解析和快照兼容策略。

### `config_web_service`

修改这些内容时进入这里：

- 本地配置网页。
- `/api/config`。
- `/api/status`。
- `/api/portal/complete`。
- `/api/restart`。
- `/portal/open` 和 `/portal/proxy*`。

注意它目前已经较大，新增复杂逻辑时优先保持局部函数清晰。

### `ui_service`

修改这些内容时进入这里：

- LVGL 页面布局。
- 主仪表盘显示。
- 字体和颜色。
- 网络 / provider 快照呈现。
- 详情视图和触摸交互。

当前实现文件是 `monitor_dashboard_screen.c`，公开入口仍是 `wifi_info_screen_start()`。

### `board_input_service`

修改这些内容时进入这里：

- BOOT 按钮轮询。
- 长按阈值。
- 物理按钮触发的配置 AP 行为。

当前短按被忽略，长按约 2 秒触发配置入口。

## 常用工程命令

优先使用 ESP-IDF MCP 完成工程动作。CLI 回退命令：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
idf.py -C "E:\esp32_ai_monitor" build
idf.py -C "E:\esp32_ai_monitor" -p <PORT> flash monitor
```

修改依赖、分区、`sdkconfig.defaults`、Kconfig 或组件注册后，先 `reconfigure` 再 `build`。

## 配置开发流程

新增运行时配置字段：

1. 修改 `components/app_config_service/include/app_config_service.h`。
2. 修改 `components/app_config_service/Kconfig.projbuild`。
3. 在 `app_config_service.c` 中设置默认值。
4. 在 `app_config_validate()` 中增加校验。
5. 在 `config_web_service.c` 中处理 JSON / 表单读写。
6. 更新使用该字段的服务。
7. 更新 `docs/CONFIGURATION.md` 和 `docs/API.md`。
8. secret scan。

敏感字段默认不应出现在日志中。

## UI 开发约束

- 统一使用 LVGL。
- 优先保持可读性、状态清晰和刷新稳定。
- 使用 snapshot 数据，不直接访问网络或 provider 内部状态。
- 改字体、图片、布局缓冲或 allocator 后必须考虑 PSRAM 和首帧内存压力。
- 显示链路变更需要上板验证。

## 网络开发约束

- 不按 P4 原生 Wi-Fi 假设开发。
- Hosted / Wi-Fi Remote 是当前无线主线。
- 企业认证、portal、fallback AP 状态都属于 `network_service`。
- `network_service_snapshot_t` 是对 UI 和 Web 层的公开状态面。

## Provider 开发约束

- 当前 provider polling 是 HTTP。
- 先保持 HTTP polling 可抓包、可复现、可诊断。
- 新 provider 不应破坏现有 AQI 快照字段。
- provider token、management key、用户头值都按敏感信息处理。

## 文档同步要求

这些变更必须同步文档：

- 启动顺序变化。
- 新组件或组件职责变化。
- 新 REST 路由。
- 新运行时配置字段。
- `sdkconfig.defaults`、分区、Hosted、PSRAM、LVGL 相关变化。
- 验证路径变化。

GSD 代码库映射文件在 `.planning/codebase/`。

## 提交流程

提交前最低检查：

- `git status --short`
- `git diff --check`
- secret scan 生成文档和 staged diff
- 只 stage 本次相关文件
- 不 stage `sdkconfig`、日志、构建目录、`managed_components/` 或本机临时文件

文档-only 变更不需要用固件构建代替文档事实核对；代码或配置变更必须运行对应构建和上板验证。
