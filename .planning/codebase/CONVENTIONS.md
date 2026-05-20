---
last_mapped_commit: ecd37fb43f75
refreshed: 2026-05-20
---

# Coding Conventions

## Language Style

- C source uses ESP-IDF-style `esp_err_t` returns for fallible public operations.
- Boolean state uses `bool` from `<stdbool.h>`.
- Fixed-width counters and timestamps use `<stdint.h>` types.
- Public headers are wrapped for C++ consumers with `extern "C"`.

## Naming

Public APIs use component prefixes:

- `app_config_load()`
- `app_config_save()`
- `network_service_start()`
- `network_service_get_snapshot()`
- `provider_service_get_snapshot()`
- `config_web_service_start()`
- `board_input_service_start()`
- `wifi_info_screen_start()`

Static helpers generally keep the component prefix:

- `network_service_copy_text()`
- `provider_service_publish_snapshot()`
- `config_web_service_write_json_status()`
- `wifi_info_screen_build_details_text()`

## File Organization

Each project component follows the ESP-IDF component pattern:

```text
components/<name>/
├─ CMakeLists.txt
├─ include/<name>.h
└─ <name>.c
```

Exceptions:

- `app_config_service` also owns `Kconfig.projbuild`.
- `ui_service` owns assets under `components/ui_service/assets/`.
- Vendor/BSP override components mirror upstream component layout.

## Error Handling

Common patterns:

- Use `ESP_RETURN_ON_ERROR()` for immediate propagation.
- Log service start failures in `main/main.c` without preventing later services from attempting startup.
- Validate pointers and buffer sizes before writing formatted strings.
- Return `ESP_ERR_INVALID_ARG` for invalid input and `ESP_ERR_NO_MEM` for failed allocation.

Guidance:

- Keep recoverable configuration validation errors explicit and user-readable.
- Do not swallow `esp_err_t` from ESP-IDF calls in new code.
- If a service can be called twice, follow the existing idempotent start pattern where practical.

## Resource Management

Existing code uses deterministic cleanup patterns:

- `free()` after allocated response buffers.
- ESP-IDF handles are kept in static service state.
- Timer handles are service-owned.
- HTTP server handles are static and guarded against duplicate start.

For new code:

- Release heap allocations on every error path.
- Keep ESP-IDF handles owned by the component that creates them.
- Avoid returning pointers to mutable static state; expose snapshots by copy.

## Snapshot Pattern

Runtime cross-component state is exposed through pull-based snapshots:

- `network_service_snapshot_t`
- `provider_service_snapshot_t`

The UI and web layer should read snapshots instead of reaching into service internals.

## Configuration Pattern

The required order for new runtime configuration fields is:

1. Add field to `app_config_t`.
2. Add Kconfig default in `components/app_config_service/Kconfig.projbuild` if build-time default is needed.
3. Populate defaults in `app_config_service.c`.
4. Validate field in `app_config_validate()`.
5. Expose or mask it through `config_web_service.c` as appropriate.
6. Update docs and secret-scan the result.

## UI Pattern

- UI code is LVGL-based and currently centralized in `components/ui_service/monitor_dashboard_screen.c`.
- Text update helpers avoid unnecessary LVGL label rewrites.
- Network and provider data should flow in through snapshots.
- The board-local web config page is not the on-device LVGL config view.

## Logging

- Component logs use static `TAG` constants.
- Startup failures are logged with `esp_err_to_name(err)`.
- New logs must not print credentials, tokens, cookies, or full provider authorization headers.

## Comments

Existing comments are mostly file-level and interface-level Chinese comments. New comments should explain:

- component responsibility,
- invariants,
- hardware constraints,
- non-obvious ESP-IDF behavior.

Do not add process notes, implementation diary text, or TODOs without owner/tracking context.
