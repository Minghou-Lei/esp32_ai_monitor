# Technology Stack

**Analysis Date:** 2026-05-17

## Languages

**Primary:**
- `C` - application code in `main/main.c` and `components/*/*.c`
- `CMake` - component and top-level build definitions in `CMakeLists.txt`, `main/CMakeLists.txt`, and `components/*/CMakeLists.txt`
- `Kconfig` - runtime default configuration surface in `components/app_config_service/Kconfig.projbuild` and `sdkconfig.defaults`

**Secondary:**
- `HTML/CSS/JavaScript` - embedded single-file configuration portal in `components/config_web_service/config_web_service.c`
- `Markdown` - project and planning documentation in `README.md`, `docs/*.md`, and `.planning/**/*.md`

## Runtime

**Environment:**
- `ESP-IDF v6.0.1` - detected from `project://status` and `project://config`
- `esp32p4` target - configured in `sdkconfig.defaults` and confirmed by `project://status`

**Package Manager:**
- ESP-IDF Component Manager
- Lockfile: present in `dependencies.lock`

## Frameworks

**Core:**
- `ESP-IDF` - firmware runtime, RTOS integration, networking, HTTP client/server, timers, and NVS
- `ESP-Hosted + esp_wifi_remote` - Wi-Fi host/slave path for `ESP32-P4 + ESP32-C6`, configured by `sdkconfig.defaults`
- `Waveshare BSP` - board support through `waveshare/esp32_p4_wifi6_touch_lcd_4b` in `main/idf_component.yml`

**UI / Interaction:**
- `LVGL` - board UI rendering through `components/ui_service/monitor_dashboard_screen.c`
- `TinyTTF` - runtime font loading enabled by `CONFIG_LV_USE_TINY_TTF=y` in `sdkconfig.defaults`

**Build / Dev:**
- `CMake + Ninja` - generated ESP-IDF build system, rooted at `CMakeLists.txt`
- `ESP-IDF MCP` - project inspection and engineering actions through `project://config` and `project://status`

## Key Dependencies

**Critical:**
- `waveshare/esp32_p4_wifi6_touch_lcd_4b` - board BSP for display, touch, and board bring-up; declared in `main/idf_component.yml`
- `waveshare/esp_lcd_st7703` - LCD panel driver override; declared in `main/idf_component.yml`
- `espressif/esp_wifi_remote` - hosted Wi-Fi client path; declared in `main/idf_component.yml`
- `espressif/esp_hosted` - host/slave transport support; declared in `main/idf_component.yml`

**Infrastructure:**
- `esp_http_server` - board-local configuration portal in `components/config_web_service/CMakeLists.txt`
- `esp_http_client` - remote provider polling in `components/provider_service/CMakeLists.txt`
- `nvs_flash` - runtime configuration persistence in `components/app_config_service/app_config_service.c`
- `mbedtls` and `esp_crt_bundle` - HTTPS provider requests in `components/provider_service/provider_service.c`

## Configuration

**Environment:**
- Build-time baseline is committed in `sdkconfig.defaults`
- Machine-effective configuration lives in `sdkconfig`
- Runtime overrides are persisted to NVS by `components/app_config_service/app_config_service.c`
- On-device edits flow through the embedded portal in `components/config_web_service/config_web_service.c`

**Build:**
- Top-level project file: `CMakeLists.txt`
- Main component build file: `main/CMakeLists.txt`
- Component dependency manifest: `main/idf_component.yml`
- Custom partition table: `partitions_32mb_singleapp.csv`
- Component lockfile: `dependencies.lock`

## Platform Requirements

**Development:**
- `ESP-IDF v6.0.1`
- `esp32p4` target selected before build
- Access to the board’s UART flashing path for on-device verification
- MCP or CLI ability to run `idf.py reconfigure`, `idf.py build`, and `idf.py -p <PORT> flash monitor`

**Production / Runtime Target:**
- `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`
- `ESP32-P4` as main controller
- onboard `ESP32-C6` serving Wi-Fi through hosted/remote integration
- `32MB` flash, `PSRAM`, custom single-app partitioning, and LVGL font rendering enabled

---

*Stack analysis: 2026-05-17*
