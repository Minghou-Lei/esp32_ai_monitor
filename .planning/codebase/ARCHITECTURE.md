---
last_mapped_commit: ecd37fb43f75
refreshed: 2026-05-20
---

# Architecture

## System Overview

This repository is an ESP-IDF firmware project for a board-local AI / provider monitoring terminal. The current implementation is no longer just a Wi-Fi diagnostics bring-up. It now has:

- a BSP + LVGL dashboard,
- an NVS-backed runtime configuration model,
- Wi-Fi / enterprise / portal / fallback AP state management,
- remote provider polling,
- a board-local web configuration portal,
- a physical BOOT-button entry path for the config AP.

The product boundary is still embedded-terminal first: the board displays and controls monitoring state, while heavier AI or provider systems remain external.

## Boot Flow

`main/main.c` is intentionally thin. Current `app_main()` starts services in this order:

1. `wifi_info_screen_start()`
2. `network_service_start()`
3. `provider_service_start()`
4. `config_web_service_start()`
5. `board_input_service_start()`

This ordering gives the user immediate display feedback, then brings up network state, provider state, web configuration, and finally physical input monitoring.

## Component Graph

```text
app_main()
  ├─ wifi_info_screen_start()
  ├─ network_service_start()
  ├─ provider_service_start()
  ├─ config_web_service_start()
  └─ board_input_service_start()

app_config_service
  ├─ network_service
  ├─ provider_service
  ├─ config_web_service
  └─ ui_service

network_service ── snapshot ──► ui_service
provider_service ─ snapshot ──► ui_service
network_service ── snapshot ──► config_web_service
provider_service ─ snapshot ──► config_web_service
board_input_service ─ command ─► network_service
```

## Layers

### Entry Layer

- Location: `main/main.c`
- Responsibility: service startup orchestration and fatal start error logging.
- Constraint: do not move business logic into `app_main()`.

### Configuration Layer

- Location: `components/app_config_service`
- Public API: `components/app_config_service/include/app_config_service.h`
- Responsibility:
  - assemble defaults from `sdkconfig`,
  - load and save NVS overrides,
  - validate runtime configuration,
  - convert Wi-Fi security and provider enums to and from strings.

`app_config_t` is the central runtime contract. It contains:

- `wifi`: SSID, password, hostname, security mode, enterprise identity, portal metadata.
- `config_ap`: fallback AP enabled flag, SSID, password.
- `provider`: kind, display name, base URL, endpoint path, access token, management key, user header, refresh interval.
- `ui_refresh_interval_ms`.

### Network Layer

- Location: `components/network_service`
- Public API: `components/network_service/include/network_service.h`
- Responsibility:
  - initialize NVS, event loop, netif, and Wi-Fi runtime,
  - manage STA and APSTA fallback modes,
  - apply WPA2-PSK or WPA2-Enterprise settings,
  - maintain portal state,
  - expose a `network_service_snapshot_t`.

The snapshot includes connection readiness, SoftAP state, portal state, RSSI, channel, SSID, MAC addresses, IP data, DNS data, portal URL, and cipher/auth text.

### Provider Layer

- Location: `components/provider_service`
- Public API: `components/provider_service/include/provider_service.h`
- Responsibility:
  - poll configured external provider over HTTP,
  - normalize account/subscription data into two display items,
  - track fetch counts, failures, HTTP status, delta usage, and hourly amount.

The current provider enum exposes `APP_CONFIG_PROVIDER_AQI`. The API shape is more generic than the current implementation.

### Local Web Layer

- Location: `components/config_web_service`
- Public API: `components/config_web_service/include/config_web_service.h`
- Responsibility:
  - serve the embedded configuration page,
  - expose configuration and status REST endpoints,
  - proxy captive portal traffic when configured,
  - schedule device restart after saved configuration changes.

Current route surface:

- `GET /api/config`
- `POST /api/config`
- `GET /api/status`
- `POST /api/portal/complete`
- `POST /api/restart`
- `GET /portal/open`
- `ANY /portal/proxy*`
- `GET /*`

### UI Layer

- Location: `components/ui_service`
- Public API: `components/ui_service/include/wifi_info_screen.h`
- Main implementation: `components/ui_service/monitor_dashboard_screen.c`
- Responsibility:
  - initialize the BSP display and LVGL tree,
  - load embedded TinyTTF font asset,
  - render the AQI/provider dashboard,
  - periodically refresh network and provider snapshots,
  - expose diagnostic detail views through the on-device UI.

The public function name remains `wifi_info_screen_start()` even though the implementation has moved to the monitor dashboard concept.

### Physical Input Layer

- Location: `components/board_input_service`
- Public API: `components/board_input_service/include/board_input_service.h`
- Responsibility:
  - poll the upper BOOT button,
  - detect a long press,
  - request configuration AP entry through `network_service`.

Current constants in implementation use GPIO 35, 50 ms polling, and a 2 second long-press threshold.

## Data Flows

### Runtime Configuration Flow

```text
Kconfig defaults
  └─ app_config_load()
       ├─ NVS override if valid
       └─ app_config_t snapshot
            ├─ network_service
            ├─ provider_service
            ├─ config_web_service
            └─ ui_service
```

### Provider Refresh Flow

```text
provider_service task
  ├─ reads app_config_t
  ├─ waits for network readiness
  ├─ performs HTTP request
  ├─ parses provider response
  └─ publishes provider_service_snapshot_t
```

### Config Save Flow

```text
POST /api/config
  ├─ parse submitted form body
  ├─ validate app_config_t
  ├─ save NVS blob
  └─ schedule restart
```

## Architectural Constraints

- Display and touch should stay BSP-led unless the BSP cannot satisfy a requirement.
- Wireless must remain on the ESP-Hosted / Wi-Fi Remote path for this board.
- Runtime credentials belong in NVS or ignored machine config, not documentation.
- `sdkconfig.defaults` is the committed baseline; `sdkconfig` is machine-effective state.
- The UI is LVGL-only.
- HTTP polling is the current provider integration model; WebSocket or MQTT would be a later architectural change.

## Residual Smells

- `wifi_info_screen_start()` no longer describes the dashboard role.
- `config_web_service.c` is large because it holds HTML, JSON, REST handlers, proxy code, and restart scheduling in one file.
- Provider parsing appears tailored to the current AQI response while the public config model is already generic.
- Sensitive fields are returned by the local config API; this is acceptable for current local bring-up but should be tightened before production exposure.
