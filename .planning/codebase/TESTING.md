---
last_mapped_commit: ecd37fb43f75
refreshed: 2026-05-20
---

# Testing Patterns

## Current Test Infrastructure

No dedicated automated test suite is present in the repository.

Not detected:

- `tests/`
- `test/`
- Unity component tests
- host-side test runner
- CI workflow

Validation is currently build-driven and board-driven.

## Primary Validation Paths

Preferred project-action path when available:

1. Read `project://config`.
2. Read `project://status`.
3. For build-sensitive changes, use ESP-IDF MCP build.
4. For runtime hardware validation, use ESP-IDF MCP flash with a confirmed port.

CLI fallback path:

```powershell
idf.py -C "E:\esp32_ai_monitor" reconfigure
idf.py -C "E:\esp32_ai_monitor" build
idf.py -C "E:\esp32_ai_monitor" -p <PORT> flash monitor
```

Use CLI only when MCP is unavailable, stale, or insufficient for the action.

## Smallest Relevant Checks

Documentation-only changes:

- inspect generated docs against current source,
- run a secret scan over changed docs,
- run `git diff --check`.

Configuration changes:

- `reconfigure`,
- `build`,
- review `sdkconfig.defaults` and avoid committing `sdkconfig`.

Code changes:

- targeted build at minimum,
- on-device validation when display, network, provider polling, or input behavior changes.

Display changes:

- build,
- flash,
- verify the board shows the dashboard,
- verify no first-frame LVGL / TinyTTF / PSRAM reset.

Network changes:

- build,
- flash,
- verify STA connection or fallback AP path,
- verify `network_service_snapshot_t` fields on UI and `/api/status`.

Provider changes:

- build,
- verify provider polling state transitions,
- verify success/failure counters and status text,
- verify no credential material appears in logs.

Config portal changes:

- build,
- verify `GET /api/config`,
- verify `POST /api/config`,
- verify `GET /api/status`,
- verify restart scheduling if config is saved.

Board input changes:

- build,
- flash,
- hold the upper BOOT button long enough to trigger config AP behavior,
- verify short presses do not interrupt the dashboard.

## Manual Acceptance Points

Core board acceptance:

- Display initializes through the Waveshare BSP.
- Backlight turns on.
- LVGL dashboard is visible and refreshed.
- Touch interaction or detail toggles remain responsive where implemented.
- Wi-Fi state updates between unconfigured, connecting, connected, portal, and fallback modes.
- Provider state appears on the dashboard when credentials and network are ready.
- Web config page can be reached on the board-local address.

## Known Verification Gaps

- No unit tests for `app_config_validate()`.
- No parser tests for provider response handling.
- No HTTP route tests for `config_web_service`.
- No automated UI smoke test.
- No CI gate.
- No fixture backend for provider polling.
- No automated secret regression test for generated docs.

## Risk-Based Guidance

For low-risk docs-only work, a build is not the most relevant check. Prefer doc/source verification, secret scan, and diff sanity.

For firmware behavior changes, compilation alone is not enough. The board, Hosted Wi-Fi route, PSRAM, LVGL allocator, and provider path must be verified on-device.
