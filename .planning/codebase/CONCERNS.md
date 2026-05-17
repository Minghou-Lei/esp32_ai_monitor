---
last_mapped_commit: 24911b142360e77d719d9db5cfc54443770da247
mapped_at: 2026-05-17
---

# CONCERNS

## 1. 当前工作树是 dirty 状态

映射时工作树并不干净，且未提交改动覆盖了关键区域：

- `components/ui_service/wifi_info_screen.c`
- `main/idf_component.yml`
- `dependencies.lock`
- `sdkconfig.defaults`
- `README.md`
- `docs/*`
- `.planning/codebase/*`

影响：

- 这些文档描述的是“当前工作树事实”
- 如果只拿 `HEAD` 理解仓库，会低估当前 UI、依赖和文档层的变化

## 2. `sdkconfig` 仍然是敏感机器态

虽然当前 `sdkconfig` 没有额外 diff，但它仍是已跟踪的本机生效态文件。

风险点：

- 可能包含本机 `Wi-Fi` 凭据
- 可能被 `reconfigure` 或 `menuconfig` 重写
- 容易被误当成设计基线

结论：

- 持久配置以 `sdkconfig.defaults` 为准
- 提交时要显式检查是否混入私有配置

## 3. MCP 可用，但不能把一次超时误判成不可用

当前会话已经验证：

- `project://config` 可读
- `project://status` 读取超时

风险不在于“MCP 没挂上”，而在于：

- agent 可能把超时误写成“工程坏了”或“MCP 不可用”
- 长耗时操作会受工具窗口限制

因此后续工程动作要区分：

- MCP 不可用
- MCP 可用但某个资源超时
- 工程本身失败

## 4. Hosted Wi-Fi 路线仍然很脆弱

本项目联网能力不是普通 `esp_wifi` 场景，而是：

- `P4 host + C6 slave`
- `ESP-Hosted + esp_wifi_remote`

高风险点仍然包括：

- `CONFIG_ESP_HOST_WIFI_ENABLED` 被误打开
- Hosted / Remote 配置组合跑偏
- 排障时沿用“本地 Wi-Fi 驱动”思路

这类偏差的代价很高，因为它会直接把问题定位带偏。

## 5. UI 的主要风险在内存和调度，不只是布局

当前 UI 已经明显复杂于最初的文本页，且依赖：

- `PSRAM`
- `TinyTTF`
- `CLIB malloc`
- 多块大字号文字面板

主要风险包括：

- 首帧字形缓存导致的内存峰值
- `taskLVGL` 长时间重排导致 watchdog
- 改绘图缓冲 / 字体 / allocator 后重新触发 `SW_CPU_RESET`

这意味着后续任何“只是改个 UI”都不应被当成低风险改动。

## 6. 本地 override 组件存在上游漂移风险

当前仓库把多个第三方依赖切到了本地 override。

风险：

- 上游版本继续演进时，本地 patch 可能逐渐脱节
- `dependencies.lock`、override 源文件和实际 SDK API 之间可能出现三方不一致

但当前阶段又不能简单去掉这些 override，因为它们正是 `ESP-IDF v6.0.1` 可编译路径的一部分。

## 7. 业务层仍然大量缺失

当前已落地的仍是：

- 显示 bring-up
- Wi-Fi 状态采集
- 诊断页渲染

未落地的关键产品层包括：

- `backend_client`
- `agent_state`
- 告警 / 控制动作
- 后端状态模型
- 多页面导航

风险在于：

- 如果文档措辞不严谨，容易把当前仓库误写成“AI 监控终端已成型”
- 实际上它还处在“诊断页 + 板级基础设施”阶段

## 8. 自动化质量护栏不足

当前没有：

- 单元测试
- 组件测试
- CI
- 文档 verifier 常驻流程

因此每次涉及显示、网络、依赖、配置的改动，都需要更依赖：

- 手工代码核对
- 构建验证
- 上板回归

没有这些闭环时，文档正确不代表固件行为正确。
