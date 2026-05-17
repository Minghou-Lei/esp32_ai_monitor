# Coding Conventions

**Analysis Date:** 2026-05-17

## Naming Patterns

**Files:**
- C source and header files use lowercase snake case, typically matching the component name: `app_config_service.c`, `network_service.h`
- UI files may preserve legacy names while implementation meaning evolves, for example `include/wifi_info_screen.h` alongside `monitor_dashboard_screen.c`

**Functions:**
- Public component APIs are prefixed with the component name: `network_service_start()`, `provider_service_request_refresh()`, `config_web_service_start()`
- Internal helpers are `static` and keep the same prefix, e.g. `network_service_refresh_runtime_fields_locked()`

**Variables:**
- File-scope statics use `s_` prefixes, such as `s_snapshot`, `s_state_lock`, and `s_server`
- Constants use full uppercase snake case, for example `CONFIG_WEB_SERVICE_BODY_LIMIT`

**Types:**
- Public structs and enums use `<component>_<concept>_t` naming, such as `app_config_t`, `network_service_snapshot_t`, and `provider_service_state_t`

## Code Style

**Formatting:**
- No formatter config files detected
- Current style is ESP-IDF-flavored C with four-space indentation and opening brace on the same line

**Linting:**
- No repository-local lint config detected
- Style consistency is enforced manually through the existing component patterns

## Import / Include Organization

**Order:**
1. Component’s own public header, e.g. `#include "provider_service.h"`
2. C standard library headers
3. ESP-IDF / FreeRTOS headers
4. Peer component headers

**Path Aliases:**
- No path alias system detected
- Includes use direct component headers made visible through `INCLUDE_DIRS "include"`

## Error Handling

**Patterns:**
- Return `esp_err_t` from public operations that can fail
- Use `ESP_RETURN_ON_ERROR` and `ESP_GOTO_ON_FALSE` style guard macros in implementation files where applicable
- Convert background/runtime failures into human-readable status strings for UI and HTTP consumers instead of aborting the firmware

## Logging

**Framework:** `ESP_LOG*`

**Patterns:**
- Each implementation file defines a `static const char *TAG`
- Startup failures are logged from `main/main.c`, while service-specific operational issues are localized to the owning component

## Comments

**When to Comment:**
- Public headers and source files typically begin with file-level block comments
- Brief notes are used where the code is non-obvious, such as server time handling in `provider_service_fetch_once()`

**API Comments:**
- Public headers in `components/*/include/*.h` carry concise Doxygen-style comments for exported types and functions

## Function Design

**Size:**
- Service implementation files are intentionally large and self-contained
- Logic is still decomposed into prefixed static helper functions instead of pushing everything into one giant public function

**Parameters:**
- Output buffers are usually passed as `(char *dst, size_t dst_size, ...)`
- Snapshot copy APIs use explicit out-parameters, such as `network_service_get_snapshot(network_service_snapshot_t *out)`

**Return Values:**
- Public start/save/reset functions return `esp_err_t`
- Getter functions that cannot meaningfully fail expose `void` and fill caller-provided structs

## Module Design

**Exports:**
- Public surface area is small and header-driven
- Each component exposes only a few lifecycle and snapshot APIs through `include/`

**Barrel Files:**
- Not used
- Component boundaries are managed by ESP-IDF component registration and direct header inclusion

## Prescriptive Rules to Follow

- Put new runtime capabilities in their own `components/<name>/` directory instead of expanding `main/main.c`
- Reuse the `app_config_t` model for any new operator-editable setting rather than inventing service-local config stores
- Expose UI-consumable runtime state through snapshot structs instead of direct UI callbacks from service code
- Match the existing ESP-IDF error-return style and logging conventions when adding or changing service code

---

*Convention analysis: 2026-05-17*
