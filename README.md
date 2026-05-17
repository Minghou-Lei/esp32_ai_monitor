# esp32_ai_monitor

基于 `ESP-IDF` 的 `ESP32-P4` 监控终端项目，目标硬件是 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`。

当前项目定位不是“在板子上直接运行完整 AI Agent”，而是做一个带触摸屏的本地监控与配置终端：

- 板子负责 UI、状态展示、联网、配置入口和少量控制动作
- 真正的 AI / 监控后端运行在 PC、本地服务器、NAS 或云端
- 固件优先保证 bring-up、状态可观测和运维入口可用

## 当前工作树状态

当前本地实现已经包含：

- `ESP-IDF v6.0.1` + `esp32p4` 工程骨架
- `waveshare` 官方 `BSP` 路线
- `ESP-Hosted + esp_wifi_remote` 无线链路
- `app_config_service`
  - 统一管理 Wi-Fi、门户、配置热点、provider 和 UI 刷新配置
- `network_service`
  - 管理 `STA`、企业认证、门户状态和 `SoftAP` 回退
- `provider_service`
  - 轮询外部 provider，并输出统一状态快照
- `config_web_service`
  - 提供板上配置页与本地 REST 接口
- `ui_service`
  - 渲染基于 `LVGL` 的主监控屏

这意味着当前项目已经不再是早期单功能诊断原型，而是“监控终端骨架 + 本地配置门户”的阶段。

## 已实现能力

- 启动板级显示与背光
- 加载内嵌 `TinyTTF` 字体
- 展示网络、门户和 provider 状态
- 支持运行期读取与保存配置
- 支持本地配置网页
- 支持 provider 轮询与额度 / delta 类状态展示

## 当前未完成能力

- 多 provider 扩展
- 更完整的控制动作
- 多页面导航与更完整的监控总览
- 自动化测试与 CI
- 更成熟的后端状态模型

## 目录结构

- `main/`
  - 应用入口与组件依赖声明
- `components/app_config_service/`
  - 统一运行时配置模型与 NVS 持久化
- `components/network_service/`
  - Wi-Fi 接入、门户状态、配置热点回退
- `components/provider_service/`
  - 外部 provider 轮询与状态归一化
- `components/config_web_service/`
  - 板上配置网页与 REST 接口
- `components/ui_service/`
  - `BSP + LVGL` 主监控屏
- `docs/`
  - 项目说明文档
- `.planning/`
  - 代码映射、研究记录和工程分析资料

## 构建前置

至少确认这些条件：

- 已安装 `ESP-IDF v6.0.1`
- 当前 target 为 `esp32p4`
- 目标板为 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`
- 本地能解析仓库声明的 `managed_components` 与项目级 override 组件

## 构建与烧录

按仓库约定，`ESP-IDF` 工程动作优先使用 MCP。手动在 PowerShell 中操作时，可在仓库根目录执行：

```powershell
idf.py reconfigure
```

```powershell
idf.py build
```

```powershell
idf.py -p <PORT> flash monitor
```

优先使用板上的 `USB TO UART` 口进行烧录和串口观察，不要默认使用 `USB OTG`。

## 关键配置方向

`sdkconfig.defaults` 当前固定了这些关键意图：

- `32MB flash`
- 自定义分区表
- `PSRAM`
- Hosted / Wi-Fi Remote
- `LVGL TinyTTF`
- `LVGL CLIB malloc`

这些都属于当前设计基线，不应随意回退。

## 敏感信息约定

仓库当前已经存在多类运行时敏感配置：

- Wi-Fi 密码
- EAP 凭据
- 门户凭据
- provider token
- provider 用户头值

因此：

- 不把本机实际值写进文档
- 不把本机敏感项沉淀到 `sdkconfig.defaults`
- 评审 `sdkconfig` 时优先检查是否混入私有配置

## 开发注意事项

- 不要把这块板当成“P4 本地原生 Wi-Fi 板型”
- 不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 改显示、字体、allocator 或 `PSRAM` 配置时，重新评估首帧内存峰值
- 改组件依赖、分区或默认配置后，至少重新 `reconfigure` 和 `build`
- 涉及显示或网络关键路径的改动，最终仍要上板验证

## 参考文档

- [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)
- [docs/CONFIGURATION.md](./docs/CONFIGURATION.md)
- [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md)
- [docs/GETTING-STARTED.md](./docs/GETTING-STARTED.md)
- [docs/TESTING.md](./docs/TESTING.md)
