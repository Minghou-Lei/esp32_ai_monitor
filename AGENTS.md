# E:\esp32_ai_monitor · AGENTS.md

## 1. 项目定位

这是一个基于 `ESP-IDF` 的 `ESP32-P4` 项目，目标硬件是 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`。

当前更合理的产品定位不是“在板子上直接跑完整 AI Agent”，而是：

- 板子负责 `UI`、触摸交互、联网、状态展示、告警提示、简单远程控制。
- 真正的 `AI Agent` 运行在上位机、本地服务器、NAS 或云端。
- 本仓库优先做“监控终端”与“控制台”，不是做重型本地推理。

如果后续任务与这个定位冲突，先在仓库内文档里更新方案，再动代码。

## 2. 当前仓库事实

截至当前状态：

- 工程是最小可编译的 `ESP-IDF` C 项目。
- 根构建文件是 [CMakeLists.txt](E:/esp32_ai_monitor/CMakeLists.txt)。
- 应用入口是 [main/main.c](E:/esp32_ai_monitor/main/main.c)。
- 组件注册文件是 [main/CMakeLists.txt](E:/esp32_ai_monitor/main/CMakeLists.txt)。
- 当前 `app_main()` 为空，业务代码还未开始。
- 当前 `sdkconfig` 目标芯片是 `esp32p4`。
- 当前仓库已经存在依赖清单文件 [main/idf_component.yml](E:/esp32_ai_monitor/main/idf_component.yml)，并声明了 `waveshare/esp32_p4_wifi6_touch_lcd_4b`。
- 当前仓库已经生成 `dependencies.lock`，且本地存在 `managed_components/`，说明板级 `BSP` 依赖已通过 `ESP-IDF` 组件管理器解析到工程内。
- 当前仓库已有一份方向性方案文档：[docs/ai-agent-monitor-proposal.md](E:/esp32_ai_monitor/docs/ai-agent-monitor-proposal.md)。
- 当前仓库根目录已有 [sdkconfig.defaults](E:/esp32_ai_monitor/sdkconfig.defaults)，它应被视为可提交的 `ESP-IDF` 持久配置基线。
- 当前仓库根目录的 [sdkconfig](E:/esp32_ai_monitor/sdkconfig) 是当前生效态，不应被当成唯一持久来源；它可以被 `idf.py reconfigure` 或 `menuconfig` 重写。
- 当前工作区的 [settings.json](E:/esp32_ai_monitor/.vscode/settings.json) 已记录 `ESP-IDF` 安装根：`idf.currentSetup = C:\esp\v5.5.4\esp-idf`。

注意：

- 当前 `sdkconfig` 里是 `SINGLE_APP` 分区，且 `flash size` 配置仍是 `2MB`。
- 这与板载 `32MB Nor Flash` 的硬件条件不匹配。
- 在引入 `LVGL`、图片资源、字体、日志缓存、网络功能或 `OTA` 之前，必须先复核分区表与 `flash` 配置，不能直接沿用当前最小工程配置。

## 3. 硬件开发结论

基于 Waveshare 官方文档与 `ESP Component Registry`，这块板子的推荐开发方式如下：

### 3.1 框架选择

- 首选 `ESP-IDF`。
- 不要优先选 `Arduino`。
- 不要默认选 `PlatformIO` 或 `MicroPython`。

原因：

- Waveshare 官方文档明确推荐 `ESP-IDF`。
- `ESP32-P4` 在 `Arduino` 上适配有限。
- 相关家族 FAQ 对 `PlatformIO`、`MicroPython` 的支持都带明显限制，不适合作为本项目主线。

### 3.2 板级支持方式

优先使用官方 `BSP`，不要一开始手写整套屏幕时序和触摸初始化。

推荐依赖：

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`

依赖来源约束：

