# E:\esp32_ai_monitor · AGENTS.md

## 0. 文档目的

这份 `AGENTS.md` 是本仓库给未来 agent 的持久执行契约。

目标不是重复仓库文档，而是把这些内容固定下来：

- 项目定位与边界
- 当前工作树事实
- `ESP-IDF` / `ESP32-P4` / `Waveshare BSP` 的硬约束
- `ESP-IDF MCP` 的优先级、使用顺序、回退规则和已知坑
- 输出、验证、配置修改、上板验收的最低要求

如果后续文档与代码发生偏差，以“当前工作树事实 + 本文件规则 + 实际验证结果”为准。

## 1. 项目定位

这是一个基于 `ESP-IDF` 的 `ESP32-P4` 项目，目标硬件是 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`。

当前推荐的产品定位不是“在板子上直接跑完整 AI Agent”，而是：

- 板子负责 `UI`、触摸交互、联网、状态展示、告警提示、简单远程控制。
- 真正的 `AI Agent` 运行在上位机、本地服务器、NAS 或云端。
- 本仓库优先做“监控终端”与“控制台”，不是做重型本地推理。

如果后续任务与这个定位冲突，先更新仓库文档里的方案和约束，再动代码。

## 2. 当前工作树事实

截至当前工作树状态，已确认这些事实：

- 这是一个真实在开发中的 `ESP-IDF` C 项目，不再是空模板。
- 根构建文件是 [CMakeLists.txt](E:/esp32_ai_monitor/CMakeLists.txt)。
- 应用入口是 [main/main.c](E:/esp32_ai_monitor/main/main.c)。
- 组件注册文件是 [main/CMakeLists.txt](E:/esp32_ai_monitor/main/CMakeLists.txt)。
- 当前 `app_main()` 会先启动 `wifi_info_screen_start()`，再启动 `network_service_start()`。
- 当前 target 是 `esp32p4`。
- 当前仓库已经生成 [dependencies.lock](E:/esp32_ai_monitor/dependencies.lock)，说明 `ESP-IDF` 组件管理器已参与依赖解析。
- 当前仓库根目录已有 [sdkconfig.defaults](E:/esp32_ai_monitor/sdkconfig.defaults)，它是可提交的持久配置基线。
- 当前仓库根目录的 [sdkconfig](E:/esp32_ai_monitor/sdkconfig) 是当前机器的生效态，可能被 `reconfigure` 或 `menuconfig` 重写。
- 当前工作区的 [settings.json](E:/esp32_ai_monitor/.vscode/settings.json) 记录的 `ESP-IDF` 根路径是 `C:\esp\v6.0.1\esp-idf`。
- 当前仓库已有代码映射和研究沉淀：
  - [.planning/codebase](E:/esp32_ai_monitor/.planning/codebase)
  - [.planning/research](E:/esp32_ai_monitor/.planning/research)

当前已存在的业务组件：

- [components/network_service](E:/esp32_ai_monitor/components/network_service)
- [components/ui_service](E:/esp32_ai_monitor/components/ui_service)

当前存在的仓库级 override / 本地板级组件目录：

- [components/espressif__esp_codec_dev](E:/esp32_ai_monitor/components/espressif__esp_codec_dev)
- [components/waveshare__esp_lcd_st7703](E:/esp32_ai_monitor/components/waveshare__esp_lcd_st7703)
- [components/waveshare__esp32_p4_wifi6_touch_lcd_4b](E:/esp32_ai_monitor/components/waveshare__esp32_p4_wifi6_touch_lcd_4b)

当前已确认的实现现状：

- 已有一个基于 `LVGL` 的 Wi-Fi 详情页面。
- 已有一个负责 Wi-Fi 状态机、连接事件与快照采集的 `network_service`。
- 当前更准确的项目阶段是：
  - `板级 bring-up + Wi-Fi 诊断页`
  - 还不是完整的 AI Agent 监控终端

注意：

- 当前基线已经不是旧文档里提到的 `2MB flash + SINGLE_APP` 最小模板配置。
- 当前 `sdkconfig.defaults` 与 `sdkconfig` 已经按 `32MB flash`、自定义分区表、`PSRAM`、`ESP-Hosted + esp_wifi_remote` 路线配置。
- 当前 `sdkconfig` 可能包含本机调试态或私有参数，不能直接视为可提交设计。

## 3. 硬件与框架结论

### 3.1 框架选择

- 首选 `ESP-IDF`。
- 不要优先选 `Arduino`。
- 不要默认选 `PlatformIO` 或 `MicroPython`。

原因：

- Waveshare 官方文档明确推荐 `ESP-IDF`。
- `ESP32-P4` 在 `Arduino` 上适配有限。
- `PlatformIO`、`MicroPython` 在该板型上都不是本项目主线。

### 3.2 板级支持方式

优先使用官方 `BSP`，不要一上来手写整套屏幕时序和触摸初始化。

推荐板级依赖：

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`

