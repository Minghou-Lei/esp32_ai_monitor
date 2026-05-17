# TESTING

## 当前测试现实

这个仓库目前没有完整的自动化质量护栏：

- 没有单元测试
- 没有组件测试
- 没有 CI
- 没有后端协议测试夹具

因此当前验证仍以：

- 配置核对
- 工程构建
- 上板回归

为主。

## 最小验证顺序

对当前项目，推荐的验证顺序是：

1. 确认 target / 配置 / 依赖组合
2. `reconfigure`
3. `build`
4. 如涉及显示或联网关键路径，再 `flash monitor`

仓库 `AGENTS.md` 约定工程动作优先 MCP；如果 MCP 不支持或超时，再回退 CLI。

## 当前最重要的回归点

### 显示 / UI

至少确认：

- 屏幕点亮
- 背光正常
- 页面创建成功
- 首帧不因 `TinyTTF` / `PSRAM` / allocator 组合触发复位
- 页面定时刷新稳定

### 网络

至少确认：

- `Wi-Fi` 状态能进入连接流程
- `SSID` / `IP` / `DNS` / `RSSI` / 信道字段可刷新
- 没有明显走回本地 `esp_wifi` 路线

### 依赖与配置

改了以下内容后，必须重新验证：

- `main/idf_component.yml`
- 本地 override 组件
- `dependencies.lock`
- `sdkconfig.defaults`
- 分区表

## 常用命令

Windows / PowerShell：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
```

```powershell
idf.py -C "E:\esp32_ai_monitor" build
```

```powershell
idf.py -C "E:\esp32_ai_monitor" -p COM7 flash monitor
```

## UI 路径专项关注

当前 `ui_service` 已经加入：

- `TinyTTF`
- `PSRAM` 绘图缓冲
- heap 快照日志
- 只在文本变化时更新 label

因此 UI 回归要重点看：

- `Heap[before-fonts]`
- `Heap[after-fonts]`
- `Heap[first-refresh]`
- watchdog 或 `SW_CPU_RESET`

## Hosted Wi-Fi 路径专项关注

当前项目联网链路必须保持：

- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`

如果日志出现这些异常，先查 Hosted / Remote 路线：

- `OS adapter function version error`
- `Failed to unregister Rx callbacks`
- `esp_wifi_init failed`
- `net80211` 风格错误

## 文档验证

文档刷新后，至少应检查：

- 路径是否真实存在
- `sdkconfig.defaults` 关键值是否匹配
- 组件依赖与 override 描述是否匹配 `main/idf_component.yml` / `dependencies.lock`
- 不继续保留已经失效的“环境不可用”之类旧结论
