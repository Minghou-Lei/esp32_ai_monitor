<!-- refreshed: 2026-05-17 -->
# Architecture

**Analysis Date:** 2026-05-17

## System Overview

当前系统是一个基于 `ESP-IDF` 的嵌入式监控终端，采用“薄入口 + 组件化服务 + 共享快照”的分层结构。输入主要来自板上配置网页、Wi-Fi / 企业网状态、远端 provider HTTP 响应和定时器事件；输出主要是 LVGL 主监控屏、板上配置 API 响应以及串口日志。

```text
┌─────────────────────────────────────────────────────────────┐
│                    Runtime Entry Layer                      │
├─────────────────────────────────────────────────────────────┤
│ `main/main.c`                                               │
│ app_main() starts UI, network, provider, and config web    │
└──────────────┬───────────────────┬──────────────────────────┘
               │                   │
               ▼                   ▼
┌──────────────────────────┐  ┌──────────────────────────────┐
│   Shared Config Layer    │  │   Local Interaction Layer    │
│ `components/app_config_  │  │ `components/ui_service/`     │
│  service/`               │  │ `components/config_web_      │
│                          │  │  service/`                   │
└──────────────┬───────────┘  └──────────────┬───────────────┘
               │                              │
               ▼                              ▼
┌─────────────────────────────────────────────────────────────┐
│               Runtime Service Layer                          │
│ `components/network_service/`                                │
│ `components/provider_service/`                               │
└──────────────┬──────────────────────────────┬───────────────┘
               │                              │
               ▼                              ▼
┌──────────────────────────┐  ┌──────────────────────────────┐
│   Board / IDF Backends   │  │     External Provider API    │
│ ESP-Hosted, esp_wifi_    │  │ Configured HTTPS endpoint    │
│ remote, BSP, LVGL, NVS   │  │ polled via esp_http_client   │
└──────────────────────────┘  └──────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Entry orchestration | Start UI, network, provider, and config web services | `main/main.c` |
| Shared runtime config | Load defaults, validate edits, and persist runtime overrides to NVS | `components/app_config_service/app_config_service.c` |
| Network state machine | Drive STA / fallback AP behavior and expose a normalized network snapshot | `components/network_service/network_service.c` |
| Provider polling | Fetch remote provider data and publish a normalized provider snapshot | `components/provider_service/provider_service.c` |
| Local config portal | Serve HTML + REST endpoints for runtime configuration and maintenance actions | `components/config_web_service/config_web_service.c` |
| Board UI | Render the dashboard with BSP + LVGL + TinyTTF | `components/ui_service/monitor_dashboard_screen.c` |

## Pattern Overview

**Overall:** service-oriented embedded runtime with shared snapshot structs

**Key Characteristics:**
- `app_main()` is orchestration-only and delegates behavior to components
- Runtime services communicate primarily through pull-based snapshot accessors rather than direct UI callbacks
- Configuration is centralized in one model (`app_config_t`) and consumed by all runtime services

## Layers

**Entry Layer:**
- Purpose: define boot ordering and error logging
- Location: `main/main.c`
- Contains: `app_main()` and service start calls
- Depends on: all runtime services
- Used by: ESP-IDF startup runtime

**Configuration Layer:**
- Purpose: own the canonical runtime configuration model
- Location: `components/app_config_service/`
- Contains: default assembly, validation, NVS save/reset, string conversion helpers
- Depends on: `nvs_flash` and `sdkconfig`
- Used by: `network_service`, `provider_service`, `config_web_service`, and indirectly the UI

**Network Layer:**
- Purpose: manage Wi-Fi join path, enterprise config, portal gating, and fallback AP exposure
- Location: `components/network_service/`
- Contains: state machine, event handlers, snapshot refresh, portal state transitions
- Depends on: `app_config_service`, `esp_event`, `esp_netif`, `esp_wifi`, `lwip`, and `wpa_supplicant`
- Used by: `provider_service`, `config_web_service`, and `ui_service`

**Provider Layer:**
- Purpose: poll the configured remote provider and normalize provider/account data into UI-ready fields
- Location: `components/provider_service/`
- Contains: HTTP request logic, lightweight parsing, delta/hourly tracking, refresh task
- Depends on: `app_config_service`, `esp_http_client`, `esp_timer`, `mbedtls`, and `network_service`
- Used by: `config_web_service` and `ui_service`

**Interaction Layer:**
- Purpose: provide operator-facing UI and board-local configuration entry
- Location: `components/ui_service/` and `components/config_web_service/`
- Contains: LVGL screen tree, periodic refresh timer, HTML portal, REST handlers, reboot hook
- Depends on: snapshots from `network_service` and `provider_service`
- Used by: human operator on-device or over the local network

## Data Flow

### Primary Boot Path

1. `app_main()` starts all runtime services in order (`main/main.c:10`)
2. `wifi_info_screen_start()` initializes the display and screen tree (`components/ui_service/monitor_dashboard_screen.c:958`)
3. `network_service_start()` initializes NVS, event loop, netif, and Wi-Fi runtime (`components/network_service/network_service.c:489`)
4. `provider_service_start()` spawns the polling task (`components/provider_service/provider_service.c:947`)
5. `config_web_service_start()` binds the board-local HTTP routes (`components/config_web_service/config_web_service.c:429`)

### Runtime Configuration Flow

1. `GET /api/config` serializes the current runtime config (`components/config_web_service/config_web_service.c:190`)
2. `POST /api/config` decodes form data and rewrites `app_config_t` (`components/config_web_service/config_web_service.c:309`)
3. `app_config_validate()` enforces field constraints (`components/app_config_service/app_config_service.c:139`)
4. `app_config_save()` persists the blob to NVS (`components/app_config_service/app_config_service.c:274`)
5. Services pick up refreshed config on their next read or restart cycle

### Provider Refresh Flow

1. `provider_service_poll_task()` loops forever with task notifications and refresh delay (`components/provider_service/provider_service.c:931`)
2. `provider_service_fetch_once()` rebuilds request config from the latest `app_config_t`
3. `esp_http_client` performs the HTTPS GET and captures headers/body (`components/provider_service/provider_service.c`)
4. Parsed values are folded into `provider_service_snapshot_t`
5. UI and config API pull the latest snapshot through `provider_service_get_snapshot()` (`components/provider_service/provider_service.c:980`)

**State Management:**
- Shared mutable state is held in module-level static structs guarded by FreeRTOS semaphores in `network_service` and `provider_service`
- UI and HTTP handlers are consumers that request snapshot copies, not owners of the underlying state

## Key Abstractions

**`app_config_t`:**
- Purpose: single runtime configuration contract
- Examples: `components/app_config_service/include/app_config_service.h`
- Pattern: centralized configuration object shared by services

**`network_service_snapshot_t`:**
- Purpose: normalized network/portal status surface for UI and HTTP
- Examples: `components/network_service/include/network_service.h`
- Pattern: pull-based snapshot DTO

**`provider_service_snapshot_t`:**
- Purpose: normalized remote provider state surface
- Examples: `components/provider_service/include/provider_service.h`
- Pattern: pull-based snapshot DTO with derived metrics

**Embedded config portal routes:**
- Purpose: local operator control plane
- Examples: `components/config_web_service/config_web_service.c`
- Pattern: small REST + HTML surface over `esp_http_server`

## Entry Points

**Firmware boot entry:**
- Location: `main/main.c`
- Triggers: ESP-IDF application startup
- Responsibilities: initialize major services and log startup failures

**Config web server entry:**
- Location: `components/config_web_service/config_web_service.c`
- Triggers: `config_web_service_start()`
- Responsibilities: bind `GET /`, `GET /api/config`, `POST /api/config`, `GET /api/status`, `POST /api/portal/complete`, and `POST /api/restart`

**UI entry:**
- Location: `components/ui_service/monitor_dashboard_screen.c`
- Triggers: `wifi_info_screen_start()`
- Responsibilities: initialize BSP display, load fonts, create the dashboard layout, and arm refresh timers

## Architectural Constraints

- **Threading:** service state lives behind FreeRTOS synchronization; provider polling runs in its own task and UI updates run from LVGL-related timer callbacks
- **Global state:** `network_service` and `provider_service` both own static singleton state blocks in their `.c` files
- **Circular component dependency:** `components/network_service/CMakeLists.txt` requires `provider_service`, while `components/provider_service/CMakeLists.txt` requires `network_service`
- **Board coupling:** display path is intentionally tied to the Waveshare BSP and LVGL/TinyTTF configuration
- **Hosted Wi-Fi constraint:** wireless path assumes `ESP-Hosted + esp_wifi_remote`, not native on-chip Wi-Fi on the P4

## Anti-Patterns

### Public API name diverges from implementation meaning

**What happens:** the UI implementation file is `monitor_dashboard_screen.c`, but the exported start symbol remains `wifi_info_screen_start()`.
**Why it's wrong:** it hides the current product shape and makes future refactors easier to misread.
**Do this instead:** keep docs explicit about the mismatch and rename the public API only as part of a coordinated cleanup touching `main/main.c` and `components/ui_service/include/wifi_info_screen.h`.

### Cross-service compile-time coupling

**What happens:** `network_service` and `provider_service` require each other at the component level.
**Why it's wrong:** it hardens boundaries, complicates extraction of shared contracts, and increases rebuild fragility.
**Do this instead:** move shared types or readiness queries into a thinner shared contract component before adding more runtime coordination.

## Error Handling

**Strategy:** fail soft on startup and surface runtime failures through status text rather than aborting the app

**Patterns:**
- `ESP_RETURN_ON_ERROR` guards HTTP handler and initialization code in `components/config_web_service/config_web_service.c`
- `provider_service_set_status()` converts HTTP and parsing failures into user-visible provider state text
- `main/main.c` logs individual service start failures without stopping the whole boot sequence

## Cross-Cutting Concerns

**Logging:** `ESP_LOGE` / `ESP_LOGI` across entry and service components
**Validation:** centralized in `app_config_validate()` and reused by config portal saves
**Persistence:** NVS-backed runtime config plus in-memory snapshot state
**Operator access:** board-local HTTP portal plus the LVGL dashboard

---

*Architecture analysis: 2026-05-17*