- 在本仓库里，微雪官方 `ESP32-P4-WIFI6-Touch-LCD-4B` 相关依赖的唯一来源是 <https://components.espressif.com/components?q=namespace:waveshare>。
- 不要把 GitHub 仓库、第三方教程、博客摘录或单个组件详情页当成新的依赖来源基准。

官方组件公开信息表明，这个 `BSP` 已经封装了以下能力：

- 显示驱动：`waveshare/esp_lcd_st7703`
- `LVGL` 适配：`espressif/esp_lvgl_port`
- 触摸驱动：`espressif/esp_lcd_touch_gt911`
- 音频能力：`espressif/esp_codec_dev`
- `SD card` 支持

结论：

- 点屏、背光、触摸输入、`LVGL` 端口，优先走 `BSP`。
- 只有在官方 `BSP` 无法满足需求时，才考虑下探到更底层驱动。

### 3.2.1 板卡软件结构认知

这块板子的正确理解不是“一个自带无线的小屏板”，而是：

- `ESP32-P4` 负责主控、多媒体、显示和主业务
- `ESP32-C6` 作为协处理器提供 `Wi-Fi 6 / BLE`

这会影响后续架构判断：

- 不要把无线能力误当成 `P4` 原生片上功能
- 遇到联网链路问题时，要先确认是否涉及 `C6` 侧
- AI 监控终端的首版应优先把 `P4` 的屏幕、UI 和状态展示能力跑通

### 3.2.2 已确认的重要板载能力

已确认且与本项目密切相关的能力：

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

### 3.3 外设开发优先级

本项目推荐的真实落地顺序：

1. `LCD` 点亮
2. 背光控制
3. 触摸输入
4. `LVGL` 基础界面
5. `Wi-Fi` 或 `Ethernet` 联网
6. `HTTP` 状态拉取
7. 日志 / 告警 / 控制按钮
8. 音频、摄像头、`SD card` 等扩展能力

不要倒着做。

在监控终端产品里，先证明“屏能亮、UI 能跑、网络能通、状态能刷”，再做音视频和复杂交互。

### 3.4 烧录与调试

- 烧录和串口调试优先使用板上的 `USB TO UART` 接口。
- 不要把 `USB OTG` 默认当成烧录口。
- 如需进入下载模式，按官方说明配合 `BOOT` / `RESET`。

如果任务涉及 `ESP32-C6` 协处理器：

- 先确认是否真的需要改 `C6` 固件。
- 默认假设板载 `C6` 固件可用，不要无故重刷。
- 只有在明确要处理 `Wi-Fi 6` / `BLE` 协处理链路问题时，才进入 `C6` 烧录路径。

### 3.5 官方资料入口

后续任何 agent 研究这块板子时，优先查这些资料，而不是凭印象改代码：

#### 板卡资料

- 板卡主页
- `ESP-IDF` 开发页面
- 资料页
- 主板原理图 PDF
- 子板原理图 PDF

#### 芯片资料

- `ESP32-P4` 数据手册
- `ESP32-P4` 技术参考手册

#### 项目内沉淀

- `AGENTS.md`
- `.planning/research/2026-04-27-waveshare-esp32-p4-wifi6-touch-lcd-4b.md`
- `.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md`
- `.planning/codebase/*.md`

### 3.6 官方示例的使用顺序

根据 Waveshare 官方开发页面，本项目应按下面顺序借用示例：

1. `MIPI-DSI` 点屏示例
2. `LVGL HMI` 示例
3. `Camera LCD Display` 示例
4. `MP4 Player` / `ESP-Phone` 作为扩展参考

原因：

- 这符合“先 bring-up，再 UI，再多媒体扩展”的节奏
- 对 AI 监控终端来说，最早阶段不需要摄像头和完整多媒体壳工程

## 4. 本项目的推荐实现路线

未来 agent 在这个仓库里工作时，默认按下面路线推进：

### 阶段 A：板级 Bring-up

目标：

