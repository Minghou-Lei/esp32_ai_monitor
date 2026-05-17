# ARCHITECTURE

## 产品定位

本项目面向 `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`，当前目标是构建一个板上监控与配置终端，而不是在板子上直接运行完整 AI Agent。

板子的职责优先是：

- 展示设备与远端服务状态
- 提供触摸交互和配置入口
- 提供本地网络诊断和少量控制动作

真正的 AI / 监控后端仍然应运行在外部环境。

## 当前运行时分层

当前工作树已经形成五层运行时结构：

1. 启动编排层
   - `main/main.c`
2. 配置中心层
   - `components/app_config_service`
3. 网络接入层
   - `components/network_service`
4. 外部 provider 轮询层
   - `components/provider_service`
5. 本地交互层
   - `components/config_web_service`
   - `components/ui_service`

## 启动顺序

当前 `app_main()` 依次启动：

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`

注意：

- `ui_service` 的主实现已经在 `monitor_dashboard_screen.c`
- 但公开入口名仍沿用 `wifi_info_screen_start()`
- 这属于当前命名过渡态

## 配置中心

`app_config_service` 是当前架构中心。它统一维护：

- Wi-Fi 配置
- 企业认证配置
- 门户元数据
- 配置热点参数
- provider 参数
- UI 刷新间隔

默认值来自 `sdkconfig.defaults` / `sdkconfig`，运行时覆盖则通过 `NVS` 持久化。网络层、provider 层、配置网页和 UI 都消费同一份配置模型。

## 网络层

`network_service` 当前不是简单的“连上一个热点”，而是承接这条接入路径：

- `STA` 连网
- `WPA2-PSK`
- `WPA2-Enterprise`
- 公司门户附加状态
- 配置 `SoftAP` 回退

它对外导出统一快照，供主屏和配置网页复用。

## Provider 层

`provider_service` 负责轮询外部 provider，并把结果整理成板上可消费的统一状态。当前真实实现只有 `AQI` provider，但接口边界已经通用化：

- provider 名称来自配置
- `base_url` 与 `endpoint_path` 来自配置
- token 与用户头来自配置
- 快照包含状态、请求统计、最近成功时间和增量信息

## 配置网页

`config_web_service` 当前提供：

- 板上 HTML 配置页
- 当前配置读取接口
- 当前状态读取接口
- 保存配置接口
- 门户完成接口
- 重启接口

它的定位是 bring-up 与现场维护入口，不是完整前后端应用。

## 板上主屏

`ui_service` 当前主职责是：

- 启动 `Waveshare BSP`
- 初始化 `LVGL`
- 加载字体
- 创建主监控视图
- 周期性拉取网络与 provider 快照

当前主屏更偏“监控总览 + 诊断面板”，重点字段包括：

- 网络状态
- 门户状态
- provider 状态
- 百分比或额度值
- delta 信息
- 底部详情文本

## 板级和无线硬约束

当前架构依赖这些硬约束：

- `ESP-IDF v6.0.1`
- `esp32p4`
- `32MB flash`
- `PSRAM`
- `LVGL + TinyTTF + CLIB malloc`
- `ESP-Hosted + esp_wifi_remote`

无线链路的正确理解仍然是：

- `ESP32-P4` 负责主控、UI 和业务逻辑
- 板载 `ESP32-C6` 负责无线协处理

不要把本项目按“P4 本地原生 Wi-Fi 板型”理解。

## 当前阶段判断

当前项目已经完成：

- 板级显示 bring-up
- 统一配置模型
- 网络状态机
- provider 轮询首版
- 配置网页
- 主监控屏

但尚未完成：

- 多 provider 扩展
- 更完整的控制面
- 多页面导航
- 自动化测试与 CI

因此它已经跨过早期单功能诊断原型阶段，但仍处在监控终端产品化早期。
