# Architecture

日期：2026-04-27

## 当前架构结论

当前仓库还谈不上完整架构，只有一个最小启动骨架。

现状结构：

- 根项目通过 `CMakeLists.txt` 进入 `ESP-IDF`
- `main/CMakeLists.txt` 注册唯一组件
- `main/main.c` 提供空的 `app_main()`

这意味着当前架构只有一层：

- 启动入口层

还没有出现以下架构层：

- 板级支持层
- 显示服务层
- 输入服务层
- 网络服务层
- 后端协议层
- UI 状态层
- 存储与配置层

## 当前数据流

当前没有真实业务数据流。

仅有的流程是：

1. `ESP-IDF` 启动
2. 调用 `app_main()`
3. 函数立即结束

## 当前抽象水平

当前抽象几乎为零，属于“空仓起步”状态。

优点：

- 还没有历史包袱
- 可以按板卡能力和目标产品定位重新设计模块边界

风险：

- 如果下一步直接在 `main/main.c` 里堆代码，会很快把显示、触摸、网络、协议逻辑混成一团

## 建议的目标架构

面向这块板子，推荐后续逐步演进为以下结构：

### 1. Board Support

职责：

- 初始化 Waveshare `BSP`
- 提供背光、屏幕、触摸、音频等板级能力入口

建议位置：

- `components/board_support/`

### 2. Display / UI

职责：

- 屏幕初始化
- `LVGL` 端口与渲染循环
- 页面切换与组件绘制

建议位置：

- `components/display_service/`
- `components/ui/`

### 3. Input

职责：

- 触摸事件采集
- 手势或点击到 UI 事件映射

建议位置：

- `components/touch_service/`

### 4. Network

职责：

- `Wi-Fi` / `Ethernet` 建链
- 重连策略
- 在线状态维护

建议位置：

- `components/network_service/`

### 5. Backend Client

职责：

- 状态拉取
- 控制命令发送
- `JSON` 解析与错误处理

建议位置：

- `components/backend_client/`

### 6. State Model

职责：

- 缓存最新监控状态
- 为 UI 提供只读视图

建议位置：

- `components/agent_state/`

## 与硬件形态强相关的架构认知

这块板子的关键点不是“单片机 + 小屏”，而是“`ESP32-P4` 多媒体主控 + `ESP32-C6` 无线协处理 + `MIPI DSI` 触摸屏”。

因此架构上要避免两个误区：

- 把它当普通串口屏来写
- 把它当能本地跑重型 `LLM` 的边缘算力盒子来写

更合理的理解是：

- 它是一个强 HMI 能力的联网终端
- 适合做 AI Agent 监控台、控制面板、事件屏、轻交互前端
