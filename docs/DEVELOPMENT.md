<!-- generated-by: gsd-doc-writer -->
# DEVELOPMENT

## 本地开发原则

当前仓库的开发方向已经不是“单页 Wi-Fi 诊断示例”，而是围绕统一配置模型的板上监控终端。做开发时优先遵守这些原则：

- `main/main.c` 保持轻量，只做启动编排
- 业务能力优先拆到 `components/`
- 编译期默认值和运行时覆盖分层
- UI 优先可读性和稳定性
- 工程动作优先 `ESP-IDF MCP`，CLI 作为明确回退路径

## 何时修改哪个组件

### `components/app_config_service`

优先在这些场景修改它：

- 新增运行时可配置项
- 需要统一校验逻辑
- 需要把 Kconfig 默认值接入运行态
- 需要新增 NVS 持久化字段

### `components/network_service`

优先在这些场景修改它：

- Wi-Fi 接入逻辑变化
- 企业认证变化
- 门户状态管理变化
- fallback `SoftAP` 逻辑变化
- 网络快照字段变化

### `components/provider_service`

优先在这些场景修改它：

- 调整 provider 轮询节奏
- 新增或扩展 provider 字段
- 调整 HTTP 请求、错误处理或 delta 统计
- 增加手动刷新或历史聚合逻辑

### `components/config_web_service`

优先在这些场景修改它：

- 新增或修改配置页字段
- 新增本地 REST 接口
- 调整保存流程、状态输出或维护动作

### `components/ui_service`

优先在这些场景修改它：

- 主屏布局变化
- 字体与视觉层级变化
- 刷新策略变化
- 新状态字段展示

## 常用工程命令

| Command | Description |
|---------|-------------|
| `idf.py reconfigure` | 依赖、分区或配置变更后刷新生成态 |
| `idf.py build` | 构建固件 |
| `idf.py -p <PORT> flash monitor` | 烧录并串口监视 |

如果在 Codex 会话里工作，推荐的最小检查顺序是：

1. 读 `project://config`
2. 读 `project://status`
3. 如需烧录，再看 `project://devices`
4. 再执行 `build` / `flash`
5. 回头读取 `project://status` 确认 `operation` 结果

## 代码风格与组织

当前仓库没有独立的 lint / formatter 配置文件，实际约定来自已有源码：

- C 代码使用四空格缩进
- 每个组件都维持“小 public API + 多个 static helper”模式
- 公共类型与函数统一放在 `include/`
- 文件级静态变量使用 `s_` 前缀
- 公共枚举和结构体使用 `<component>_<name>_t`

新增代码时优先复制现有组件风格，不做无关格式化。

## UI 开发约束

当前 UI 已经依赖：

- `PSRAM`
- `TinyTTF`
- `LVGL`
- 多块大字号文本

因此 UI 开发时要特别注意：

- 只在文本变化时更新 label
- 不要默认小内存池足够
- 改字体、绘图缓冲或 allocator 时重新评估首帧内存峰值
- “只是改个显示文案”也可能影响刷新和内存压力

## 网络开发约束

当前无线链路不是普通本地 Wi-Fi 直驱，而是 Hosted / Remote 路线。因此：

- 不打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 不按原生片上 Wi-Fi 心智模型推断所有网络问题
- 遇到接入异常时先检查 Hosted / Remote 配置组合

## 配置开发约束

当前项目采用“编译期默认值 + 运行时覆盖”的模式。新增配置项时优先遵守这条流程：

1. 在 `components/app_config_service/Kconfig.projbuild` 增加默认值
2. 在 `app_config_service` 中加入字段、默认值装配和校验
3. 在 `config_web_service` 中暴露运行时入口
4. 按需在 UI / 网络 / provider 中消费该字段

## 分支与提交流程

当前仓库没有在仓内文档中声明分支命名约定，也没有 `.github/PULL_REQUEST_TEMPLATE.md`。因此更安全的做法是：

- 保持单次改动聚焦
- 只提交请求范围内的文件
- 提交信息直接描述这次变更意图
- 在提交前审查文档、`sdkconfig` 和日志材料里是否混入敏感信息

## PR / 评审关注点

当前评审时优先看这些点：

- 是否误把本机凭据或绝对路径写入文档
- 是否把运行时配置模型拆散到了多个组件
- 是否引入了更多 `network_service` / `provider_service` 耦合
- 是否只做了编译验证却没有说明上板验证缺口

## 文档同步要求

当前仓库已经把 `README.md`、`docs/*.md` 和 `.planning/codebase/*.md` 当作长期可消费文档。涉及这些区域的改动时：

- 以当前工作树事实为准
- 不沿用过时“Wi-Fi 详情页原型”描述
- 不写本机绝对路径、串口号、用户名、Python 虚拟环境路径或 agent 主目录细节