依赖来源约束：

- 微雪官方 `ESP32-P4-WIFI6-Touch-LCD-4B` 相关依赖的唯一来源基准是 <https://components.espressif.com/components?q=namespace:waveshare>。
- 不要把 GitHub 仓库、博客、第三方教程、转抄文章当成新的依赖来源基准。

当前已知 `BSP` 覆盖的关键能力：

- 显示驱动：`waveshare/esp_lcd_st7703`
- `LVGL` 适配：`espressif/esp_lvgl_port`
- 触摸驱动：`espressif/esp_lcd_touch_gt911`
- 音频能力：`espressif/esp_codec_dev`
- `SD card` 支持

结论：

- 点屏、背光、触摸输入、`LVGL` 端口，优先走 `BSP`。
- 只有在官方 `BSP` 明确不满足需求时，才考虑下探到更底层驱动。

### 3.3 板卡结构认知

这块板子的正确理解不是“一个自带无线的小屏板”，而是：

- `ESP32-P4` 负责主控、多媒体、显示和主业务。
- `ESP32-C6` 作为协处理器提供 `Wi-Fi 6 / BLE`。

因此后续必须坚持：

- 不要把无线能力误当成 `P4` 原生片上功能。
- 遇到联网链路问题时，要先确认是否涉及 `C6` 侧。
- 首版监控终端优先把 `P4` 的屏幕、UI、状态展示和联网状态跑通。

### 3.4 已确认的重要板载能力

与本项目密切相关的已确认能力：

- `4` 英寸 `720 x 720` 触摸屏
- `MIPI DSI` 显示链路
- `MIPI CSI` 摄像头接口
- 板载麦克风与扬声器接口
- `Micro SD`
- `Ethernet`
- `USB OTG 2.0 HS`
- `USB TO UART`

首版 AI 监控终端优先级：

1. 屏幕
2. 触摸
3. 联网
4. 状态拉取
5. 告警与控制

### 3.5 外设开发优先级

推荐的真实落地顺序：

1. `LCD` 点亮
2. 背光控制
3. 触摸输入
4. `LVGL` 基础界面
5. `Wi-Fi` 或 `Ethernet` 联网
6. `HTTP polling`
7. 日志 / 告警 / 控制按钮
8. 音频、摄像头、`SD card` 等扩展能力

不要倒着做。

### 3.6 烧录与调试

- 烧录和串口调试优先使用板上的 `USB TO UART` 接口。
- 不要把 `USB OTG` 默认当成烧录口。
- 如需进入下载模式，按官方说明配合 `BOOT` / `RESET`。

如果任务涉及 `ESP32-C6` 协处理器：

- 先确认是否真的需要改 `C6` 固件。
- 默认假设板载 `C6` 固件可用，不要无故重刷。
- 只有在明确要处理 `Wi-Fi 6 / BLE` 协处理链路问题时，才进入 `C6` 烧录路径。

## 4. 当前代码实现认知

### 4.1 启动流

当前 `main/main.c` 的启动顺序是：

1. `wifi_info_screen_start()`
2. `network_service_start()`

