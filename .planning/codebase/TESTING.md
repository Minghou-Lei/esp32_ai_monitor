# Testing Patterns

**Analysis Date:** 2026-05-17

## Test Framework

**Runner:**
- Not detected
- No dedicated unit-test framework config files or test directories are present in the repository

**Assertion Library:**
- Not detected

**Run Commands:**
```bash
idf.py reconfigure        # Refresh generated config after dependency or sdkconfig changes
idf.py build              # Compile the firmware
idf.py -p <PORT> flash monitor  # Flash and verify behavior on hardware
```

## Test File Organization

**Location:**
- No `tests/`, `test/`, or `__tests__/` tree detected

**Naming:**
- Not applicable

**Structure:**
```text
No repository-level automated test tree detected.
```

## Test Structure

**Suite Organization:**
```c
/* Current verification is service-level and hardware-driven rather than test-runner-driven. */
```

**Patterns:**
- Build verification is currently the first guardrail
- Service behavior is inspected through on-device UI, config API responses, and serial logs
- MCP status snapshots are used to confirm build completion and toolchain state

## Mocking

**Framework:** Not used

**Patterns:**
```c
/* No mocking layer detected in the current repository. */
```

**What to Mock:**
- No existing pattern to follow

**What NOT to Mock:**
- Hardware-facing BSP, hosted Wi-Fi path, and provider polling currently need real build/on-device checks

## Fixtures and Factories

**Test Data:**
```c
/* Runtime configuration defaults come from Kconfig + NVS, not from test fixtures. */
```

**Location:**
- Not applicable

## Coverage

**Requirements:** No coverage threshold configured

**View Coverage:**
```bash
Not available in the current repository.
```

## Test Types

**Build Verification:**
- `project://status` and `project://config` are the fastest local checks for target, build dir, and recent operation status

**Hardware Bring-up Checks:**
- UI path: confirm the board boots into the LVGL dashboard defined in `components/ui_service/monitor_dashboard_screen.c`
- Network path: verify Wi-Fi / portal states exposed by `components/network_service/network_service.c`
- Provider path: verify refresh loop, failure texts, and snapshot values exposed by `components/provider_service/provider_service.c`
- Config portal path: verify `GET /api/config`, `POST /api/config`, `GET /api/status`, `POST /api/portal/complete`, and `POST /api/restart`

**Integration Tests:**
- Not automated
- Real integration currently happens on device across `app_config_service`, `network_service`, `provider_service`, `config_web_service`, and `ui_service`

**E2E Tests:**
- Not used

## Common Verification Patterns

**Smallest relevant validation:**
```bash
idf.py build
```

**Config-sensitive validation:**
```bash
idf.py reconfigure
idf.py build
```

**Runtime validation:**
```bash
idf.py -p <PORT> flash monitor
```

## Prescriptive Guidance

- Treat display, hosted Wi-Fi, provider polling, and config-portal changes as hardware-verification work even when the code compiles cleanly
- Re-run `reconfigure` before `build` after touching dependencies, `sdkconfig.defaults`, or partition settings
- Use the board-local HTTP surface and dashboard text as the current regression probes until a formal test harness exists

---

*Testing analysis: 2026-05-17*
