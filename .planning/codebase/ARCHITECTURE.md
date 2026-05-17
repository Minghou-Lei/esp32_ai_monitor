---
last_mapped_commit: 24911b142360e77d719d9db5cfc54443770da247
mapped_at: 2026-05-17
---

# ARCHITECTURE

## 当前运行时结构

当前固件仍处于“板级 bring-up + Wi-Fi 诊断页”阶段，但运行时拓扑已经稳定成三层：

- `main/main.c`
  - 只负责启动编排
- `components/network_service`
  - 负责 `Wi-Fi Station` 生命周期与状态快照
- `components/ui_service`
  - 负责 `BSP + LVGL` 显示初始化与详情页渲染

这套结构已经符合“薄入口 + 独立组件”的仓库约束，没有把显示、网络和页面逻辑重新塞回 `app_main()`。

注意：

- 本文映射基于当前工作树，而不是只基于 `HEAD`
- `last_mapped_commit` 记录的是最近一次提交 `24911b142360e77d719d9db5cfc54443770da247`
- 当前工作树还包含未提交实现，尤其是 `components/ui_service/wifi_info_screen.c`、`main/idf_component.yml`、`dependencies.lock` 和文档本身

## 启动编排

入口文件是 `main/main.c`，当前顺序固定为：

1. `wifi_info_screen_start()`
2. `network_service_start()`

这意味着显示链路先起来，页面会先进入等待数据状态；随后 `network_service` 开始驱动 `Wi-Fi` 状态机，界面通过定时刷新读取最新快照。

这个顺序和当前产品目标一致：

- 先保证板载显示可见
- 再让网络状态逐步收敛到 UI
- 避免把网络初始化失败误判成“屏幕没起来”

## 网络服务层

核心实现位于 `components/network_service/network_service.c`，对外头文件是
`components/network_service/include/network_service.h`。

当前职责包括：

- 初始化 `NVS`
- 初始化默认事件循环
- 创建默认 `STA` `netif`
- 设置主机名
- 启动 `Wi-Fi Station`
- 处理 `WIFI_EVENT_STA_START`
- 处理 `WIFI_EVENT_STA_CONNECTED`
- 处理 `WIFI_EVENT_STA_DISCONNECTED`
- 处理 `IP_EVENT_STA_GOT_IP`
- 聚合 `SSID`、`BSSID`、`IPv4`、`DNS`、`MAC`、`RSSI`、信道、认证方式和加密方式

对外暴露的关键接口只有两个：

- `network_service_start()`
- `network_service_get_snapshot()`

其中 `network_service_get_snapshot()` 会在持锁状态下补齐运行时字段，因此当前 UI 可以安全地周期性拉取整份快照，而不需要关心底层事件和同步细节。

## UI 与显示层

核心实现位于 `components/ui_service/wifi_info_screen.c`，当前并不是简单文本页，而是一套建立在 `Waveshare BSP` 之上的 `LVGL` 监控面板。

当前实现的关键路径：

- 通过 `bsp_display_start_with_config()` 启动显示
- 打开背光
- 把 `LVGL` 绘图缓冲放进 `PSRAM`
- 通过 `target_add_binary_data()` 把 `assets/jnr_sb_font.ttf` 打包进固件
- 在运行时通过 `TinyTTF` 构建多组字体对象
- 创建主状态板、辅助状态板和详情区
- 通过定时刷新读取 `network_service` 快照并更新屏幕

当前未提交 UI 重构已经把页面从“单块详情文本”推进到了更强的站牌式信息结构：

- 主状态 badge
- 状态副标题
- 主展示值
- 辅助展示值
- 数字面板
- 细节文本区

同时加入了两个很重要的运行时约束：

- 使用 `heap_caps_*` 打点内部 SRAM / `SPIRAM`
- 仅在文本变化时调用 `lv_label_set_text()`，避免 `TinyTTF` 大字号对象在定时器里反复重排导致 `taskLVGL` 持续占用 CPU 并触发 watchdog

## 板级与组件依赖层

项目已经不再完全依赖 registry 原样组件，而是使用“registry 版本声明 + 仓库内 override_path”模式：

- `espressif/esp_codec_dev`
  - `override_path: ../components/espressif__esp_codec_dev`
- `waveshare/esp_lcd_st7703`
  - `override_path: ../components/waveshare__esp_lcd_st7703`
- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
  - `override_path: ../components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

这些 override 的角色不是自建整套板级抽象，而是：

- 追随官方组件形状
- 处理 `ESP-IDF v6.0.1` 下的兼容点
- 保住当前 `ESP32-P4 + Waveshare BSP + LVGL` 路线可编译、可启动

当前 `main/idf_component.yml` 还显式声明了：

- `espressif/usb`
- `espressif/esp_wifi_remote`
- `espressif/esp_hosted`

说明当前架构已经把“主控 P4 + 无线协处理 C6”的 hosted 路线当成正式依赖面，而不是临时实验。

## 配置与内存架构

当前配置层的关键设计不在 `sdkconfig`，而在 `sdkconfig.defaults`。

已确认的持久意图包括：

- `CONFIG_IDF_TARGET="esp32p4"`
- `CONFIG_ESPTOOLPY_FLASHSIZE="32MB"`
- `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_32mb_singleapp.csv"`
- `CONFIG_SPIRAM=y`
- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

这里最关键的架构结论是：

- `PSRAM` 不只是性能优化，而是显示路径前置条件
- `LVGL + TinyTTF` 不能再依赖默认小内存池假设
- Hosted Wi-Fi 路线和本地 `esp_wifi` 路线必须明确二选一，当前项目选择的是前者

## 工具链与工程操作架构

仓库级 `AGENTS.md` 已把 `ESP-IDF MCP` 设为优先操作路径，当前会话里也确实能读到：

- `project://config`
- `project://devices`

但当前会话对 `project://status` 的读取在 120 秒窗口内超时，因此当前准确结论不是“MCP 不可用”，而是：

- 资源读取部分可用
- 长耗时或较重资源读取需要把“工具超时”与“工程失败”分开判断

这会直接影响后续 agent 的行为：

- 先走 MCP
- MCP 超时或不支持时明确回退 `idf.py`
- 回退时写明原因和验证结果

## 当前缺口

按产品目标，当前架构还缺这些业务层：

- `backend_client`
- `agent_state`
- `settings_store`
- 监控总览页
- 告警 / 控制动作
- 后端状态拉取协议

因此当前架构更接近：

- 已完成板级显示与网络诊断支架
- 尚未进入完整 AI 监控终端业务层