这意味着当前页面先启动显示，再通过定时刷新读取网络快照。

### 4.2 `network_service`

[components/network_service](E:/esp32_ai_monitor/components/network_service) 当前负责：

- 初始化 `NVS`
- 初始化默认事件循环
- 创建默认 STA `netif`
- 设置主机名
- 启动 `Wi-Fi Station`
- 处理 `WIFI_EVENT_STA_START`、`WIFI_EVENT_STA_CONNECTED`、`WIFI_EVENT_STA_DISCONNECTED`、`IP_EVENT_STA_GOT_IP`
- 聚合 `SSID`、`IP`、`DNS`、`MAC`、`RSSI`、信道等快照字段

它不直接创建 UI，只通过头文件接口把快照暴露给界面层。

### 4.3 `ui_service`

[components/ui_service](E:/esp32_ai_monitor/components/ui_service) 当前负责：

- 调用官方 `BSP` 启动显示
- 打开背光
- 加载内嵌 `TinyTTF` 字体资源
- 创建 `LVGL` 页面对象树
- 周期性刷新 Wi-Fi 详情页
- 渲染这些信息：
  - 状态与状态文本
  - `SSID`
  - 主机名
  - `STA MAC` / `BSSID`
  - `IPv4` / `Netmask` / `Gateway`
  - 主 / 备 `DNS`
  - 认证方式与加密方式
  - `RSSI` 与主信道

### 4.4 当前阶段判断

当前仓库已经跨过“空工程规划”阶段，但还没有进入完整产品阶段。

已落地：

- 显示 bring-up
- Wi-Fi 状态采集
- 诊断型界面

未落地：

- 后端 `HTTP polling`
- Agent 心跳模型
- 监控总览页
- 告警 / 控制动作
- 日志流

## 5. 代码与配置组织规则

### 5.1 入口文件

- 保持 [main/main.c](E:/esp32_ai_monitor/main/main.c) 很薄。
- `app_main()` 只做启动编排，不承载具体业务逻辑。

### 5.2 模块拆分

新增功能时，优先拆成 `components/` 下的独立模块，推荐方向：

- `board_support`
- `display_service`
- `touch_service`
- `ui`
- `network_service`
- `backend_client`
- `agent_state`
- `settings_store`

禁止把显示、网络、协议解析、页面逻辑全部塞进一个 `main.c`。

### 5.3 UI 规则

- `UI` 统一走 `LVGL`。
- 先做可维护的页面状态管理，再做花哨动画。
- 页面应区分“初始化中、在线、离线、错误、空数据”几类状态。
- 监控型界面优先保证可读性，不要做消费级炫技布局。

### 5.4 网络协议规则

- `V1` 优先 `HTTP polling`。
- `WebSocket` 作为后续优化。
- `MQTT` 只有在后端已明确采用时再接入。

理由：

- `HTTP polling` 最容易抓包、复现和调试。
- 在嵌入式端先把数据模型跑稳定，比过早引入长连接更重要。

### 5.5 配置分层

当前项目配置分层应始终按这个模型理解：

- `sdkconfig.defaults`
  - 可提交、可复用的持久配置基线
- `sdkconfig`
  - 当前机器的生效态
- `partitions_32mb_singleapp.csv`
  - 当前自定义分区表
- `.vscode/settings.json`
  - 当前工作区的本地开发设置

### 5.6 `sdkconfig.defaults` 是持久基线

凡是涉及这些内容，都应优先沉淀到 `sdkconfig.defaults`：

- flash size
- 分区表
- `PSRAM`
- `ESP-Hosted`
- `Wi-Fi Remote`
- `LWIP`
- 显示相关能力开关

### 5.7 不把本机敏感项写回默认基线

当前 `sdkconfig` 已经包含本机 Wi-Fi 凭据。后续必须坚持：

- 不把本机 `SSID` / 密码复制进 `sdkconfig.defaults`
- 文档里不展开这些具体值
- 评审 `sdkconfig` diff 时优先检查是否混入私有配置

## 6. 当前已验证的关键配置事实

