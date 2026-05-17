---
last_mapped_commit: f4a155a1d23a3aa8ca4e7cb568217b35c1d5a510
mapped_at: 2026-05-17
---

# TESTING

## 当前测试形态

当前仓库仍然没有：

- 单元测试
- 组件测试
- CI
- 自动化文档校验流水线

因此验证主要依赖：

- 工程级 `reconfigure`
- 工程级 `build`
- 必要时 `flash monitor`
- 人工代码核对
- 板上回归

## 与当前实现匹配的最小验证面

### 配置模型

涉及 `app_config_service` 的改动时，至少应验证：

- 默认值能从 `sdkconfig.defaults` 正常装配
- 保存前校验能拦截越界或非法输入
- 运行时保存不会破坏已有配置结构

### 网络层

涉及 `network_service` 的改动时，至少应验证：

- `STA` / `SoftAP` 模式选择正确
- 企业认证配置能通过校验
- 门户状态和网络状态不会互相覆盖
- 页面与配置网页都能读到一致快照

### Provider 层

涉及 `provider_service` 的改动时，至少应验证：

- 无凭据时能稳定退回 idle / not ready 状态
- 远端异常会反映到 `status_text` 与 HTTP 状态统计
- 增量与小时统计不会因为无效响应崩坏

### 配置网页

涉及 `config_web_service` 的改动时，至少应验证：

- `/api/config` 读写正常
- `/api/status` 输出与 UI 一致
- `/api/portal/complete` 能正确推进门户状态
- `/api/restart` 行为明确

### UI

涉及 `ui_service` 的改动时，至少应验证：

- 首帧不会因 `TinyTTF` / allocator / `PSRAM` 组合复位
- 主屏能展示网络与 provider 状态
- 刷新周期切换正常
- 文本变化检测不会卡死或漏更新

## 工程动作验证

按仓库约定，`ESP-IDF` 工程动作优先使用 MCP。当前会话的实际情况是：

- `project://config` 读取成功
- `project://status` 在资源读取阶段超时

这意味着当前不能把 “`status` 资源超时” 简化成 “MCP 不可用”。后续验证应区分：

- MCP 可用
- 某资源 / 某长耗时动作超时
- 工程本身失败

## 上板验证仍然是最终闭环

当前项目属于显示链路、Hosted Wi-Fi 链路和本地网页都参与的嵌入式系统。只通过静态代码阅读和构建成功，仍不足以证明行为正确。

对这些改动，最终仍需要上板验证：

- 显示路径
- 网络接入
- 门户状态切换
- 配置网页可达性
- provider 轮询表现

## 当前验证缺口

这次映射确认的主要测试缺口包括：

- 没有 provider 响应夹具
- 没有配置网页接口自动化回归
- 没有网络状态机自动化覆盖
- 没有 UI 内存峰值或 watchdog 回归工具链

因此每次涉及这些区域的改动，都要把“手工回归成本高”当成现实约束。
