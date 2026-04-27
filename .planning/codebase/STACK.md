# Codebase Stack

日期：2026-04-27

## 概览

当前仓库是一个最小化的 `ESP-IDF` C 工程，目标芯片为 `esp32p4`，尚未引入 Waveshare 官方板级 `BSP`，也尚未进入功能开发阶段。

## 当前技术栈

### 语言与构建

- 语言：`C`
- 构建系统：`CMake`
- SDK：`ESP-IDF 5.5.4`（根据根目录 `sdkconfig` 文件头）
- 目标芯片：`esp32p4`
- 入口组件：`main`

关键文件：

- 根构建入口：`CMakeLists.txt`
- 应用组件注册：`main/CMakeLists.txt`
- 应用入口：`main/main.c`
- 当前配置：`sdkconfig`
- 忽略规则：`.gitignore`

### 工程配置现状

- 当前 `main/main.c` 仅包含空的 `app_main()`。
- 当前仓库没有 `idf_component.yml`。
- 当前仓库没有 `components/` 业务模块目录。
- 当前仓库没有自定义 `partitions.csv`。
- 当前仓库没有测试目录，也没有测试框架接入。

### 本地开发辅助

- `/.devcontainer/`：存在容器化开发配置。
- `/.vscode/`：存在本地编辑器调试配置。
- `/build/`：本地已生成构建产物，用于说明此工程已经被编译过。

## 当前依赖

### 已确认依赖

- `ESP-IDF`
- `FreeRTOS`（由 `ESP-IDF` 提供）
- `esp32p4` 目标平台工具链

### 尚未接入但强烈建议后续接入的板级依赖

以下依赖来自官方 `BSP`，属于推荐后续接入方向，不代表当前仓库已使用：

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `waveshare/esp_lcd_st7703`
- `espressif/esp_lvgl_port`
- `espressif/esp_lcd_touch_gt911`
- `espressif/esp_codec_dev`

## 配置风险

当前 `sdkconfig` 暴露出几个值得尽快处理的现实问题：

- `flash size` 当前配置为 `2MB`
- 分区模式当前为 `SINGLE_APP`
- 与目标硬件的 `32MB Nor Flash` 能力不匹配

这意味着：

- 如果直接引入 `LVGL`、大字体、图片资源、日志缓冲或 `OTA`，很容易先在分区和容量上撞墙。
- 后续功能开发前，应优先确认 `flash`、分区表、`PSRAM` 使用策略。

## 建议的下一个栈演进点

1. 增加 `idf_component.yml`
2. 接入 `waveshare/esp32_p4_wifi6_touch_lcd_4b`
3. 补齐显示、触摸、`LVGL` 的板级 bring-up
4. 再进入网络监控协议和 UI 业务开发