- 接入 `BSP`
- 点亮屏幕
- 打开背光
- 跑通 `LVGL`
- 建一个最小仪表盘页面

验收：

- 屏幕稳定刷新
- 触摸有基础输入事件
- 不是只编译通过，而是有实际显示结果

### 阶段 B：监控数据链路

目标：

- 先接 `Wi-Fi`
- 定义一个最小监控 `JSON` 协议
- 从模拟服务或真实后端拉取状态
- 页面显示在线状态、延迟、任务名、最后心跳时间

验收：

- 能看到实时或准实时状态变化
- 网络断开时有明确 UI 状态

### 阶段 C：控制与告警

目标：

- 增加刷新、重连、确认告警等控制动作
- 增加错误信息和事件日志显示

验收：

- 每个按钮都能映射到明确动作
- 失败态有 UI 反馈，不允许静默失败

### 阶段 D：扩展能力

按需求启用：

- 麦克风
- 扬声器
- 摄像头
- `SD card`
- `Ethernet`

默认不要在早期阶段把这些能力一次性全部接入。

## 5. 代码组织规则

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

## 6. 开发约束

### 6.1 不要做的事

- 不要绕过官方 `BSP` 盲写 `MIPI DSI` 时序。
- 不要在没有必要时自定义触摸驱动。
- 不要把“能编译”当成“能上板”。
- 不要默认当前 `sdkconfig` 可以承载未来功能。
- 不要默认板子适合本地跑大型 `LLM`。
- 不要无文档地大改分区表、`flash` 设置、时钟、内存策略。

### 6.2 先查文档，再改代码

以下场景先查官方资料：

- 屏幕点不亮
- 触摸坐标异常
- 背光不工作
- `LVGL` 刷新异常
- `C6` 联网异常
- 摄像头或音频链路异常
- GPIO、屏幕、触摸、音频引脚不确定
- 分区、容量、外设分工判断不确定

优先级：

1. Waveshare 该板卡文档
2. 原理图 PDF
3. `ESP32-P4` 数据手册 / 技术参考手册
4. Waveshare `BSP` / 示例
5. Espressif 组件文档
6. 再考虑自己下探驱动

## 7. 推荐命令

在本仓库里，优先使用 `ESP-IDF` 标准命令：

```bash
idf.py set-target esp32p4
idf.py fullclean
idf.py reconfigure
idf.py build
idf.py -p <PORT> flash monitor
```

如果需要重新声明、升级或核对板级组件依赖，先以唯一依赖来源确认包名与版本，再使用：

```bash
idf.py add-dependency "waveshare/esp32_p4_wifi6_touch_lcd_4b^1.0.1"
```

注意：

- 改了组件依赖、分区、`sdkconfig` 或 `menuconfig` 后，优先 `reconfigure` 再 `build`。
- 改了显示链路后，必须上板验证，不接受只看编译结果。

### 7.1 Windows 下获取 `idf.py` 环境

- 不要默认当前 `PowerShell` 已经能直接识别 `idf.py`。
- 在本仓库对应的当前工作区里，优先从 [settings.json](E:/esp32_ai_monitor/.vscode/settings.json) 的 `idf.currentSetup` 读取 `ESP-IDF` 根路径。
- 当前机器上的已知路径是 `C:\esp\v5.5.4\esp-idf`，后续如路径变化，以 `.vscode/settings.json` 的值为准。
- 如果终端里 `idf.py` 不可用，先执行该路径下的 `export.ps1`，再运行 `idf.py`。

```powershell
& 'C:\esp\v5.5.4\esp-idf\export.ps1' *> $null
idf.py reconfigure
```

- 也可以串成单行：

```powershell
& 'C:\esp\v5.5.4\esp-idf\export.ps1' *> $null; idf.py build
```

### 7.2 `SDK Configuration Editor` 的正确持久化流程

