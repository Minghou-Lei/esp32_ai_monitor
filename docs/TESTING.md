<!-- generated-by: gsd-doc-writer -->
# TESTING

## 当前验证现实

当前仓库没有这些自动化护栏：

- 单元测试
- 集成测试
- 组件测试
- CI
- `.github/workflows/`

因此当前验证模式不是“跑测试套件”，而是：

1. 刷新生成态
2. 构建固件
3. 如涉及显示、网络、provider 或配置网页，必须上板验证

## 基本验证命令

### 刷新配置与依赖

```powershell
idf.py reconfigure
```

### 构建

```powershell
idf.py build
```

### 上板回归

```powershell
idf.py -p <PORT> flash monitor
```

## MCP 路径验证

如果当前会话接入了 `ESP-IDF MCP`，最小验证顺序是：

1. 读 `project://config`
2. 读 `project://status`
3. 如需烧录，再读 `project://devices`
4. 触发 `build` 或 `flash`
5. 再读一次 `project://status`

要确认的重点是：

- `target`
- `idf_version`
- `build_dir`
- `operation.status`
- `exit_code`
- `log_tail`

## 按模块的手工验证点

### `app_config_service`

- 默认值是否正确装配
- 运行时保存是否成功
- 非法输入是否被校验拦截
- 重置默认值后是否恢复到编译期基线

### `network_service`

- `STA` / fallback `SoftAP` 路径是否正确
- 企业认证参数是否生效
- 门户状态是否能正确推进
- UI 与配置网页读取到的网络快照是否一致

### `provider_service`

- 无凭据时是否进入合理状态
- 抓取成功 / 失败统计是否更新
- HTTP 错误能否反映到 `state_text` / `status_text`
- delta 与小时消费统计是否合理

### `config_web_service`

- `GET /api/config` 读取是否正确
- `POST /api/config` 校验和保存是否正确
- `GET /api/status` 是否返回最新运行态
- `POST /api/portal/complete` 是否能推进门户状态
- `POST /api/restart` 是否会按预期延迟重启设备

### `ui_service`

- 首帧是否稳定
- 字体加载是否成功
- 网络 / provider 状态是否可见
- 刷新节奏下是否出现抖动或卡顿

## 高风险改动的最低验证要求

以下改动即使编译通过，也不应视为验证完成：

- `sdkconfig.defaults` 变化
- Hosted / Remote 相关配置变化
- `PSRAM`、字体、allocator、显示缓冲变化
- 配置网页字段或接口变化
- provider 数据模型变化

这些改动都至少要做一次上板回归。

## 当前缺口

由于没有自动化测试，当前最大的验证风险是：

- 配置模型变化可能静默破坏保存/恢复逻辑
- provider 返回格式变化可能静默破坏解析
- UI、网络、配置网页三套观察面可能发生漂移
- 文档可能看起来正确，但固件行为并不正确

因此每次声称“完成”前，都要明确说明：

- 做了哪些验证
- 哪些没有验证
- 下一步最小验证是什么
