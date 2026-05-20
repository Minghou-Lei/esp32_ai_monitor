---
last_mapped_commit: ecd37fb43f75
refreshed: 2026-05-20
---

# Codebase Structure

## Root Layout

```text
.
├─ CMakeLists.txt
├─ dependencies.lock
├─ partitions_32mb_singleapp.csv
├─ sdkconfig.defaults
├─ main/
├─ components/
├─ docs/
├─ .planning/
└─ .vscode/
```

Ignored local/build state includes:

- `build/`
- `managed_components/`
- `sdkconfig`
- `sdkconfig.old`
- `*.log`

## Entry Point

- `main/main.c`
  - Starts UI, network, provider polling, config web service, and board input.
- `main/CMakeLists.txt`
  - Registers the main component.
- `main/idf_component.yml`
  - Declares managed dependencies consumed by the application component.

## Project Components

### `components/app_config_service`

Purpose: canonical runtime configuration service.

Key files:

- `components/app_config_service/include/app_config_service.h`
- `components/app_config_service/app_config_service.c`
- `components/app_config_service/Kconfig.projbuild`
- `components/app_config_service/CMakeLists.txt`

Add code here when changing:

- config schema,
- Kconfig defaults,
- NVS persistence,
- validation rules,
- string conversion for config enums.

### `components/network_service`

Purpose: Wi-Fi, enterprise auth, portal state, and fallback AP state machine.

Key files:

- `components/network_service/include/network_service.h`
- `components/network_service/network_service.c`
- `components/network_service/CMakeLists.txt`

Add code here when changing:

- Wi-Fi station startup,
- SoftAP fallback,
- portal completion semantics,
- network snapshot fields,
- Hosted / Remote interaction from the application layer.

### `components/provider_service`

Purpose: remote provider polling and normalized provider snapshot.

Key files:

- `components/provider_service/include/provider_service.h`
- `components/provider_service/provider_service.c`
- `components/provider_service/CMakeLists.txt`

Add code here when changing:

- provider HTTP request construction,
- response parsing,
- provider state text,
- delta and hourly metrics,
- future provider kinds.

### `components/config_web_service`

Purpose: board-local web UI and REST API.

Key files:

- `components/config_web_service/include/config_web_service.h`
- `components/config_web_service/config_web_service.c`
- `components/config_web_service/CMakeLists.txt`

Add code here when changing:

- `/api/config`,
- `/api/status`,
- captive portal proxy,
- restart scheduling,
- embedded config page behavior.

### `components/ui_service`

Purpose: BSP + LVGL dashboard.

Key files:

- `components/ui_service/include/wifi_info_screen.h`
- `components/ui_service/include/monitor_dashboard_screen.h`
- `components/ui_service/monitor_dashboard_screen.c`
- `components/ui_service/assets/jnr_sb_font.ttf`
- `components/ui_service/CMakeLists.txt`

The current public startup symbol is `wifi_info_screen_start()`.

### `components/board_input_service`

Purpose: physical BOOT button action handling.

Key files:

- `components/board_input_service/include/board_input_service.h`
- `components/board_input_service/board_input_service.c`
- `components/board_input_service/CMakeLists.txt`

Current action: long press enters or toggles configuration AP behavior through `network_service`.

## Vendor / BSP Override Components

These directories are source-controlled component overrides:

- `components/espressif__esp_codec_dev`
- `components/waveshare__esp_lcd_st7703`
- `components/waveshare__esp32_p4_wifi6_touch_lcd_4b`

Treat them as part of the build surface. Avoid broad rewrites unless fixing a BSP compatibility issue.

## Documentation

Canonical docs:

- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/CONFIGURATION.md`
- `docs/GETTING-STARTED.md`
- `docs/DEVELOPMENT.md`
- `docs/TESTING.md`
- `docs/API.md`

Additional product note:

- `docs/ai-agent-monitor-proposal.md`

Codebase intelligence:

- `.planning/codebase/STACK.md`
- `.planning/codebase/ARCHITECTURE.md`
- `.planning/codebase/STRUCTURE.md`
- `.planning/codebase/CONVENTIONS.md`
- `.planning/codebase/TESTING.md`
- `.planning/codebase/INTEGRATIONS.md`
- `.planning/codebase/CONCERNS.md`

Research notes:

- `.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md`
- `.planning/research/2026-04-27-waveshare-esp32-p4-wifi6-touch-lcd-4b.md`

## Where To Add New Code

- New screen or LVGL view: `components/ui_service`
- New backend polling provider: `components/provider_service`
- New config field: `components/app_config_service` first, then `config_web_service`, then consumers.
- New board physical action: `components/board_input_service`
- New network behavior: `components/network_service`
- New local REST endpoint: `components/config_web_service`
- Build or hardware baseline change: `sdkconfig.defaults`, `partitions_32mb_singleapp.csv`, and docs.

## Naming Notes

- Public component APIs use service prefixes such as `app_config_`, `network_service_`, `provider_service_`, and `config_web_service_`.
- Internal helpers use the same prefix plus `static`.
- The UI startup name is legacy: `wifi_info_screen_start()` starts the current monitor dashboard.
