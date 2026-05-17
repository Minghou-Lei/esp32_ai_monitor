---
last_mapped_commit: 24911b142360e77d719d9db5cfc54443770da247
mapped_at: 2026-05-17
---

# TESTING

## 当前测试现实

这个仓库目前没有完善的自动化测试护栏：

- 没有单元测试
- 没有组件测试
- 没有 CI
- 没有后端协议测试夹具

因此当前验证仍以工程构建和上板回归为主。

## 当前可用验证层级

### 1. 配置与工程状态验证

当前会话已验证：

- `ESP-IDF MCP` 资源已注册
- `project://config` 可读
- `project://config` 返回：
  - `project_path = E:/esp32_ai_monitor`
  - `idf_version = v6.0.1`
  - `target = esp32p4`
  - `build_dir = E:/esp32_ai_monitor/build`

`project://status` 本次读取在 120 秒窗口内超时，因此当前应把它视为“资源读取超时”，而不是直接视为“工程状态异常”。

### 2. 构建产物验证

当前 `build/` 目录存在这些关键产物：

- `esp32_ai_monitor.elf`
- `esp32_ai_monitor.bin`
- `esp32_ai_monitor.map`
- `compile_commands.json`
- `project_description.json`

从时间戳看，最近一轮成功镜像生成时间是：

- `2026-05-17 10:52:40`

这说明工程至少在当前依赖 / 配置组合下已经成功出过镜像。

### 3. 板级回归验证

对本项目来说，真正高价值的验证不是只看编译成功，而是这些上板验收动作：

1. 屏幕点亮
2. 背光正常
3. 页面创建成功
4. 首帧不因 `TinyTTF` / allocator / `PSRAM` 路径复位
5. `Wi-Fi` 状态能进入连接流程
6. `SSID` / `IP` / `DNS` / `RSSI` / 信道等字段能稳定刷新

## 当前推荐验证命令

按仓库约定，优先 MCP；若需要 CLI，则 Windows / PowerShell 下的最小命令为：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
```

```powershell
idf.py -C "E:\esp32_ai_monitor" build
```

```powershell
idf.py -C "E:\esp32_ai_monitor" -p COM7 flash monitor
```

如果是 agent 执行：

- 先确认 MCP 是否可用
- 再决定是否退回 CLI
- 回退时记录原因

## UI 路径的专项验证点

当前 `wifi_info_screen.c` 已经引入更复杂的字体与布局逻辑，因此 UI 回归应重点看：

- `Heap[before-fonts]` / `Heap[after-fonts]` / `Heap[first-refresh]` 日志
- 首帧是否触发 `SW_CPU_RESET`
- 页面定时刷新是否稳定
- 是否因重复重排大字号文本触发 watchdog

这部分验证比传统“能不能点亮屏幕”更重要，因为它直接覆盖了当前最脆弱的运行时链路。

## Hosted Wi-Fi 路径的专项验证点

联网验证时优先确认：

- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
- Hosted / Remote 路线没有回退到本地 `esp_wifi`

如果日志出现这些现象，要先查配置路线而不是先改业务代码：

- `OS adapter function version error`
- `Failed to unregister Rx callbacks`
- `esp_wifi_init failed`
- `net80211` 风格异常

## 文档验证约定

对于 `.planning/codebase` 和 `docs/` 的刷新，当前最小验证是：

- 内容与源码路径一致
- 关键配置值与 `sdkconfig.defaults` / `project://config` 一致
- 不继续保留“环境不可用”这类已被当前事实推翻的断言

因为没有自动化 verifier 常驻，本仓库的文档正确性仍需要靠：

- 源码核对
- 配置核对
- 构建 / 上板事实回归

三者闭环。