当前 `sdkconfig.defaults` 已经表达这些关键意图：

- target：`esp32p4`
- flash size：`32MB`
- 自定义分区表：`partitions_32mb_singleapp.csv`
- 开启 `PSRAM`
- Hosted Wi-Fi 路线
- `LVGL TinyTTF`

关键方向包括：

- `CONFIG_IDF_TARGET="esp32p4"`
- `CONFIG_ESPTOOLPY_FLASHSIZE="32MB"`
- `CONFIG_PARTITION_TABLE_CUSTOM=y`
- `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_32mb_singleapp.csv"`
- `CONFIG_SPIRAM=y`
- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

当前 `sdkconfig` 已确认的关键事实：

- target 为 `esp32p4`
- flash size 为 `32MB`
- 分区表指向 `partitions_32mb_singleapp.csv`
- 已启用 Hosted / Wi-Fi Remote
- 当前 `LVGL` allocator 已切到 `CLIB malloc`
- 存在本机 Wi-Fi 凭据和主机名配置

## 7. ESP-Hosted / Wi-Fi Remote 约束

当前项目不是“本地 `esp_wifi` 直驱板载无线”的假设，而是：

- `ESP32-P4 host`
- `ESP32-C6` 协处理无线
- `ESP-Hosted + esp_wifi_remote`

因此后续必须坚持：

- 不打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- Hosted 板级配置优先 `P4 Function EV Board + SDIO`
- 优先从 Hosted / Remote 路线排查联网问题
- 看到 `net80211` 风格异常时先检查配置是否跑偏

如果运行日志出现这些异常，优先怀疑 Hosted 路线配置跑偏，而不是先改业务代码：

- `OS adapter function version error`
- `Failed to unregister Rx callbacks`
- `esp_wifi_init failed`
- 本地 `net80211` 风格错误栈

完整复盘参考：

- [2026-04-27-esp32-p4-wifi-bringup-pitfalls.md](E:/esp32_ai_monitor/.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md)

## 8. `LVGL` / `PSRAM` / `TinyTTF` 运行时约束

当前 UI 运行路径有这些已验证结论：

- `PSRAM` 是显示路径前置条件。
- `TinyTTF` 会放大首帧内存压力。
- builtin `LVGL` allocator 的小池配置不足时，可能在首帧触发 `SW_CPU_RESET`。
- 当前更稳的组合是：
  - `CONFIG_LV_USE_CLIB_MALLOC=y`
  - `CONFIG_LV_USE_TINY_TTF=y`

因此后续必须坚持：

- 不要默认 builtin `LVGL` `64KB` 池足够承载首帧字体渲染。
- 出现首帧 `SW_CPU_RESET` 时，优先先看 `LVGL allocator / TinyTTF / PSRAM` 路径，而不是先怀疑 Wi-Fi。
- 任何清理 `sdkconfig`、切换 target、恢复 builtin allocator、扩字体资源的动作，都必须把这条链路当成高优先级风险重新评估。

## 9. ESP-IDF MCP / Agent 规则

本仓库显式采用 `ESP-IDF v6.0+` 的 MCP / Agent 优先规则。

### 9.1 Critical Rule

当任务涉及这些 `ESP-IDF` 工程动作时：

- `set target`
- `build`
- `flash`
- `clean`
- `status`
- `devices`
- `config`

优先级必须是：

1. 优先使用 `idf.py mcp-server` 暴露的 MCP 能力。
2. 不要一上来直接跑裸 `idf.py`。
3. 只有在 MCP 不可用、未挂载、启动失败、超时不足以完成或当前环境不支持时，才回退到普通命令行方式。
4. 回退时必须明确写出原因，不允许静默切回 CLI。

### 9.2 标准确认顺序

执行任何 `ESP-IDF` 工程动作前，先确认：

1. 当前目录是否为项目根目录
2. `ESP-IDF` 版本
3. 当前 target
4. 构建目录
5. MCP 是否实际可用

如果需要烧录，再额外确认：

