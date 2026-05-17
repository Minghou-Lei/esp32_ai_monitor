# Codebase Structure

**Analysis Date:** 2026-05-17

## Directory Layout

```text
esp32_ai_monitor/
├── main/                          # app entrypoint and top-level component manifest
├── components/                    # project-owned runtime services and component overrides
├── docs/                          # operator/developer-facing project documentation
├── .planning/                     # checked-in codebase maps and research notes
├── managed_components/            # ESP-IDF component-manager downloads, generated locally
├── build/                         # generated build output, not tracked
├── sdkconfig.defaults             # committed build baseline
├── sdkconfig                      # machine-effective config snapshot
├── dependencies.lock             # component dependency lockfile
└── partitions_32mb_singleapp.csv # custom flash partition layout
```

## Directory Purposes

**`main/`:**
- Purpose: define the application entrypoint and project-level component dependency manifest
- Contains: `main.c`, `CMakeLists.txt`, `idf_component.yml`
- Key files: `main/main.c`, `main/idf_component.yml`

**`components/app_config_service/`:**
- Purpose: canonical runtime configuration model
- Contains: public header, implementation, and `Kconfig.projbuild`
- Key files: `components/app_config_service/app_config_service.c`, `components/app_config_service/include/app_config_service.h`

**`components/network_service/`:**
- Purpose: Wi-Fi join path, portal state, and fallback AP behavior
- Contains: service implementation and public snapshot API
- Key files: `components/network_service/network_service.c`, `components/network_service/include/network_service.h`

**`components/provider_service/`:**
- Purpose: remote provider polling and derived provider metrics
- Contains: polling task, HTTP parsing helpers, and provider snapshot API
- Key files: `components/provider_service/provider_service.c`, `components/provider_service/include/provider_service.h`

**`components/config_web_service/`:**
- Purpose: board-local HTML portal and REST endpoints for runtime edits
- Contains: single-file embedded web app and HTTP handlers
- Key files: `components/config_web_service/config_web_service.c`, `components/config_web_service/include/config_web_service.h`

**`components/ui_service/`:**
- Purpose: LVGL-based dashboard UI
- Contains: display startup, font asset registration, and screen layout/rendering code
- Key files: `components/ui_service/monitor_dashboard_screen.c`, `components/ui_service/include/wifi_info_screen.h`, `components/ui_service/assets/jnr_sb_font.ttf`

**`components/espressif__esp_codec_dev/`, `components/waveshare__esp_lcd_st7703/`, `components/waveshare__esp32_p4_wifi6_touch_lcd_4b/`:**
- Purpose: project-local override copies for board/component dependencies
- Contains: vendor component sources and metadata
- Key files: component-local `CMakeLists.txt`, `idf_component.yml`, and source trees

**`docs/`:**
- Purpose: canonical human-facing project documentation
- Contains: README-adjacent guides, architecture notes, testing/configuration guidance, and supporting images
- Key files: `docs/ARCHITECTURE.md`, `docs/API.md`, `docs/CONFIGURATION.md`, `docs/images/dashboard-live.jpg`

**`.planning/`:**
- Purpose: checked-in planning intelligence consumed by GSD workflows
- Contains: `codebase/` map documents and `research/` notes
- Key files: `.planning/codebase/*.md`, `.planning/research/2026-04-27-esp32-p4-wifi-bringup-pitfalls.md`

## Key File Locations

**Entry Points:**
- `main/main.c`: boot order and service orchestration
- `components/ui_service/monitor_dashboard_screen.c`: on-device dashboard startup

**Configuration:**
- `sdkconfig.defaults`: committed firmware baseline
- `components/app_config_service/Kconfig.projbuild`: runtime default symbol surface
- `components/app_config_service/app_config_service.c`: NVS-backed runtime config implementation

**Core Logic:**
- `components/network_service/network_service.c`: Wi-Fi and portal state machine
- `components/provider_service/provider_service.c`: provider refresh loop and parsing
- `components/config_web_service/config_web_service.c`: local portal and REST API

**Testing / Verification:**
- No dedicated `tests/`, `test/`, or `.github/workflows/` directory detected
- Validation currently relies on `project://status`, local builds, and on-device flashing/monitoring

## Naming Conventions

**Files:**
- Service modules use lowercase snake-case component names such as `network_service.c` and `provider_service.h`
- UI still carries legacy `wifi_info_screen` names even when implementation meaning has shifted to a dashboard

**Directories:**
- ESP-IDF components live under `components/<component_name>/`
- vendor override components preserve the component-manager namespace with `namespace__component` naming

## Where to Add New Code

**New runtime feature:**
- Primary code: `components/<new_service>/`
- Public interface: `components/<new_service>/include/<new_service>.h`
- Wiring into boot: `main/main.c` and `main/CMakeLists.txt`

**New board/operator HTTP action:**
- Handler implementation: `components/config_web_service/config_web_service.c`
- Backing state/config contract: `components/app_config_service/` or the relevant service component

**New UI surface:**
- Screen implementation: `components/ui_service/`
- Asset registration: `components/ui_service/CMakeLists.txt`
- Shared state source: consume snapshots from `network_service` and `provider_service`

**Shared helpers / common contracts:**
- Prefer a dedicated new component under `components/` rather than deepening the circular dependency between `network_service` and `provider_service`

## Special Directories

**`managed_components/`:**
- Purpose: component-manager fetched sources
- Generated: Yes
- Committed: No

**`build/`:**
- Purpose: generated binaries, flash args, object files, and build metadata
- Generated: Yes
- Committed: No

**`.planning/codebase/`:**
- Purpose: checked-in codebase reference docs used by workflow tooling
- Generated: Yes, but intentionally committed
- Committed: Yes

**`docs/images/`:**
- Purpose: project screenshots and illustration assets
- Generated: No assumption
- Committed: Yes

---

*Structure analysis: 2026-05-17*
