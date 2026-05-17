---
last_mapped_commit: f4a155a1d23a3aa8ca4e7cb568217b35c1d5a510
mapped_at: 2026-05-17
---

# CONVENTIONS

## 入口与模块边界

### `app_main()` 保持轻量

当前 `main/main.c` 只做启动编排，不承载具体业务逻辑。这条约定已经从旧阶段继续保留下来，但启动内容已经扩展为：

- `wifi_info_screen_start()`
- `network_service_start()`
- `provider_service_start()`
- `config_web_service_start()`

后续不应把网络、HTTP、provider 或页面细节重新塞回 `main`。

### 业务能力优先拆进 `components/`

当前业务能力已经拆成四个清晰组件：

- `app_config_service`
- `network_service`
- `provider_service`
- `config_web_service`

UI 则单独留在 `ui_service`。这条边界应继续保持。

## 配置管理约定

### 构建期默认值和运行期配置分层

当前项目已经形成稳定分层：

- `sdkconfig.defaults`
  - 持久配置基线
- `sdkconfig`
  - 当前机器生效态
- `NVS`
  - 运行期覆盖存储
- `config_web_service`
  - 运行期人机入口

后续新增可配置项时，应优先接入这条链路，而不是只把字段硬编码进模块内部。

### 不把敏感值沉淀进文档或默认基线

当前配置模型已经包含多类敏感字段：

- Wi-Fi 密码
- EAP 用户名 / 密码
- 门户用户名 / 密码
- provider token
- provider 用户头值

因此约定必须明确：

- 文档不展开这些实际值
- `sdkconfig.defaults` 不写入本机私有配置
- 评审时要重点看 `sdkconfig` 与文档是否意外泄露配置细节

## UI 约定

### 统一走 `LVGL`

当前板上主视图完全基于 `LVGL` 与 `BSP`。后续页面继续沿这条路径扩展，不引入第二套 UI 体系。

### 优先状态可读性

当前主视图不是消费级炫技布局，而是围绕：

- 网络状态
- 门户状态
- provider 状态
- 额度 / 百分比 / delta
- 底部详情文本

这符合“监控终端”和“bring-up 诊断面板”的定位。

### 只在文本变化时更新 label

当前实现已经显式采用“文本没变就不调用 `lv_label_set_text()`”的策略。这个做法应继续保留，因为页面依赖 `TinyTTF` 和多块大字号文本，盲刷会放大字形缓存与布局成本。

## 网络约定

### 不把当前工程当成原生 Wi-Fi 板型

项目的无线约定仍然是：

- `ESP32-P4 host`
- `ESP32-C6` 协处理器
- `ESP-Hosted + esp_wifi_remote`

因此：

- 不打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 联网异常优先从 Hosted / Remote 组合排查
- 不要按普通 `esp_wifi` 直驱板型的直觉推断问题

### 首配与企业网络都是一等场景

`network_service` 当前已经纳入：

- `STA`
- 企业认证
- 门户元数据
- 配置热点回退

这说明后续改动不能只围绕“连上家庭 Wi-Fi”来设计。

## 配置网页约定

当前配置网页追求的是：

- 单文件
- 少接口
- 可抓包
- 能在 bring-up 阶段稳定使用

因此后续如果继续扩展配置页，应优先保持接口简单和可诊断，而不是先追求前端框架化。

## 工程操作约定

### `ESP-IDF` 工程动作优先 MCP

当前会话已确认：

- `project://config` 可读
- `project://status` 本次读取超时

因此工程操作的正确约定仍是：

- 优先 MCP
- 区分“资源超时”和“工程失败”
- MCP 不可用时明确回退 CLI，并写明原因

### 改配置后最小回归

涉及这些内容时，不能只看代码 diff：

- `sdkconfig.defaults`
- 分区表
- Hosted / Remote 组合
- UI 字体 / allocator / 显示缓冲

最小回归仍应是：

1. `reconfigure`
2. `build`
3. 涉及显示或网络关键路径时上板验证