1. 项目已经成功 build
2. 已检查串口设备
3. 目标串口明确
4. 不存在多个无法区分的候选设备

### 9.3 MCP 优先读取顺序

如果 MCP 可用，优先按这个顺序做：

1. `project://config`
2. `project://status`
3. 如需烧录，再看 `project://devices`
4. 再调用 `set target / build / flash / clean`
5. 最后再次读取状态或执行等价验证

### 9.4 MCP 不可用时的回退规则

如果 MCP 不可用，仍要保持同样的依赖检查与验证流程。

必须在输出里说明：

- 为什么不能走 MCP
- 改走了什么 CLI 路径
- 回退后的验证结果是什么

### 9.5 当前全局 MCP 配置事实

当前仓库默认依赖“用户级 Codex 全局 MCP 配置”，不是项目级 MCP 配置。

当前全局 `esp-idf` MCP server 已注册在：

- `C:\Users\admin\.codex\config.toml`

当前全局 `esp-idf` MCP 启动脚本位于：

- `C:\Users\admin\.codex\scripts\start-esp-idf-mcp.ps1`

当前启动方式是：

- `pwsh.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\admin\.codex\scripts\start-esp-idf-mcp.ps1`

### 9.6 当前启动脚本的职责

[start-esp-idf-mcp.ps1](C:/Users/admin/.codex/scripts/start-esp-idf-mcp.ps1) 当前负责：

- 设定 PowerShell 严格错误策略
- 统一 `UTF-8` I/O 编码
- 设置：
  - `IDF_PATH`
  - `PYTHONUTF8`
  - `PYTHONIOENCODING`
- 显式调用：
  - `C:\Users\admin\.espressif\python_env\idf6.0_py3.14_env\Scripts\python.exe`
  - `C:\esp\v6.0.1\esp-idf\tools\activate.py --export`
- dot-source 激活脚本
- 吞掉激活脚本写到 `stdout` 的提示输出，避免破坏 MCP `stdio` 协议
- 最终执行：
  - `idf.py -C E:\esp32_ai_monitor mcp-server`

### 9.7 当前已验证的 MCP 事实

当前已验证这些事实：

- 当前会话能看到 `esp-idf` MCP 资源：
  - `project://config`
  - `project://status`
  - `project://devices`
- 当前会话已能通过 MCP 读取到：
  - `project_path = E:\esp32_ai_monitor`
  - `idf_version = v6.0.1`
  - `target = esp32p4`
- 当前会话已能通过 MCP 识别串口：
  - `COM1`
  - `COM7`

### 9.8 MCP 使用注意事项

当前 `ESP-IDF MCP` 真实使用中需要注意：

- 资源读取可用，不等于所有长耗时工具调用都能在当前默认超时窗口内完成。
- `build_project` 可能因为工具层 `120s` 超时而失败，即使底层编译其实能完成。
- 出现这种情况时，应把它判断为“工具超时限制”还是“构建失败”，不要直接下结论说 MCP 不可用。
- 如果 `build_project` 超时，应补做：
  - 读取 `project://status`
  - 检查 `build` 目录最新产物
  - 必要时用 CLI 复核 `idf.py build` 尾部输出

### 9.9 当前已知 CLI 稳定进入方式

当前机器上更稳定的 CLI 进入方式是：

1. 显式调用 `python activate.py --export`
2. 在当前 `PowerShell` 会话 dot-source 返回的临时脚本
3. 再执行 `idf.py`

比起直接依赖 `export.ps1`，这种路径更稳。

## 10. 当前本地环境现实约束

虽然工作区已记录 `ESP-IDF v6.0.1` 根路径，但是否能直接使用仍取决于当前会话是否真的接入了本机环境。

已观察到：

- 当前工作区的 [settings.json](E:/esp32_ai_monitor/.vscode/settings.json) 指向 `C:\esp\v6.0.1\esp-idf`
- 当前本机 `ESP-IDF` Python venv 已存在：
  - `C:\Users\admin\.espressif\python_env\idf6.0_py3.14_env\Scripts\python.exe`
