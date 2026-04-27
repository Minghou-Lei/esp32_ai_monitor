# Waveshare ESP32-P4-WIFI6-Touch-LCD-4B 平台研究

日期：2026-04-27

## 研究目标

为 `E:\esp32_ai_monitor` 项目沉淀这块板子的硬件与开发路径认知，避免后续实现阶段重复踩坑。

## 资料来源

### 板卡资料

- Waveshare 板卡主页
- Waveshare `ESP-IDF` 开发页面
- Waveshare 资料页
- 主板原理图 PDF
- 子板原理图 PDF

### 芯片资料

- `ESP32-P4` 数据手册
- `ESP32-P4` 技术参考手册

### 软件资料

- Espressif Component Registry 的 `waveshare` 命名空间搜索页：<https://components.espressif.com/components?q=namespace:waveshare>
- 本项目后续凡涉及微雪官方 `ESP32-P4-WIFI6-Touch-LCD-4B` 依赖确认，只认上面的搜索页，不再把 GitHub 仓库或单个组件详情页当成新的来源基准

## 硬件结论

### 主控结构

这不是“单芯片自带无线”的常见 `ESP32-S3` 思路，而是：

- `ESP32-P4` 负责主控、多媒体、显示、UI、外设编排
- `ESP32-C6` 作为协处理器，提供 `Wi-Fi 6 / BLE`

这对软件架构的直接影响是：

- 联网链路要按 `P4 + C6` 的组合来理解
- 不要把无线能力误当成 `P4` 原生片上射频

### 板上关键资源

已确认的重点能力：

- `4` 英寸 `720 x 720` 触摸屏
- `MIPI DSI` 显示链路
- `MIPI CSI` 摄像头接口
- 麦克风与扬声器链路
- `Micro SD`
- `USB OTG 2.0 HS`
- `USB TO UART`
- `Ethernet`
- 可充电 `RTC` 电池接口

对 AI 监控终端最关键的不是“全都能用”，而是：

- 屏幕
- 触摸
- 联网
- 稳定刷新

## 软件开发结论

### 首选开发框架

官方推荐使用 `ESP-IDF`。

原因：

- `ESP32-P4` 在 `Arduino` 上适配有限
- 官方示例和板级支持主要围绕 `ESP-IDF`

### 首选板级支持方式

优先使用官方 `BSP`：

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- 该依赖的来源固定为 `namespace:waveshare` 搜索页，不再单独维护第二套来源口径

官方组件资料显示，这个 `BSP` 已封装：

- 显示驱动
- `LVGL` 端口
- `GT911` 触摸
- 音频编解码能力
- `SD card` 支持

结论：

- 首版开发不应从裸驱动写起
- 优先复用 `BSP` 与官方示例路径

## 官方示例的价值排序

基于 Waveshare 开发页面，和本项目目标最相关的示例优先级如下：

### 第一优先级

- `MIPI-DSI` 点屏示例
- `LVGL HMI` 示例

用途：

- 用于验证屏幕、背光、刷新、`LVGL` 端口链路

### 第二优先级

- `Camera LCD Display`

用途：

- 只有当项目需要摄像头接入时再展开

### 第三优先级

- `MP4 Player`
- `ESP-Phone`

用途：

- 用于参考更完整的多媒体 UI 架构
- 不应作为本项目的第一步

## 结合当前仓库的判断

当前仓库现状：

- 只有空的 `app_main()`
- 还未接入 `BSP`
- 还未进入显示 bring-up

因此最合理的下一阶段不是直接写 AI 协议界面，而是：

1. 接入 `BSP`
2. 调整 `flash` 与分区策略
3. 点亮屏幕
4. 跑通触摸
5. 跑通最小 `LVGL` 页面
6. 再接入网络与状态协议

## 对 AI Agent 监控产品的落地建议

推荐把这块板子定义成：

- AI Agent 的监控与控制终端

而不是：

- 板上本地跑重型 `LLM` 的执行节点

更适合首版承载的能力：

- 在线状态
- 心跳
- 当前任务
- 日志与告警
- 简单远程控制

## 当前最重要的三个风险

1. 当前 `sdkconfig` 的 `flash size` 与板卡能力不匹配
2. 后续如果继续把代码堆进 `main/main.c`，维护性会快速恶化
3. 如果绕过官方 `BSP` 直接下探显示和触摸驱动，bring-up 成本会显著上升

## 建议下一步

### 文档侧

- 继续执行 `GSD new-project`，把项目目标、需求和路线正式化

### 工程侧

- 保持 `main/idf_component.yml` 与 `dependencies.lock` 受控
- 后续如需升级或新增微雪官方组件，只能先从 `namespace:waveshare` 搜索页确认
- 建立 `components/` 级模块骨架

### 验证侧

- 以“点屏 + 触摸 + `LVGL` 页面”作为第一阶段真实验收目标
