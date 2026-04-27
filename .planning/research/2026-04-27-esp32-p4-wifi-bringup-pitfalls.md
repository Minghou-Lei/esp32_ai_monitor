# ESP32-P4 Wi-Fi Bring-up Pitfalls

日期：2026-04-27

## 背景

本次 bring-up 的目标不是让 `ESP32-P4` 直接跑本地 `Wi-Fi` 驱动，而是让
`ESP32-P4-WIFI6-Touch-LCD-4B` 按板卡真实硬件结构工作：

- `ESP32-P4` 负责主控、显示、触摸、UI 与业务
- 板载 `ESP32-C6` 负责 `Wi-Fi 6 / BLE`
- 主工程通过 `ESP-Hosted + esp_wifi_remote` 使用无线能力

因此，任何把它当作“普通 `esp_wifi` 直连板”的改法，都很容易走偏。

## 已踩过的坑

### 1. 把板子当成本地 `esp_wifi` 目标

表现：

- `network_service_start()` 按普通 `ESP-IDF Wi-Fi STA` 路线初始化
- 运行时出现：
  - `wifi_init: Failed to unregister Rx callbacks`
  - `wifi_init: Failed to deinit Wi-Fi driver (0x3001)`
  - `network_service: esp_wifi_init failed`

根因：

- 虽然依赖里已经接入 `esp_hosted` 与 `esp_wifi_remote`，但业务初始化仍按
  “本地 Wi-Fi 驱动”理解来排查问题，容易把注意力放错地方

结论：

- 看到 `ESP32-P4-WIFI6-Touch-LCD-4B` 的联网问题时，先假设这是
  `P4 host + C6 slave` 的 hosted/remote 路线，而不是本地 `esp_wifi`

### 2. `SDK Configuration Editor` 改对了当前态，但没沉淀到持久基线

表现：

- 编辑器里看起来已经改成目标配置
- 但重新 `reconfigure`、切终端或后续 agent 接手后，配置又漂移

根因：

- 当前生效态是仓库根的 `sdkconfig`
- 可提交、可复现的持久基线是仓库根的 `sdkconfig.defaults`
- 只改 `sdkconfig`，不改 `sdkconfig.defaults`，后续就可能被覆盖

结论：

- 正确顺序必须是：
  1. 在 `SDK Configuration Editor` / `menuconfig` 里验证配置组合
  2. 将需要长期保留的项同步到 `sdkconfig.defaults`
  3. 运行 `idf.py reconfigure`
  4. 再 `idf.py build`

### 3. 板载 `C6` 选成了错误的开发板 / 总线组合

表现：

- `ESP-Hosted` 已启用，但实际 transport 路径异常
- 早期曾出现“把板载 `C6` 当成 `SPI host` 自定义板”的错配

根因：

- 这块板的板载 `P4 <-> C6` 通信链路应走 `SDIO`
- 对应 `ESP-Hosted` 配置应选 `ESP32-P4-Function-EV-Board with on-board C6`
  一类的板级预设，而不是 `No development board + SPI`

结论：

- 这块板的已知正确方向是：
  - `ESP_HOSTED_CP_TARGET_ESP32C6=y`
  - `ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y`
  - `ESP_HOSTED_SDIO_HOST_INTERFACE=y`

### 4. `CONFIG_ESP_HOST_WIFI_ENABLED=y` 会把 `Wi-Fi Remote` 路线关掉

表现：

- 看起来已经启用了 `esp_wifi_remote`
- 但运行日志仍出现本地 `net80211` 风格报错：
  - `net80211: OS adapter function version error! Version 8 is expected, but it is 0`

根因：

- 本地组件 `esp_wifi_remote/Kconfig` 明确规定：
  - `ESP_HOST_WIFI_ENABLED` 与 `ESP_WIFI_REMOTE_ENABLED` 互斥
- 一旦打开 `CONFIG_ESP_HOST_WIFI_ENABLED=y`
  - 工程就会回到本地 host Wi-Fi 路线
  - 无法按预期走 `esp_wifi_remote` 的 hosted glue

结论：

- 对本项目这块板：
  - `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
  - `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
  - `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`

### 5. 首帧 UI 文本格式化过重，`main` 任务栈会炸

表现：

- `Wi-Fi` 失败后，`main` 任务没有立刻正常返回
- 随后出现：
  - `Guru Meditation Error: Core 0 panic'ed (Stack protection fault)`
  - 栈回溯落在 `_svfprintf_r` / `snprintf`

根因：

