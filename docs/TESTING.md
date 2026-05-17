# TESTING

## 当前测试现实

当前仓库还没有：

- 单元测试
- 组件测试
- CI
- 自动化板级回归

因此当前验证方式以工程构建和上板验证为主。

## 最小验证流程

### 工程级

优先路径是通过 `ESP-IDF MCP` 做工程动作和状态确认：

- 先读 `project://config`
- 再读 `project://status`
- 如需上板，再读 `project://devices`
- 调用 `build_project` 或 `flash_project` 后，再次读取 `project://status`

注意：

- 当前 `build_project` / `flash_project` 可以采用后台任务模式
- 工具先返回“已启动”并不代表失败
- 验证闭环要看 `project://status` 里的 `operation.status`、`exit_code` 和 `log_tail`

如果无法使用 MCP，再回退到命令行。在仓库根目录执行：

```powershell
idf.py reconfigure
```

```powershell
idf.py build
```

### 板级

涉及显示、网络、配置网页或 provider 逻辑时，继续执行：

```powershell
idf.py -p <PORT> flash monitor
```

## 按模块的手工验证点

### `app_config_service`

- 默认值是否正确装配
- 运行时保存是否成功
- 非法输入是否被校验拦截

### `network_service`

- `STA` / `SoftAP` 路径是否正确
- 企业认证参数是否生效
- 门户状态是否能正确推进
- UI 与配置网页读取到的网络快照是否一致

### `provider_service`

- 无凭据时是否进入合理状态
- 抓取成功 / 失败统计是否更新
- HTTP 错误能否反映到状态文本
- delta 与最近成功时间是否合理

### `config_web_service`

- `/api/config` 读取与保存是否正常
- `/api/status` 是否返回最新运行态
- `/api/portal/complete` 是否能推进门户状态
- `/api/restart` 是否行为明确

### `ui_service`

- 首帧是否稳定
- 字体加载是否成功
- 网络 / provider 状态是否可见
- 文本变化刷新是否正常

## 高风险改动

以下改动即使编译通过，也不应视为低风险：

- `sdkconfig.defaults` 变化
- Hosted / Remote 相关配置变化
- `PSRAM`、字体、allocator、显示缓冲变化
- 配置网页字段或接口变化
- provider 数据模型变化

这些改动都应至少做一次上板回归。

## MCP 回归要点

每次动到工程动作链、用户级 MCP 启动脚本或全局 `Codex` 集成时，至少验证：

- `project://config` 可读
- `project://status` 能在秒级返回，而不是卡死到工具超时
- `project://devices` 能返回当前可见串口
- `build_project` 返回后，`project://status` 能看到 `operation` 状态推进
- `operation.status` 最终会收敛到 `succeeded` 或 `failed`，而不是一直悬挂

如果看到 `Transport closed`，先把它归类为 MCP 会话失效，再决定是否要重连或重开会话；不要先把锅甩给固件代码。
