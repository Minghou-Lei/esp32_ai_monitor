---
last_mapped_commit: 24911b142360e77d719d9db5cfc54443770da247
mapped_at: 2026-05-17
---

# STRUCTURE

## 顶层目录

- `CMakeLists.txt`
  - 根工程入口
- `main/`
  - 应用入口组件与依赖声明
- `components/`
  - 自定义业务组件与项目内 override 组件
- `docs/`
  - 面向项目使用与设计的 canonical 文档
- `.planning/`
  - 代码映射、研究记录与历史 handoff
- `managed_components/`
  - `ESP-IDF` 组件管理器解析后的托管依赖
- `build/`
  - 当前机器生成态构建目录
- `.vscode/`
  - 工作区级 `ESP-IDF` / OpenOCD / 串口设置

## main 组件

目录结构：

- `main/CMakeLists.txt`
- `main/main.c`
- `main/idf_component.yml`

职责：

- 注册入口源文件
- 声明 `network_service` 与 `ui_service`
- 声明 Hosted / `LVGL` / `BSP` 相关托管依赖
- 指定本地 override 组件路径

当前 `main/main.c` 极薄，只负责调用：

- `wifi_info_screen_start()`
- `network_service_start()`

## 自定义业务组件

### `components/network_service`

文件：

- `components/network_service/CMakeLists.txt`
- `components/network_service/Kconfig.projbuild`
- `components/network_service/network_service.c`
- `components/network_service/include/network_service.h`

职责：

- `Wi-Fi Station` 生命周期管理
- 连接事件处理
- 网络详情快照导出
- 暴露 `menuconfig` 业务参数

当前 `Kconfig.projbuild` 已暴露：

- `CONFIG_AI_MONITOR_WIFI_SSID`
- `CONFIG_AI_MONITOR_WIFI_PASSWORD`
- `CONFIG_AI_MONITOR_WIFI_HOSTNAME`
- `CONFIG_AI_MONITOR_UI_REFRESH_MS`

### `components/ui_service`

文件：

- `components/ui_service/CMakeLists.txt`
- `components/ui_service/wifi_info_screen.c`
- `components/ui_service/include/wifi_info_screen.h`
- `components/ui_service/assets/jnr_sb_font.ttf`

职责：

- 启动板级显示
- 打开背光
- 初始化 `LVGL` 页面对象树
- 装载嵌入式 `TinyTTF` 字体资源
- 周期性拉取网络快照并渲染

当前 `CMakeLists.txt` 还通过：

- `target_add_binary_data(${COMPONENT_LIB} "assets/jnr_sb_font.ttf" BINARY RENAME_TO jnr_sb_font_ttf)`

把字体资源编进固件，这意味着字体资产已经是 UI 组件的正式组成部分，而不是临时外部文件。

## 项目内 override 组件

当前 `components/` 下除了业务组件，还存在 3 个本地 override：

- `components/espressif__esp_codec_dev`
- `components/waveshare__esp_lcd_st7703`
- `components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

它们的角色是：

- 对齐 `ESP-IDF v6.0.1`
- 修补 registry 组件与当前 SDK 的兼容面
- 保持官方组件命名与接口习惯

这类目录结构说明仓库已经进入“可维护的本地 patch 层”阶段，而不是单纯依赖托管组件原样输入。

## 配置文件层次

- `sdkconfig.defaults`
  - 可提交的持久基线
- `sdkconfig`
  - 当前机器的生效态
- `sdkconfig.old`
  - 历史配置快照
- `partitions_32mb_singleapp.csv`
  - 当前自定义分区表
- `.vscode/settings.json`
  - 当前工作区本地开发设置

这几份文件之间的边界已经比较清晰：

- 设计意图放 `sdkconfig.defaults`
- 机器态放 `sdkconfig`
- IDE 辅助放 `.vscode/settings.json`

## 文档层次

### 代码映射与研究

- `.planning/codebase/ARCHITECTURE.md`
- `.planning/codebase/STRUCTURE.md`
- `.planning/codebase/STACK.md`
- `.planning/codebase/INTEGRATIONS.md`
- `.planning/codebase/CONVENTIONS.md`
- `.planning/codebase/TESTING.md`
- `.planning/codebase/CONCERNS.md`
- `.planning/research/2026-04-27-waveshare-esp32-p4-wifi6-touch-lcd-4b.md`
- `.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md`
- `.planning/HANDOFF-2026-04-27-wifi-blocker.md`

### 项目 canonical 文档

- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/GETTING-STARTED.md`
- `docs/DEVELOPMENT.md`
- `docs/TESTING.md`
- `docs/CONFIGURATION.md`

### 现有 hand-written / proposal 文档

- `docs/ai-agent-monitor-proposal.md`

该文件不属于 map-codebase 的 7 份固定输出，但属于 docs-update 需要一起校准的现有项目文档。

## 生成态目录

### `managed_components/`

这里反映的是锁定后的第三方依赖实际落地态，当前可以看到的关键组件包括：

- `espressif__esp_hosted`
- `espressif__esp_wifi_remote`
- `espressif__esp_lvgl_port`
- `espressif__usb`
- `lvgl__lvgl`

它描述的是“当前锁下解析结果”，不是业务代码主战场。

### `build/`

当前 `build/` 已包含有效生成物，例如：

- `esp32_ai_monitor.elf`
- `esp32_ai_monitor.bin`
- `esp32_ai_monitor.map`
- `compile_commands.json`
- `project_description.json`
- `hints.yml`

从时间戳看，最近一轮成功产物落在 `2026-05-17 10:52:40`。

## 当前结构缺口

按项目定位，后续大概率还会新增这些目录或组件，但当前尚不存在：

- `components/backend_client`
- `components/agent_state`
- `components/settings_store`
- `components/touch_service`
- 后端协议测试夹具
- 自动化测试目录

所以当前仓库结构已经跨过“空工程”，但还没有进入“完整监控终端产品”阶段。
