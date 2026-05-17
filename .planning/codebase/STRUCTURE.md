---
last_mapped_commit: f4a155a1d23a3aa8ca4e7cb568217b35c1d5a510
mapped_at: 2026-05-17
---

# STRUCTURE

## 顶层目录

- `CMakeLists.txt`
  - 根工程入口
- `main/`
  - 应用入口组件与依赖注册
- `components/`
  - 业务组件与项目内 override 组件
- `docs/`
  - 面向项目使用与设计的文档
- `.planning/`
  - codebase map、研究记录与历史 handoff
- `managed_components/`
  - `ESP-IDF` 组件管理器解析出的托管依赖
- `build/`
  - 构建产物目录
- `.vscode/`
  - 工作区开发配置

## main 入口组件

文件：

- `main/CMakeLists.txt`
- `main/main.c`
- `main/idf_component.yml`

职责：

- 保持 `app_main()` 极薄
- 串联各业务服务启动
- 声明项目主入口依赖的本地组件

当前 `main/CMakeLists.txt` 直接依赖：

- `app_config_service`
- `config_web_service`
- `network_service`
- `provider_service`
- `ui_service`

这说明主入口已经围绕“配置、网络、provider、配置网页、UI”五个服务层展开。

## 自定义业务组件

### `components/app_config_service`

文件：

- `components/app_config_service/CMakeLists.txt`
- `components/app_config_service/Kconfig.projbuild`
- `components/app_config_service/app_config_service.c`
- `components/app_config_service/include/app_config_service.h`

职责：

- 定义统一运行时配置结构
- 从 Kconfig 默认值构建配置基线
- 通过 `NVS` 持久化运行时覆盖
- 对配置做长度和字段级校验

这是当前仓库的配置中心组件。

### `components/network_service`

文件：

- `components/network_service/CMakeLists.txt`
- `components/network_service/network_service.c`
- `components/network_service/include/network_service.h`

职责：

- 启动 `STA`
- 可选启用配置 `SoftAP`
- 处理企业 Wi-Fi 认证与门户状态
- 聚合网络快照供 UI 与配置网页复用

当前组件不再有独立 `Kconfig.projbuild`，而是改为消费统一配置组件。

### `components/provider_service`

文件：

- `components/provider_service/CMakeLists.txt`
- `components/provider_service/provider_service.c`
- `components/provider_service/include/provider_service.h`

职责：

- 启动远端 provider 轮询任务
- 采集外部订阅或额度数据
- 归一化为板上监控快照
- 维护增量、成功率、刷新间隔等统计

### `components/config_web_service`

文件：

- `components/config_web_service/CMakeLists.txt`
- `components/config_web_service/config_web_service.c`
- `components/config_web_service/include/config_web_service.h`

职责：

- 启动 `esp_http_server`
- 提供单文件配置页
- 提供配置读写与状态查询 REST 接口
- 负责门户完成与设备重启这类运维动作入口

### `components/ui_service`

文件：

- `components/ui_service/CMakeLists.txt`
- `components/ui_service/monitor_dashboard_screen.c`
- `components/ui_service/include/wifi_info_screen.h`
- `components/ui_service/include/monitor_dashboard_screen.h`
- `components/ui_service/assets/jnr_sb_font.ttf`

职责：

- 初始化板级显示与 `LVGL`
- 渲染板上主监控视图
- 周期性拉取网络与 provider 快照
- 管理字体和文本更新策略

注意：

- 公开入口头文件仍保留 `wifi_info_screen.h`
- 但主要实现已迁移到 `monitor_dashboard_screen.c`
- 这是当前工作树的命名过渡态

## 项目内 override 组件

当前仓库保留了多个本地 override 目录，用来兼容 `ESP-IDF v6.0.1` 与官方组件路线：

- `components/espressif__esp_codec_dev`
- `components/waveshare__esp_lcd_st7703`
- `components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

这些目录不是业务逻辑主战场，但属于构建链路的关键现实。

## 托管依赖目录

`managed_components/` 当前可见的关键组件包括：

- `espressif__esp_hosted`
- `espressif__esp_wifi_remote`
- `espressif__esp_lvgl_port`
- `espressif__usb`
- `lvgl__lvgl`
- 其他 `ESP-IDF` 组件管理器解析出的依赖

它反映的是锁文件驱动下的第三方依赖落地态。

## 配置相关文件

关键配置文件：

- `sdkconfig.defaults`
  - 可提交的持久配置基线
- `sdkconfig`
  - 当前机器生效态
- `sdkconfig.old`
  - 旧配置快照
- `partitions_32mb_singleapp.csv`
  - 当前分区表

当前配置模型已不是“只靠 `sdkconfig`”的老结构，而是：

- 构建期默认值来自 `sdkconfig.defaults`
- 运行时覆盖来自 `NVS`
- 使用期配置入口来自 `config_web_service`

## 文档层次

当前文档分为三层：

- `README.md` 与 `docs/*.md`
  - 面向项目使用、设计和开发流程
- `.planning/codebase/*.md`
  - 面向 agent / 工程分析的结构化映射
- `.planning/research/*.md`
  - 面向问题复盘和板级研究

这次映射涉及的七份固定输出仍然是：

- `ARCHITECTURE.md`
- `STRUCTURE.md`
- `STACK.md`
- `INTEGRATIONS.md`
- `CONVENTIONS.md`
- `TESTING.md`
- `CONCERNS.md`
