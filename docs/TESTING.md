<!-- generated-by: gsd-doc-writer -->
# TESTING

## 当前验证现实

当前仓库没有自动化测试套件或 CI。未检测到：

- `tests/`
- `test/`
- Unity 组件测试。
- GitHub Actions 或其他 CI 工作流。
- provider fixture backend。

因此验证主要依赖：

- 文档与源码事实核对。
- `reconfigure`。
- `build`。
- 上板 `flash monitor`。
- 手工检查 UI、网络、provider 和本地配置门户。

## MCP 优先路径

ESP-IDF 工程动作优先使用 MCP。

先读取：

- `project://config`
- `project://status`
- `project://devices`

确认：

- 项目根目录。
- `idf_version`。
- target。
- build 目录。
- 串口候选。

再执行 build 或 flash。看到 MCP 动作很快返回时，先读 `project://status` 的 operation 状态，不要误判为未执行。

## CLI 回退路径

MCP 不可用、transport 失效或工具无法完成时，使用 CLI：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
idf.py -C "E:\esp32_ai_monitor" build
idf.py -C "E:\esp32_ai_monitor" -p <PORT> flash monitor
```

回退时需要记录原因和验证结果。

## 按变更类型验证

### 文档-only

最低检查：

```powershell
git diff --check
```

另需：

- 对照当前源码和 MCP 状态核对事实。
- 扫描生成文档中的 token、密钥、本机用户路径、Wi-Fi 凭据和日志路径。

### 配置变更

最低检查：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
idf.py -C "E:\esp32_ai_monitor" build
```

另需：

- 审查 `sdkconfig.defaults`。
- 确认没有把本机 `sdkconfig` 私有值提交。

### UI / 显示变更

最低检查：

- build。
- flash。
- 观察主监控屏。
- 检查是否出现首帧复位。
- 检查字体、刷新和状态文本。

重点关注：

- `PSRAM`。
- `CONFIG_LV_USE_CLIB_MALLOC=y`。
- `CONFIG_LV_USE_TINY_TTF=y`。
- 字体资产大小。

### 网络变更

最低检查：

- build。
- flash。
- 验证连接状态。
- 验证 fallback AP。
- 验证 `/api/status`。

重点关注：

- Hosted / Wi-Fi Remote 配置。
- WPA2-Enterprise 字段。
- portal 状态。
- 断连和重连状态文本。

### Provider 变更

最低检查：

- build。
- 在网络可用时观察 provider 状态。
- 检查成功 / 失败计数。
- 检查 HTTP 状态。
- 检查 delta 和小时消费显示。

日志中不得输出 token、management key 或完整授权头。

### 配置门户变更

最低检查：

- build。
- 打开配置页。
- 调用 `GET /api/config`。
- 调用 `POST /api/config`。
- 调用 `GET /api/status`。
- 保存后确认设备重启和配置生效。

### BOOT 按钮变更

最低检查：

- build。
- flash。
- 短按确认不触发配置入口。
- 长按约 2 秒确认进入配置 AP 路径。

## 模块手工检查点

### `app_config_service`

- 默认值可装配。
- NVS 覆盖可读取。
- 保存前校验能拒绝非法配置。
- schema 变化不会误读旧 blob。

### `network_service`

- 未配置时状态清晰。
- 配置正确时能进入 connected。
- portal required / completed 状态正确。
- fallback AP 可进入。

### `provider_service`

- 网络未就绪时不误报成功。
- 凭据缺失时状态可读。
- provider 响应解析后快照稳定。
- 失败时保留可诊断状态文本。

### `config_web_service`

- JSON 字符串转义正确。
- 表单解析不会越界。
- 保存后调度重启。
- proxy body limit 生效。

### `ui_service`

- 首屏可见。
- 网络和 provider 状态刷新。
- 详情视图可读。
- 字体不触发内存问题。

## 当前缺口

- 缺少自动化单元测试。
- 缺少 provider response fixtures。
- 缺少本地 HTTP route 测试。
- 缺少 UI smoke 测试。
- 缺少 CI。
- 缺少自动化 secret scan gate。
