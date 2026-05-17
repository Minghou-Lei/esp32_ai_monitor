# CONFIGURATION

## 配置分层

当前项目的配置应按四层理解：

- `sdkconfig.defaults`
  - 可提交、可复用的构建期默认基线
- `sdkconfig`
  - 当前机器的生效态
- `NVS`
  - 运行时配置覆盖存储
- `config_web_service`
  - 运行时的人机配置入口

不要把 `sdkconfig` 当成唯一真相，它只是当前机器的即时状态。

## 当前持久基线

`sdkconfig.defaults` 当前表达的关键方向包括：

- `CONFIG_IDF_TARGET="esp32p4"`
- `CONFIG_ESPTOOLPY_FLASHSIZE="32MB"`
- `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_32mb_singleapp.csv"`
- `CONFIG_SPIRAM=y`
- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

这几项共同定义了当前工程的硬约束：Hosted 无线、`PSRAM`、大字体与自定义分区表。

## 运行时配置模型

当前统一配置结构由 `app_config_service` 维护，至少包含这些域：

### Wi-Fi

- 基本接入参数
- 主机名
- 安全模式
- 企业认证参数
- 门户相关元数据

### 配置热点

- `enabled`
- `ssid`
- `password`

### Provider

- `kind`
- `display_name`
- `base_url`
- `endpoint_path`
- 鉴权与身份相关参数
- `refresh_interval_ms`

### UI

- `ui_refresh_interval_ms`

## Kconfig 默认值入口

`components/app_config_service/Kconfig.projbuild` 当前暴露了多类首启动默认值：

- Wi-Fi 基本信息
- 企业认证字段
- 门户元数据
- 配置热点参数
- provider 名称、端点和凭据字段
- UI 刷新周期

这些默认值用于首次启动或 `NVS` 尚未写入覆盖值时的配置装配。

## 运行时修改入口

当前推荐通过本地配置网页修改运行时参数。配置网页会：

- 读取当前配置快照
- 对用户输入做统一校验
- 把合法配置写回 `NVS`
- 提示用户在需要时重启设备以应用网络或 provider 变化

这比直接改 `sdkconfig` 更符合当前产品形态。

## 敏感信息处理

当前配置模型里已经包含多类敏感字段：

- Wi-Fi 密码
- EAP 用户名 / 密码
- 门户用户名 / 密码
- provider token
- provider 用户头值

因此必须坚持：

- 不在文档里写实际值
- 不把本机敏感项复制进 `sdkconfig.defaults`
- 审查 `sdkconfig`、日志和网页返回值时优先检查是否泄露

## 配置变更后的推荐动作

涉及这些内容时，建议动作顺序是：

1. 审查 `sdkconfig.defaults`
2. 运行 `idf.py reconfigure`
3. 运行 `idf.py build`
4. 如涉及显示或网络关键路径，上板验证

如果改动的是运行时配置模型或配置网页，则还应验证：

- 页面读写是否正常
- 保存后的校验行为是否正确
- 重启后配置是否按预期恢复
