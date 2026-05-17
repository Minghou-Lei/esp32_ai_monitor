# DEVELOPMENT

## 当前开发阶段

当前仓库处于：

- 板级 bring-up 已落地
- Wi-Fi 状态页已落地
- 监控终端主业务尚未开始

所以当前开发原则应是：

- 先保证现有板级链路稳定
- 再进入监控协议和控制台能力

## 模块开发原则

### 保持 `main/main.c` 很薄

当前入口已经符合这个方向，后续继续坚持：

- 入口只做启动编排
- 不把业务逻辑塞回 `main`

### 新能力优先拆到 `components/`

当前已有正向示例：

- `components/network_service`
- `components/ui_service`

后续推荐补齐：

- `backend_client`
- `agent_state`
- `settings_store`
- `touch_service`
- 必要时再抽 `display_service`

## UI 开发原则

当前 UI 统一使用 `LVGL`，并依赖官方 `BSP`。

后续页面开发建议：

- 继续复用 `ui_service` 的状态驱动方式
- 优先保证可读性和错误态
- 先做页面状态管理，再做视觉层扩展

当前已证明有价值的做法包括：

- 周期性定时刷新
- 只在文本变化时更新 label
- 使用嵌入式字体资源
- 记录首帧关键 heap 快照

当前新增的重要运行时约束：

- `TinyTTF` 的首帧渲染压力必须和 `LVGL` allocator 策略一起看
- 不要默认 `LVGL` builtin 小池足够承载首帧字形缓存
- 如果继续使用 `TinyTTF`，优先保住 `CLIB malloc` 路线

## 网络开发原则

当前网络层主要是 `Wi-Fi Station` 详情采集，尚未进入业务协议。

后续协议开发建议顺序：

1. `HTTP polling`
2. 明确后端 `JSON` 模型
3. 再考虑 `WebSocket`
4. 只有后端已采用时再评估 `MQTT`

## Hosted 路线原则

当前项目必须按这个硬件结构理解：

- `ESP32-P4`
  - 主控、显示、触摸、业务
- `ESP32-C6`
  - `Wi-Fi 6 / BLE`

因此后续开发时：

- 不要把无线问题默认当成本地 `esp_wifi` 问题
- 优先从 Hosted / Remote 路线排查
- 改配置前先确认是否影响 `esp_hosted` / `esp_wifi_remote`

## 依赖管理原则

当前依赖以 `ESP-IDF` 组件管理器为主，同时有项目内 override 层。

应继续坚持：

- 依赖优先写进 `main/idf_component.yml`
- 变更后检查 `dependencies.lock`
- 只有确实需要 SDK 兼容补丁时才改 override 组件
- override 的目标是跟随官方，而不是长期自建分叉

## 配置变更原则

凡是涉及以下内容，都应同步看 `sdkconfig.defaults`：

- target
- flash size
- 分区表
- `PSRAM`
- Hosted Wi-Fi
- 显示参数
- `LWIP` 缓冲区
- `LVGL` allocator

当前仓库已经踩过的约束包括：

- 不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- Hosted 路线对内存和配置组合敏感
- UI 首帧和字体加载受 `PSRAM` 影响
- `LVGL` allocator 选择会直接影响 `TinyTTF` 是否触发首帧断言复位

## 工程动作原则

仓库 `AGENTS.md` 要求 `ESP-IDF` 工程动作优先 MCP，当前开发也应延续：

- 先检查 `project://config`
- 再尝试 `project://status`
- 构建 / 烧录动作优先 MCP
- MCP 超时或不支持时再明确回退 `idf.py`

## 文档同步原则

以下情况发生时应同步文档：

- 模块边界变化
- 依赖与 override 策略变化
- 分区与内存策略变化
- Hosted Wi-Fi 关键配置变化
- 页面结构从诊断页演进为监控页

本项目当前推荐同步位置：

- `README.md`
- `docs/*.md`
- `.planning/codebase/*.md`
- `.planning/research/*.md`

## 当前最值得推进的开发方向

在当前代码基础上，最自然的下一步不是继续堆更多 Wi-Fi 字段，而是：

1. 保持当前 UI 和配置组合稳定
2. 引入最小 `backend_client`
3. 建立“后端在线 / 离线 / 延迟 / 最近错误”的状态模型
4. 把当前页面从“Wi-Fi 详情页”演进为“监控总览页”
