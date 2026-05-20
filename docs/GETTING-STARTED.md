<!-- generated-by: gsd-doc-writer -->
# GETTING STARTED

## 前置条件

- Windows PowerShell 或等价 shell。
- `ESP-IDF v6.0.1`。
- `esp32p4` target。
- `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`。
- 可用 UART 烧录端口。
- 优先可用的 ESP-IDF MCP 项目资源。

当前仓库默认目标是 `ESP32-P4` 主控和板载 `ESP32-C6` Wi-Fi 协处理链路。不要按 P4 原生 Wi-Fi 项目处理。

## 获取代码

```powershell
git clone https://github.com/Minghou-Lei/esp32_ai_monitor.git
cd esp32_ai_monitor
```

## 确认工程事实

优先读取 ESP-IDF MCP：

- `project://config`
- `project://status`
- `project://devices`

需要确认：

- `project_path` 指向当前仓库。
- `idf_version` 是 `v6.0.1`。
- `target` 是 `esp32p4`。
- build 目录存在或可生成。
- 只有在需要烧录时才选择明确串口。

## 首次构建

MCP 可用时优先用 MCP 构建动作。

CLI 回退：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
idf.py -C "E:\esp32_ai_monitor" build
```

构建成功后应生成：

- `build/esp32_ai_monitor.bin`

## 烧录和监控

确认串口后：

```powershell
idf.py -C "E:\esp32_ai_monitor" -p <PORT> flash monitor
```

板上应看到：

- LVGL 主监控屏。
- 网络状态。
- provider 状态。
- 底部或详情状态区。

如果配置未完成，可通过配置 AP 和本地网页修改运行时配置。

## 本地配置

配置入口：

- 长按上方 BOOT 按钮约 2 秒。
- 连接 fallback 配置 AP。
- 打开配置网页。

配置网页管理：

- Wi-Fi / 企业认证。
- 门户 URL 和门户账号字段。
- provider base URL、endpoint、token、management key、用户头。
- 刷新周期。

保存配置后设备会调度重启，使 Wi-Fi 和 provider 配置重新生效。

## 常用命令

重新生成配置：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
```

构建：

```powershell
idf.py -C "E:\esp32_ai_monitor" build
```

烧录并串口监控：

```powershell
idf.py -C "E:\esp32_ai_monitor" -p <PORT> flash monitor
```

## 常见问题

### 改了默认配置但行为没变

检查改动是否写入 `sdkconfig.defaults` 或 `components/app_config_service/Kconfig.projbuild`。运行时 NVS 覆盖可能仍在生效，必要时通过配置页重置或保存新值。

### 构建通过但 Wi-Fi 不正常

先确认 Hosted / Wi-Fi Remote 路线没有跑偏：

- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`

不要先按 P4 原生 Wi-Fi 方向修改业务代码。

### 首屏复位或显示异常

优先检查：

- PSRAM 是否启用。
- `CONFIG_LV_USE_CLIB_MALLOC=y`。
- `CONFIG_LV_USE_TINY_TTF=y`。
- 字体资源和 LVGL 首帧内存压力。

### 配置页能打开但保存后没有变化

检查：

- `POST /api/config` 是否返回成功。
- 配置是否通过 `app_config_validate()`。
- 保存后设备是否完成重启。
- NVS 是否仍保留旧值。

## 下一步

- 阅读 [ARCHITECTURE.md](./ARCHITECTURE.md) 理解组件边界。
- 阅读 [CONFIGURATION.md](./CONFIGURATION.md) 理解配置分层。
- 阅读 [API.md](./API.md) 查看本地 HTTP 接口。
- 阅读 [TESTING.md](./TESTING.md) 选择最小验证路径。
