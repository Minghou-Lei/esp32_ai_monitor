# GETTING-STARTED

## 目标硬件

当前目标板卡是：

- `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`

建议把它理解为“带触摸屏的网络监控终端”，而不是本地重推理设备。

## 前置条件

开始前至少确认：

- 已安装 `ESP-IDF v6.0.1`
- 当前 target 为 `esp32p4`
- 本地能解析仓库依赖
- 板卡可通过 `USB TO UART` 进行烧录和串口观察

## 初次构建

在仓库根目录执行：

```powershell
idf.py reconfigure
```

```powershell
idf.py build
```

如果构建成功，关键产物会出现在 `build/` 目录下。

## 烧录

选择正确的串口后执行：

```powershell
idf.py -p <PORT> flash monitor
```

其中 `<PORT>` 替换为当前机器检测到的实际串口。

## 首次上板验收

第一次跑起来时，优先确认：

1. 屏幕点亮
2. 背光正常
3. 主视图能进入初始状态
4. 首帧没有因 `TinyTTF` / allocator / `PSRAM` 路径复位
5. 网络状态能进入连接流程
6. 配置网页可访问
7. provider 状态能进入 idle / fetching / ready 这类合理状态

## 配置入口

当前工程支持运行时配置，不必把所有参数都写死在构建期。

主要入口包括：

- `sdkconfig.defaults`
  - 首启动默认基线
- 配置网页
  - 运行时读写入口

建议流程是：

- 构建期只保留通用默认值
- 现场配网、provider 凭据和门户参数通过配置页填写

## 常见误区

- 不要把项目当成“P4 本地自带 Wi-Fi”的普通板卡
- 不要打开 `CONFIG_ESP_HOST_WIFI_ENABLED`
- 不要把 `sdkconfig` 当成唯一可提交配置来源
- 不要把本机敏感项直接写进 `sdkconfig.defaults`

## 后续阅读

- [ARCHITECTURE.md](./ARCHITECTURE.md)
- [CONFIGURATION.md](./CONFIGURATION.md)
- [DEVELOPMENT.md](./DEVELOPMENT.md)
- [TESTING.md](./TESTING.md)