- 当前本机已补齐 `ESP-IDF` 的 MCP Python 依赖：
  - `C:\esp\v6.0.1\esp-idf\tools\requirements\requirements.mcp.txt`
- 当前 `idf.py --version` 可返回 `ESP-IDF v6.0.1`
- 当前机器上的 `reconfigure / build / flash / 串口启动验证` 已做过实际验证

这意味着：

- 不要继续沿用“本机 `ESP-IDF` 环境不可用”的旧判断。
- 不要把“全局已配置”误写成“当前会话一定已拿到 `ESP-IDF MCP` 资源”；仍应先检查。
- 不要把 `.vscode/settings.json` 的路径误写成“会话环境一定已经自动激活”。

## 11. 推荐命令与 Windows 规则

在本仓库里，优先使用这些 `ESP-IDF` 标准命令：

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
```

```powershell
idf.py -C "E:\esp32_ai_monitor" build
```

```powershell
idf.py -C "E:\esp32_ai_monitor" -p COM7 flash monitor
```

如需核对或声明板级依赖，再使用：

```powershell
idf.py -C "E:\esp32_ai_monitor" add-dependency "waveshare/esp32_p4_wifi6_touch_lcd_4b^1.0.1"
```

注意：

- 改了组件依赖、分区、`sdkconfig` 或 `menuconfig` 后，优先 `reconfigure` 再 `build`
- 改了显示链路后，必须上板验证，不接受只看编译结果

### 11.1 Windows 下获取 `idf.py` 环境

- 不要默认当前 `PowerShell` 已经能直接识别 `idf.py`
- 优先从 [settings.json](E:/esp32_ai_monitor/.vscode/settings.json) 的 `idf.currentSetup` 读取 `ESP-IDF` 根路径
- 当前已知路径是 `C:\esp\v6.0.1\esp-idf`
- 当前 Codex 全局 `ESP-IDF MCP` 启动入口是 [start-esp-idf-mcp.ps1](C:/Users/admin/.codex/scripts/start-esp-idf-mcp.ps1)

但请注意：

- 直接调用 `C:\esp\v6.0.1\esp-idf\export.ps1` 仍可能因为 shell 上下文差异失败
- 当前更稳的做法是显式调用 `python activate.py --export` 后再 dot-source，或直接通过已配置好的 Codex MCP server 使用 `idf.py mcp-server`

### 11.2 `SDK Configuration Editor` / `menuconfig` 持久化流程

- `VSCode` 的 `SDK Configuration Editor` 或 `menuconfig` 主要修改当前生效态 [sdkconfig](E:/esp32_ai_monitor/sdkconfig)
- 如果希望配置对仓库长期生效，必须把应提交的基线同步回 [sdkconfig.defaults](E:/esp32_ai_monitor/sdkconfig.defaults)
- 改动涉及 `ESP-Hosted`、`Wi-Fi Remote`、分区、`flash size`、`PSRAM`、显示参数或 `LWIP` 缓冲区时，都按这个流程处理
- `idf.py save-defconfig` 可以回写默认配置，但必须先审查结果，避免把本机私有项一并导出

## 12. 修改配置后的推荐动作

修改配置后，默认动作顺序是：

1. 审查 `sdkconfig.defaults`
2. 运行 `reconfigure`
3. 再运行 `build`
4. 如涉及显示或网络关键路径，必须上板验证

如果构建或上板结果与预期不一致，不要只修文档；要先把事实重新核对再更新规则。

## 13. 运行时风险与已知坑

当前高优先级运行时风险包括：

- `GPIO 26` 的 `LEDC` 背光冲突
- `ESP-Hosted` 主从版本不匹配提示
- `LVGL + TinyTTF + allocator` 组合导致的首帧断言复位
- `sdkconfig` 混入本机敏感项

当前仓库没有这些安全垫：

- 单元测试
- 组件测试
- CI
- 模拟后端夹具

因此：

- “文档正确”不代表“固件行为正确”
- 最终仍要靠 `build` 与上板回归闭环

## 14. 修改前检查清单

开始写代码前，先回答这些问题：

1. 这次改动属于 `LCD / Touch / UI / Network / Backend Protocol / Audio / Camera / Storage` 哪一层？
2. 这次改动是否可复用官方 `BSP` 或现有组件？
3. 当前 `sdkconfig.defaults`、分区表、`flash`、`PSRAM` 配置是否足够？
4. 这次改动能否拆成独立组件，而不是继续堆进 `main.c`？
5. 这次改动的“上板验收动作”是什么？
6. 如果这次改动涉及 `menuconfig`，哪些项只在 `sdkconfig` 生效，哪些项必须回写到 `sdkconfig.defaults`？
7. 这次 `ESP-IDF` 工程动作是否应优先走 MCP？如果不能走，回退原因是什么？
8. 这次改动是否可能重新触发：
  - Hosted Wi-Fi 配置跑偏
  - `PSRAM` 丢失
  - `LVGL/TinyTTF` 首帧问题

如果这些问题答不清，不要急着写实现。

## 15. 输出与验证契约

未来 agent 在这个仓库里工作时，默认执行这些约束：

- 如果任务意图清楚、下一步可逆且低风险，直接推进，不要停在分析。
- 不要把“计划”当成完成，除非用户明确只要计划。
- 对 `ESP-IDF` 工程动作，优先 MCP；回退 CLI 时必须写原因。
- 对构建、配置、分区、显示、联网类改动，必须给出验证结果。
- 如果不能验证，明确说明为什么不能验证、下一步最小验证是什么。
- 不要把旧文档当成真理；要优先以当前工作树、当前配置、当前资源读取、当前构建和当前上板结果为准。

## 16. 参考资料

- Waveshare 板卡主页：
  - <https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-4B/>
- Waveshare `ESP-IDF` 开发页面：
  - <https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-4B/Development-Environment-Setup-IDF/>
- Waveshare 相关资料页：
  - <https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-4B/Resources-And-Documents/>
- 主板原理图：
  - <https://www.waveshare.net/w/upload/1/19/ESP32-P4-WIFI6-Touch-LCD-4B.pdf>
- 子板原理图：
  - <https://www.waveshare.net/w/upload/9/95/86_Panel_Bottom_Board.pdf>
- `ESP32-P4` 数据手册（中文）：
  - <https://documentation.espressif.com/esp32-p4_datasheet_cn.pdf>
- `ESP32-P4` 技术参考手册（中文）：
  - <https://documentation.espressif.com/esp32-p4_technical_reference_manual_cn.pdf>
- Espressif 组件注册表 `waveshare` 命名空间搜索页：
  - <https://components.espressif.com/components?q=namespace:waveshare>

项目内高价值资料：

- [README.md](E:/esp32_ai_monitor/README.md)
- [docs/CONFIGURATION.md](E:/esp32_ai_monitor/docs/CONFIGURATION.md)
- [docs/ARCHITECTURE.md](E:/esp32_ai_monitor/docs/ARCHITECTURE.md)
- [docs/DEVELOPMENT.md](E:/esp32_ai_monitor/docs/DEVELOPMENT.md)
- [docs/TESTING.md](E:/esp32_ai_monitor/docs/TESTING.md)
- [.planning/codebase/CONVENTIONS.md](E:/esp32_ai_monitor/.planning/codebase/CONVENTIONS.md)
- [.planning/codebase/CONCERNS.md](E:/esp32_ai_monitor/.planning/codebase/CONCERNS.md)
- [.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md](E:/esp32_ai_monitor/.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md)

## 17. 对未来 agent 的一句话要求

在这个仓库里，先把 `ESP32-P4-WIFI6-Touch-LCD-4B` 当成“带触摸屏的网络监控终端”来开发；优先用 `ESP-IDF v6.0+` 的 MCP / Agent 能力操作工程，MCP 不可用时再明确回退到 `idf.py`；先吃透官方 `BSP`、当前工作树事实、当前全局 MCP 配置与当前已验证配置，再做自己的架构，不要一上来就从底层裸写整板驱动。
