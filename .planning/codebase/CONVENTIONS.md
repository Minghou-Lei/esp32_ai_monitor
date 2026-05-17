---
last_mapped_commit: 24911b142360e77d719d9db5cfc54443770da247
mapped_at: 2026-05-17
---

# CONVENTIONS

## 入口与模块边界

### `app_main()` 保持轻量

当前 `main/main.c` 只做两件事：

- 启动 `wifi_info_screen_start()`
- 启动 `network_service_start()`

这条约定已经落地，不应回退成把显示、网络、协议解析再塞回 `main`。

### 业务能力优先拆进 `components/`

当前正向示例已经存在：

- `components/network_service`
- `components/ui_service`

后续如果增加监控协议、状态存储或控制能力，优先继续沿组件边界扩展，而不是把逻辑压回单文件。

## 配置管理约定

### `sdkconfig.defaults` 是持久基线

当前仓库已经把设计基线与机器态分开：

- `sdkconfig.defaults`
  - 应提交、可复现
- `sdkconfig`
  - 当前机器生效态

涉及这些内容时，应优先落到 `sdkconfig.defaults`：

- target
- flash size
- 分区表
- `PSRAM`
- Hosted / Wi-Fi Remote
- `LWIP`
- `LVGL` allocator
- 显示相关能力开关

### 不把本机私有项沉淀到默认基线

当前 `sdkconfig` 可能带本机 `Wi-Fi` 凭据和主机名，因此应继续坚持：

- 不把 `SSID` / 密码复制进 `sdkconfig.defaults`
- 文档不展开本机敏感值
- 评审 `sdkconfig` 时先检查是否混入私有配置

## 依赖来源与 override 约定

### 优先官方组件形状

板级依赖的来源基准仍然是 `components.espressif.com` 下的 `waveshare` 命名空间，而不是第三方博客或随手拷贝的仓库。

### 允许最小 override，但不鼓励长期分叉

当前仓库已经存在本地 override 组件，这是当前工作树事实。对应约定应明确为：

- 可以为了 `ESP-IDF v6.0.1` 兼容性做最小 patch
- patch 目标是“保住官方组件路线”
- 不要把 override 演变成长期自维护的大 fork

## Wi-Fi 路线约定

当前项目不是“P4 本地直驱无线”的假设，而是：

- `ESP32-P4 host`
- `ESP32-C6` 无线协处理器
- `ESP-Hosted + esp_wifi_remote`

因此后续应坚持：

- 不打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 联网异常先检查 Hosted / Remote 配置与依赖组合
- 看到 `net80211` 风格异常时先怀疑路线跑偏

## UI 与内存约定

### 统一走 `LVGL`

当前 UI 全部基于 `LVGL`，并依赖 `Waveshare BSP`。后续不要再引入第二套本地 UI 体系。

### `PSRAM + TinyTTF + allocator` 要一起看

当前仓库已把：

- `CONFIG_SPIRAM=y`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

作为更稳的组合写进默认基线。

因此约定应明确：

- 不要默认 `LVGL` builtin `64KB` 池足够承载首帧字形缓存
- 看到首帧 `SW_CPU_RESET` 或 `stb_truetype` 相关断言时，先查字体和堆路径
- 改字体、改 allocator、改 `PSRAM`、改显示缓冲，都要重新评估这条链路

### UI 刷新只更新变化文本

当前 `wifi_info_screen.c` 已显式加入“仅在文本变化时才 `lv_label_set_text()`”的做法，这不是偶然优化，而是当前页面的稳定性约定之一。

原因：

- 页面使用多个 `TinyTTF` 大字号对象
- 无差别反复更新会放大字形缓存与布局开销
- 可能导致 `taskLVGL` 长时间占用 CPU 并触发 watchdog

## 工程操作约定

### `ESP-IDF` 工程动作优先 MCP

仓库 `AGENTS.md` 已把这条写成显式规则，当前会话事实也支持它：

- `project://config` 可读
- `project://status` 本次读取超时

因此实际执行约定应是：

- 先用 MCP
- 区分“超时”与“失败”
- MCP 不可用或不支持时明确回退 CLI
- 回退原因必须写清楚

### 构建 / 配置改动后要回归

涉及以下内容时，不能只看代码 diff：

- 组件依赖
- `sdkconfig.defaults`
- 分区表
- 显示链路
- Hosted Wi-Fi 配置

最小回归路径应是：

1. `reconfigure`
2. `build`
3. 需要时 `flash monitor`

## 文档约定

### `.planning/codebase` 描述当前工作树事实

当前仓库长期处于边做边验证状态，因此 `.planning/codebase` 不应假装自己只描述“最后一次已提交版本”。

当前更合适的约定是：

- `last_mapped_commit` 记录提交基线
- 正文明确说明是否纳入当前未提交实现
- 发现文档与工作树不一致时优先刷新映射，而不是套用旧结论

### canonical 文档留在 `README.md` 与 `docs/`

当前项目文档层次已经形成：

- `README.md`
- `docs/*.md`
- `.planning/codebase/*.md`
- `.planning/research/*.md`

前两者面向项目使用与设计；后两者更偏 agent / 工程内部参考。
