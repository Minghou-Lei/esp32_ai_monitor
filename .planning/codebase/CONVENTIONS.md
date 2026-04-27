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
- 对 `ESP32-P4-WIFI6-Touch-LCD-4B`，板载 `Wi-Fi` 默认按 `ESP-Hosted + esp_wifi_remote` 处理，不要把它当成本地 `esp_wifi` 直连板
- 排查 `Wi-Fi` bring-up 时，先看是否误开了 `CONFIG_ESP_HOST_WIFI_ENABLED`，再看 transport / `C6` 路线

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

- 已提交的 `ESP-IDF` 持久配置基线放在仓库根的 `sdkconfig.defaults`
- 仓库根的 `sdkconfig` 只代表当前生效态，不应被当成唯一长期来源
- 使用 `SDK Configuration Editor` 或 `menuconfig` 后，如果配置要长期保留，必须把相关项同步回 `sdkconfig.defaults`
- 将 `sdkconfig.defaults` 更新到位后，运行 `idf.py reconfigure`，让当前 `sdkconfig` 与构建目录重新吃一遍默认值
- 在 Windows 环境中，如果终端无法直接识别 `idf.py`，优先从 `.vscode/settings.json` 的 `idf.currentSetup` 获取 `ESP-IDF` 根目录，再执行对应 `export.ps1`
- 修改 `sdkconfig` 前先明确影响范围
- 修改 `flash`、分区表、显示参数、`PSRAM` 策略时，必须补文档
- 修改 `ESP-Hosted`、`Wi-Fi Remote`、`LWIP` 缓冲区等板级联网参数时，也必须补文档
- 板级参数优先来源于官方 `BSP`、原理图和官方手册，不要凭印象写死
- `idf.py save-defconfig` 只在确认输出内容安全后再使用；它可能把本机 `SSID`、密码或其他本地调试项一并导出
- 对本项目当前已知的 Hosted 路线，`# CONFIG_ESP_HOST_WIFI_ENABLED is not set` 是重要约束；看到本地 `net80211` 风格日志时，优先检查这里
- 如果 `ESP-Hosted` 在 `SDIO` bring-up 阶段报 `mempool create failed: no mem`，优先从内部 DMA 内存压力方向排查，而不是先回退功能
- 这次 bring-up 的系统性坑点与修复路径已沉淀在 `.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md`
