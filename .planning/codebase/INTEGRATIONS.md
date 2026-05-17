# External Integrations

**Analysis Date:** 2026-05-17

## APIs & External Services

**Board / Firmware Integrations:**
- `Waveshare BSP` - display, touch, and board-level helper APIs consumed by `components/ui_service/monitor_dashboard_screen.c`
  - SDK/Client: `waveshare/esp32_p4_wifi6_touch_lcd_4b`
  - Auth: not applicable
- `ESP-Hosted / esp_wifi_remote` - Wi-Fi transport between `ESP32-P4` and onboard `ESP32-C6`
  - SDK/Client: `espressif/esp_hosted`, `espressif/esp_wifi_remote`
  - Auth: board-side transport configuration from `sdkconfig.defaults`

**Remote Provider Service:**
- AQI-style subscription endpoint polled by `components/provider_service/provider_service.c`
  - SDK/Client: `esp_http_client`
  - Auth: runtime config fields surfaced by `components/app_config_service/include/app_config_service.h`
  - Request shape: `Authorization: Bearer <token>` plus a configurable user header
  - Endpoint fallback: `/api/subscription/self`

## Data Storage

**Databases:**
- Not detected

**File Storage:**
- Local firmware assets only
  - TTF asset bundled into the firmware image through `components/ui_service/CMakeLists.txt`

**Persistent Runtime Storage:**
- `NVS` namespace `app_cfg` for runtime configuration in `components/app_config_service/app_config_service.c`

**Caching:**
- In-memory snapshots only
  - network snapshot in `components/network_service/network_service.c`
  - provider snapshot and hourly history ring in `components/provider_service/provider_service.c`

## Authentication & Identity

**Wi-Fi / Enterprise Network:**
- WPA2-PSK and WPA2-Enterprise settings stored in `app_config_t` from `components/app_config_service/include/app_config_service.h`
- Portal completion state managed by `network_service_mark_portal_complete()` in `components/network_service/network_service.c`

**Provider Auth:**
- Bearer token stored in `config.provider.access_token`
- Optional custom user header name/value stored in `config.provider.user_header_name` and `config.provider.user_header_value`
- No OAuth or third-party identity SDK detected in the firmware

## Monitoring & Observability

**Error Tracking:**
- None detected

**Logs:**
- Firmware logs use `ESP_LOG*` macros in `main/main.c` and the service components under `components/`
- MCP operation logs are exposed through `project://status` as part of the local engineering workflow

## CI/CD & Deployment

**Hosting / Delivery:**
- Firmware image is built locally for ESP32 hardware
- No cloud deploy target or hosted runtime detected in the repository

**CI Pipeline:**
- None detected
  - no `.github/workflows/` files found

## Environment Configuration

**Required runtime-config surfaces:**
- Wi-Fi connection settings
- optional enterprise credentials and portal metadata
- fallback configuration AP settings
- provider base URL, endpoint path, access token, management key, and user header fields
- UI and provider refresh intervals

**Secrets location:**
- Build defaults come from Kconfig symbols declared in `components/app_config_service/Kconfig.projbuild`
- Runtime overrides persist in NVS through `app_config_save()` in `components/app_config_service/app_config_service.c`
- Sensitive values are also exposed through the board-local config API in `components/config_web_service/config_web_service.c`

## Webhooks & Callbacks

**Incoming:**
- `GET /` - configuration portal HTML from `components/config_web_service/config_web_service.c`
- `GET /api/config`
- `POST /api/config`
- `GET /api/status`
- `POST /api/portal/complete`
- `POST /api/restart`

**Outgoing:**
- HTTPS `GET` requests to the configured provider endpoint from `components/provider_service/provider_service.c`

---

*Integration audit: 2026-05-17*
