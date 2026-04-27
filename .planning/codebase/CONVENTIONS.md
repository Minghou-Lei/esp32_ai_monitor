# Conventions

日期：2026-04-27

## 已观察到的现状约定

当前代码量非常少，可直接观察到的约定有限：

- 使用 `ESP-IDF` 标准 `app_main()` 作为入口
- 使用 `idf_component_register()` 注册组件
- 当前代码语言为 `C`
- 根构建使用 `CMake`

## 应继续坚持的约定

### 入口约定

- `main/main.c` 保持轻量
- `app_main()` 只做启动编排，不做复杂业务处理

### 模块约定

- 新能力优先拆到 `components/` 下
- 不要把显示、触摸、网络、协议解析堆到同一个源文件里

### 依赖约定

- 优先复用官方 `BSP`
- 优先使用 `ESP-IDF` 组件管理器
- 新增三方依赖时，优先写入 `idf_component.yml`
- 微雪官方 `ESP32-P4-WIFI6-Touch-LCD-4B` 依赖的来源只认 <https://components.espressif.com/components?q=namespace:waveshare>
- 如需升级或新增 `waveshare` 命名空间组件，先查上述搜索页，再更新 `idf_component.yml` 与 `dependencies.lock`

### UI 约定

- `UI` 统一走 `LVGL`
- 页面必须显式区分“初始化中、在线、离线、错误、空数据”状态
- 监控类界面优先保证信息密度与可读性

### 网络约定

- `V1` 优先 `HTTP polling`
- `WebSocket` 在数据模型稳定后再引入
- `MQTT` 只有在上位系统已采用时才接入

### 文档约定

- 中文文档中，中英文与数字混排保持可读性
- 当前仓库的板卡结论优先沉淀到 `AGENTS.md` 与 `.planning/`
- 推荐把“现状”与“建议”分开写，避免误导后续 agent

## 错误处理约定

当前代码尚未建立统一错误处理模式，建议后续明确：

- 初始化阶段的失败日志
- 网络失败时的重试与 UI 提示
- 触摸、显示、后端请求等链路错误不要静默吞掉

## 配置约定

- 修改 `sdkconfig` 前先明确影响范围
- 修改 `flash`、分区表、显示参数、`PSRAM` 策略时，必须补文档
- 板级参数优先来源于官方 `BSP`、原理图和官方手册，不要凭印象写死