- `wifi_info_screen_refresh()` 里一次超长 `snprintf` 拼接二十多个字段
- 在首帧刷新与启动阶段时序重叠时，`main` 任务栈余量不足

修复：

- 将超长 `snprintf` 拆成逐段追加
- 把 `CONFIG_ESP_MAIN_TASK_STACK_SIZE` / `CONFIG_MAIN_TASK_STACK_SIZE`
  提升到 `6144`

结论：

- 这块板的启动阶段不要在首帧 UI 刷新里做巨型单次格式化

### 6. `ESP-Hosted` 的 SDIO mempool 可能一次性抢不到 DMA 内存

表现：

- `Wi-Fi Remote` 路线已经开始工作
- 但启动早期卡在：
  - `H_API: ESP-Hosted starting`
  - `HS_MP: mempool create failed: no mem`
  - `assert failed: sdio_mempool_create sdio_drv.c:249 (buf_mp_g)`

根因：

- `ESP-Hosted` 的 `sdio_mempool_create()` 会启动时一次性申请 DMA-capable
  的内部内存池
- `P4` 工程在显示、LVGL、BSP、Hosted 线程都起来后，内部 DMA 内存比较紧

修复：

- 关闭 `CONFIG_ESP_HOSTED_USE_MEMPOOL`
- 打开 `CONFIG_ESP_HOSTED_DFLT_TASK_FROM_SPIRAM=y`
  把 Hosted 默认任务栈迁到 `SPIRAM`，给内部 DMA 堆腾空间

结论：

- 如果日志卡在 `mempool create failed: no mem`
  优先怀疑内部 DMA 内存，而不是先怀疑 `C6` 固件

### 7. Windows 终端里不一定能直接用 `idf.py`

表现：

- 在普通 `PowerShell` 里直接执行 `idf.py` 提示找不到命令

根因：

- 当前 shell 会话没有加载 `ESP-IDF` 导出环境
- 工作区的真实 `ESP-IDF` 路径记录在 `.vscode/settings.json` 的
  `idf.currentSetup`

结论：

- 当前工程的已知路径是：
  - `C:\esp\v5.5.4\esp-idf`
- 终端里先执行：

```powershell
& 'C:\esp\v5.5.4\esp-idf\export.ps1' *> $null
idf.py reconfigure
```

## 当前已验证的关键配置

以下组合在本仓库当前状态下已验证能编译，并能让 Wi-Fi 进入 Hosted 路线：

```ini
# CONFIG_ESP_HOST_WIFI_ENABLED is not set
CONFIG_ESP_WIFI_REMOTE_ENABLED=y
CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y

CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y
CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y
CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y

CONFIG_ESP_MAIN_TASK_STACK_SIZE=6144
CONFIG_MAIN_TASK_STACK_SIZE=6144

CONFIG_ESP_HOSTED_DFLT_TASK_FROM_SPIRAM=y
# CONFIG_ESP_HOSTED_USE_MEMPOOL is not set
```

## 排查时优先看的日志

### 说明走偏到本地 Wi-Fi 的日志

- `net80211: OS adapter function version error!`
- `wifi_init: Failed to unregister Rx callbacks`
- `wifi_init: Failed to deinit Wi-Fi driver (0x3001)`

### 说明已经进入 Hosted/Remote 路线的日志

- `host_init: ESP Hosted : Host chip_ip[...]`
- `H_API: ESP-Hosted starting`
- `H_API: esp_wifi_remote_init`

### 说明 transport 真正打通的日志

- `Type: SDIO`
- `transport: Received INIT event from ESP32 peripheral`
- `rpc_wrap: Received Slave ESP Init`

## 后续 agent 的最短检查顺序

1. 先确认 `sdkconfig.defaults` 是否仍保持 Hosted/Remote 正确组合
2. 再确认 `sdkconfig` 是否被旧的显式配置覆盖
3. 用 `export.ps1 + idf.py reconfigure + idf.py build` 重建当前生效态
4. 上板先看是否出现本地 `net80211` 日志
5. 若无，再看是否卡在 `ESP-Hosted` 的 SDIO/mempool 阶段
6. 若 transport 已起来但仍无法联网，再考虑板载 `C6` slave 固件版本

## 一句话结论

这块板的 Wi-Fi bring-up 不是“把 `esp_wifi` 配起来”这么简单，而是要同时守住
`Hosted/Remote` 路线、`sdkconfig.defaults` 持久化、启动栈、内部 DMA 内存和
板载 `C6` 角色认知这五个约束。
