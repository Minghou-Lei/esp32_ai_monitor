---
last_mapped_commit: ecd37fb43f75
refreshed: 2026-05-20
---

# Codebase Concerns

## High-Value Risks

### Local config API exposes sensitive fields

The runtime config model includes Wi-Fi credentials, enterprise credentials, portal credentials, provider token, management key, and user header values. The local config page needs these fields to save settings, but readback behavior should be treated as sensitive.

Relevant files:

- `components/app_config_service/include/app_config_service.h`
- `components/config_web_service/config_web_service.c`
- `docs/API.md`

Before exposing the portal beyond trusted local setup, consider masking secret readback or using write-only secret update semantics.

### `sdkconfig` is machine-effective state

`sdkconfig` is ignored and may contain local Wi-Fi credentials or host-specific values. Do not copy values from it into docs or `sdkconfig.defaults`.

Committed baseline should remain:

- `sdkconfig.defaults`
- `partitions_32mb_singleapp.csv`
- component Kconfig defaults without private credentials.

### Hosted Wi-Fi path can drift

The target board depends on Hosted / Wi-Fi Remote. Accidentally enabling local host Wi-Fi, changing SDIO assumptions, or treating P4 as native Wi-Fi would break the board model.

Risk indicators:

- Hosted version mismatch warnings.
- `esp_wifi_init` failures.
- `OS adapter function version error`.
- `net80211`-style errors that suggest the wrong Wi-Fi path.

### LVGL / TinyTTF / PSRAM memory pressure

The dashboard uses LVGL and embedded TinyTTF. The current stable direction is CLIB malloc plus PSRAM. Changes to font size, assets, LVGL allocator, or PSRAM config can cause first-frame runtime failure.

Relevant files:

- `components/ui_service/monitor_dashboard_screen.c`
- `components/ui_service/assets/jnr_sb_font.ttf`
- `sdkconfig.defaults`

## Structural Smells

### Legacy UI public name

The implementation has moved to a monitor dashboard, but the public startup symbol remains `wifi_info_screen_start()`.

Impact:

- New agents may misread the current UI as a Wi-Fi-only screen.
- Documentation must explicitly state the legacy naming.

### Large config web implementation

`components/config_web_service/config_web_service.c` combines:

- embedded HTML,
- JSON serialization,
- form parsing,
- REST handlers,
- portal proxy,
- DNS / captive behavior,
- restart scheduling.

This is acceptable for bring-up but should be watched before adding more endpoint complexity.

### Provider abstraction is ahead of implementation

The public config model uses a provider enum and generic fields, but the concrete implementation is currently AQI-specific.

Impact:

- Future provider additions may need real parser abstraction rather than just new enum values.
- Documentation should not imply multiple provider implementations already exist.

## Testing Gaps

No automated tests currently cover:

- config validation,
- NVS migration behavior,
- provider JSON parsing,
- config web route behavior,
- HTTP proxy rewriting,
- dashboard formatting,
- button long-press behavior.

Build and on-device checks remain the real gates.

## Performance Concerns

Potential pressure points:

- LVGL full dashboard refresh and TinyTTF rendering.
- Provider HTTP buffers on an embedded heap.
- Config web portal proxy body limit.
- Hosted Wi-Fi buffer settings under poor link conditions.

Treat performance claims as unmeasured unless validated on board with real telemetry.

## Missing Product Capabilities

Current firmware has a provider dashboard and configuration portal, but still lacks:

- formal backend status model beyond the current provider snapshot,
- authenticated local portal,
- OTA strategy,
- persistent logs,
- alert history,
- multi-provider runtime selection UI,
- automated test harness.

These are product gaps, not immediate defects.
