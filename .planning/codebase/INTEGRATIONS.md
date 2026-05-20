---
last_mapped_commit: ecd37fb43f75
refreshed: 2026-05-20
---

# External Integrations

## Hardware Integrations

Target board:

- `Waveshare ESP32-P4-WIFI6-Touch-LCD-4B`

Important hardware assumptions:

- `ESP32-P4` is the main controller.
- Onboard `ESP32-C6` provides wireless capability through the Hosted / Remote path.
- The display is a 4 inch 720 x 720 touch LCD.
- UART is the normal flash / monitor path.
- The upper BOOT button is used by `board_input_service` for config AP entry.

## BSP / Display Integrations

Display, touch, audio-adjacent BSP support comes through:

- `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- `waveshare/esp_lcd_st7703`
- `espressif/esp_lvgl_port`
- `espressif/esp_lcd_touch_gt911`
- `espressif/esp_codec_dev`

The application should prefer BSP entry points over hand-written panel timing or touch initialization.

## Wireless Integration

Wireless is not native P4 Wi-Fi. Current build direction is:

- `ESP32-P4` host
- onboard `ESP32-C6` coprocessor
- `ESP-Hosted`
- `esp_wifi_remote`
- SDIO host interface

Relevant baseline symbols:

- `CONFIG_ESP_WIFI_REMOTE_ENABLED=y`
- `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`
- `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `# CONFIG_ESP_HOST_WIFI_ENABLED is not set`

If Wi-Fi bring-up fails with Hosted or `net80211`-style errors, check the Hosted / Remote configuration before changing business logic.

## Provider Integration

`components/provider_service` currently polls a configured provider over HTTP.

Configuration fields include:

- provider kind,
- display name,
- base URL,
- endpoint path,
- access token,
- management key,
- user header name,
- user header value,
- refresh interval.

The current public enum includes `APP_CONFIG_PROVIDER_AQI`. The implementation is shaped around the current provider response while leaving room for future provider kinds.

## Local Web Integration

`components/config_web_service` exposes a local HTTP control plane:

- `GET /api/config`
- `POST /api/config`
- `GET /api/status`
- `POST /api/portal/complete`
- `POST /api/restart`
- `GET /portal/open`
- `ANY /portal/proxy*`
- `GET /*`

It also serves the embedded HTML configuration page and can proxy configured portal traffic.

## Storage Integration

Runtime configuration is persisted in NVS through:

- namespace: internal to `app_config_service`
- blob key: internal to `app_config_service`
- public API:
  - `app_config_load()`
  - `app_config_save()`
  - `app_config_reset_to_defaults()`

Do not duplicate configuration persistence in network, provider, UI, or web components.

## MCP / Tooling Integration

The repository contract prefers ESP-IDF MCP for project actions when available.

Useful resources:

- `project://config`
- `project://status`
- `project://devices`

Use these resources for current target, build directory, IDF version, and port discovery before invoking build or flash actions.

## Security-Relevant Integrations

Sensitive values exist in the runtime model:

- Wi-Fi password,
- enterprise credentials,
- portal username and password,
- provider access token,
- provider management key,
- provider user header value.

Risk surfaces:

- `sdkconfig` can contain machine-local default credentials and is ignored.
- `/api/config` exposes config fields to the local portal.
- provider request logs must not reveal authorization material.
- generated docs and screenshots must be scanned before commit.
