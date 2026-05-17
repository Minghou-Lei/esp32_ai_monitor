<!-- generated-by: gsd-doc-writer -->
# GETTING-STARTED

## 前置条件

开始前至少需要这些条件：

- `ESP-IDF v6.0.1`
- 当前 target 为 `esp32p4`
- 可用的 `idf.py`
- 一块 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`
- 可用串口连接用于烧录与串口监视

如果你在 Codex 会话里工作，优先确认 `ESP-IDF MCP` 可用，并先读：

- `project://config`
- `project://status`

## 安装步骤

1. 克隆仓库：

```bash
git clone https://github.com/Minghou-Lei/esp32_ai_monitor.git
cd esp32_ai_monitor
```

2. 如改动了依赖、配置基线或分区设置，先刷新构建态：

```powershell
idf.py reconfigure
```

3. 构建固件：

```powershell
idf.py build
```

4. 烧录并开始串口监视：

```powershell
idf.py -p <PORT> flash monitor
```

## 首次运行

成功启动后，应该能观察到：

1. 板上屏幕点亮并进入主监控界面
2. 串口日志显示各服务启动状态
3. 设备连上网络后，主屏或本地配置页能看到网络状态
4. provider 配置完整时，主屏会开始展示 provider 状态与金额信息

如果要修改运行时配置，打开设备当前可达 IP 上的本地配置页。

## 常见问题

### 1. 改了 `sdkconfig.defaults` 但行为没变

先执行：

```powershell
idf.py reconfigure
```

再重新 `build`。仅修改文件但不 `reconfigure`，生成态不会自动同步。

### 2. 编译过了，但板上显示或联网不正常

这类改动不能只看编译结果。当前项目没有自动化测试护栏，显示链路、Hosted Wi-Fi 路线和 provider 轮询都需要上板验证。

### 3. MCP 能列出资源，但工程动作看起来没跑

当前约定是：

- `project://status` 是快速状态快照
- `build_project` / `flash_project` 可以后台执行并立即返回

因此要以 `project://status` 中的 `operation.status`、`exit_code` 和 `log_tail` 为准，而不是只看工具调用本身是否长时间阻塞。

### 4. 本地配置页能打开，但保存后不生效

当前保存逻辑会先走 `app_config_validate()`，而且保存成功后仍可能需要重启设备，尤其是 Wi-Fi 和 provider 相关改动。

## 下一步

如果你准备继续开发，接着看：

- [DEVELOPMENT.md](./DEVELOPMENT.md)
- [CONFIGURATION.md](./CONFIGURATION.md)
- [ARCHITECTURE.md](./ARCHITECTURE.md)
- [TESTING.md](./TESTING.md)