- `VSCode` 的 `SDK Configuration Editor` 或 `menuconfig` 主要修改的是当前生效态 [sdkconfig](E:/esp32_ai_monitor/sdkconfig)。
- 如果希望配置对仓库长期生效，必须把应提交的基线同步回 [sdkconfig.defaults](E:/esp32_ai_monitor/sdkconfig.defaults)，不能只停留在 `sdkconfig`。
- 正确顺序是：先在 `SDK Configuration Editor` 中验证配置组合，再把需要持久化的选项整理到 `sdkconfig.defaults`，最后运行 `idf.py reconfigure` 让生成态重新吃一遍默认值。
- 只要改动涉及 `ESP-Hosted`、`Wi-Fi Remote`、分区、`flash size`、`PSRAM`、显示参数或 `LWIP` 缓冲区，都按上述流程处理。
- `idf.py save-defconfig` 可以把当前 `sdkconfig` 回写为默认配置，但必须先审查结果；它可能把本地 `SSID`、密码或其他只适合本机的选项一并写入基线。
- 如无明确理由，不要把本机私有凭据提交进 `sdkconfig.defaults`。

### 7.3 已踩过的 `Wi-Fi bring-up` 坑

- 对这块板，`Wi-Fi` 不是 `ESP32-P4` 本地 `esp_wifi`，而是 `ESP32-P4 host + ESP32-C6 slave` 的 `ESP-Hosted + esp_wifi_remote` 路线。
- 不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED=y`。本地 `esp_wifi_remote` 组件把它和 `Wi-Fi Remote` 视为互斥；一旦打开，就可能重新掉回本地 `net80211` 路线。
- `ESP-Hosted` 的板级配置要优先选 `P4 Function EV Board + SDIO`，不要误配成 `No development board + SPI`。
- 如果运行日志出现 `OS adapter function version error`、`Failed to unregister Rx callbacks`、`esp_wifi_init failed`，优先怀疑走偏到本地 Wi-Fi 路线，而不是先改业务代码。
- 如果日志卡在 `HS_MP: mempool create failed: no mem` 或 `sdio_mempool_create` 断言，优先怀疑内部 DMA 内存不足；当前项目的已知解法是关闭 `ESP_HOSTED_USE_MEMPOOL`，并把 Hosted 默认任务栈迁到 `SPIRAM`。
- 如果 UI 首帧阶段出现 `_svfprintf_r` / `snprintf` 的 `Stack protection fault`，优先减少首帧格式化压力，不要在启动阶段做巨型单次字符串拼接。
- 完整复盘见 [2026-04-27-esp32-p4-wifi-bringup-pitfalls.md](E:/esp32_ai_monitor/.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md)。

## 8. 修改前检查清单

开始写代码前，先回答下面几个问题：

1. 这次改动属于 `LCD / Touch / UI / Network / Backend Protocol / Audio / Camera / Storage` 哪一层？
2. 这次改动是否可以复用官方 `BSP` 或示例？
3. 当前 `sdkconfig`、分区表、`flash` 配置是否足够？
4. 这次改动能否拆成独立组件，而不是继续堆进 `main.c`？
5. 这次改动的“上板验收动作”是什么？
6. 如果这次改动涉及 `SDK Configuration Editor` 或 `menuconfig`，哪些项只在 `sdkconfig` 生效，哪些项必须回写到 `sdkconfig.defaults`？

如果这些问题答不清，不要急着写实现。

## 9. 参考资料

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
- Espressif 组件注册表的 `waveshare` 命名空间搜索页（本仓库唯一依赖来源）：
  - <https://components.espressif.com/components?q=namespace:waveshare>

## 10. 对未来 agent 的一句话要求

在这个仓库里，先把 `ESP32-P4-WIFI6-Touch-LCD-4B` 当成“带触摸屏的网络监控终端”来开发；先从唯一依赖来源确认 `waveshare` 组件，再吃透官方 `BSP` 和示例，最后做自己的架构，不要一上来就从底层裸写整板驱动。
