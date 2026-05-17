# DEVELOPMENT

## 开发原则

当前项目的开发原则是：

- 主入口保持轻量
- 业务能力优先拆到 `components/`
- 构建期默认值和运行期配置分层
- UI 以状态可读性和稳定性优先
- 工程动作优先使用 `ESP-IDF MCP`

## 何时修改哪个组件

### `app_config_service`

在这些情况下优先改它：

- 新增运行时可配置项
- 需要统一校验逻辑
- 需要把 Kconfig 默认值引入运行态
- 需要新增 NVS 持久化字段

### `network_service`

在这些情况下优先改它：

- Wi-Fi 接入逻辑变化
- 企业认证变化
- 门户状态管理变化
- 配置热点回退逻辑变化
- 网络快照字段变化

### `provider_service`

在这些情况下优先改它：

- 接入新的外部 provider
- 调整轮询周期和失败处理
- 调整快照字段或 delta 统计
- 扩展状态可视化数据

### `config_web_service`

在这些情况下优先改它：

- 新增或修改配置页字段
- 新增本地 REST 接口
- 调整页面交互或保存流程
- 增加维护动作入口

### `ui_service`

在这些情况下优先改它：

- 主屏布局变化
- 字体与视觉层级变化
- 刷新节奏变化
- 新状态字段展示

## UI 开发约束

当前 UI 已经依赖：

- `PSRAM`
- `TinyTTF`
- `LVGL`
- 多块大字号文本

所以开发时要特别注意：

- 只在文本变化时更新 label
- 不要默认小内存池足够
- 改字体、绘图缓冲或 allocator 时重新评估首帧内存峰值
- “只是改个显示文案”也可能带来刷新或布局成本变化

## 网络开发约束

当前无线链路不是普通本地 Wi-Fi 直驱，而是 Hosted / Remote 路线。因此：

- 不打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 不按原生板型的思路推断所有网络问题
- 遇到接入异常时先检查 Hosted / Remote 配置组合

## 配置开发约束

当前项目已经从“编译期配置”转向“编译期默认值 + 运行时覆盖”的模式。新增配置项时优先遵守这条流程：

1. 在 `Kconfig.projbuild` 增加默认值
2. 在 `app_config_service` 中加入字段、默认值装配和校验
3. 在 `config_web_service` 中暴露运行时入口
4. 按需在 UI / 网络 / provider 中消费该字段

## 工程动作建议

按仓库约定：

- 优先通过 MCP 做 `config`、`status`、`build`、`flash`
- MCP 不可用或超时时，再明确回退 `idf.py`
- 回退时写清原因，不做静默切换

手动操作时，在仓库根目录执行：

```powershell
idf.py reconfigure
```

```powershell
idf.py build
```

```powershell
idf.py -p <PORT> flash monitor
```

## 文档与敏感信息

当前文档必须持续贴近工作树事实，但不应包含：

- 本机绝对路径
- 固定串口号
- Wi-Fi / 门户 / provider 实际凭据

如果文档需要示例值，使用占位符而不是机器态数据。
