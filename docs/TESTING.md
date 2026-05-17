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

在仓库根目录执行：

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
