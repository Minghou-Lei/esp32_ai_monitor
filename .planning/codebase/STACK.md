---
last_mapped_commit: ecd37fb43f75
refreshed: 2026-05-20
---

# Technology Stack

## Languages

- `C`
  - Application services, UI, networking, config, and HTTP handlers.
- `CMake`
  - ESP-IDF component registration and top-level project build.
- `Kconfig`
  - Build-time defaults for runtime configuration fields.
- Embedded `HTML` / `CSS` / `JavaScript`
  - Board-local configuration portal served by `esp_http_server`.

## Runtime

- Framework: `ESP-IDF v6.0.1`
- Target: `esp32p4`
- Board: `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`
- Main controller: `ESP32-P4`
- Wireless coprocessor path: onboard `ESP32-C6` through Hosted / Wi-Fi Remote integration.

## Build System

- Root build file: `CMakeLists.txt`
- Application component: `main/CMakeLists.txt`
- Component manager lock: `dependencies.lock`
- Persistent default config: `sdkconfig.defaults`
- Machine-effective config: `sdkconfig` is ignored and should not be treated as a clean source artifact.

## Frameworks

- UI:
  - `LVGL 9.x`
  - `espressif/esp_lvgl_port`
  - `LVGL TinyTTF`
- Board support:
  - `waveshare/esp32_p4_wifi6_touch_lcd_4b`
  - `waveshare/esp_lcd_st7703`
  - `espressif/esp_lcd_touch_gt911`
- Wireless:
  - `espressif/esp_hosted`
  - `espressif/esp_wifi_remote`
  - `espressif/wifi_remote_over_eppp`
  - `espressif/eppp_link`
- HTTP:
  - `esp_http_server` for board-local config and proxy endpoints.
  - `esp_http_client` for remote provider polling and portal proxying.
- Storage:
  - `nvs_flash` for persisted runtime configuration.

## Key Dependencies

The dependency graph is managed by ESP-IDF component manager and locked in `dependencies.lock`.

High-value dependencies:

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `waveshare/esp_lcd_st7703`
- `espressif/esp_codec_dev`
- `espressif/esp_hosted`
- `espressif/esp_wifi_remote`
- `espressif/esp_lvgl_port`
- `lvgl/lvgl`

Project-local override copies currently exist under:

- `components/espressif__esp_codec_dev`
- `components/waveshare__esp_lcd_st7703`
- `components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

These are part of the active build and should be treated as source-controlled BSP compatibility surface.

## Configuration Baseline

`sdkconfig.defaults` defines the committed hardware and framework baseline:

- `CONFIG_IDF_TARGET="esp32p4"`
- `CONFIG_ESPTOOLPY_FLASHSIZE="32MB"`
- `CONFIG_PARTITION_TABLE_CUSTOM=y`
- `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_32mb_singleapp.csv"`
- `CONFIG_SPIRAM=y`
- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`
- `CONFIG_LV_USE_CLIB_MALLOC=y`
- `CONFIG_LV_USE_TINY_TTF=y`

Application-level defaults are declared in `components/app_config_service/Kconfig.projbuild`.

## Platform Requirements

Development requirements:

- ESP-IDF environment capable of `idf.py reconfigure`, `idf.py build`, and `idf.py flash`.
- ESP-IDF MCP is preferred for project actions when available.
- UART flashing path to the target board for runtime verification.

Runtime assumptions:

- `32MB` flash.
- PSRAM enabled.
- Hosted Wi-Fi Remote path stays enabled.
- Display and touch initialization remain BSP-led.
