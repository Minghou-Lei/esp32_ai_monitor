# Codebase Concerns

**Analysis Date:** 2026-05-17

## Tech Debt

**UI naming drift:**
- Issue: the public UI API still uses `wifi_info_screen_*` naming while the implementation has become an AQI-style dashboard
- Files: `components/ui_service/include/wifi_info_screen.h`, `components/ui_service/monitor_dashboard_screen.c`, `main/main.c`
- Impact: new contributors can misread what screen is actually booted and may wire future pages into the wrong abstraction
- Fix approach: perform a coordinated API rename only when all call sites, docs, and any future additional screens can move together

**Single-file embedded config portal:**
- Issue: HTML, CSS, JS, request parsing, validation flow, and route binding all live in `components/config_web_service/config_web_service.c`
- Files: `components/config_web_service/config_web_service.c`
- Impact: every portal change increases merge risk and review burden inside one large source file
- Fix approach: split rendering helpers, payload serializers, and route handlers into smaller compilation units when the portal grows further

## Known Bugs

**Potential component-dependency knot:**
- Symptoms: `network_service` and `provider_service` are mutually required at the ESP-IDF component level
- Files: `components/network_service/CMakeLists.txt`, `components/provider_service/CMakeLists.txt`
- Trigger: adding more shared runtime APIs or trying to extract these services independently
- Workaround: keep shared cross-service contracts minimal until the dependency is refactored into a thinner common layer

## Security Considerations

**Sensitive runtime config is exposed over the local config API:**
- Risk: the config JSON includes Wi-Fi credentials, enterprise fields, portal credentials, provider token, and management key
- Files: `components/config_web_service/config_web_service.c`, `components/app_config_service/include/app_config_service.h`
- Current mitigation: no secrets are committed to docs by default; runtime edits are centralized through validation
- Recommendations: reduce readback of secret fields or mask them in the HTTP response before treating the portal as production-ready

**Secrets can leak into machine-state config and logs:**
- Risk: `sdkconfig` and runtime screenshots/log captures can accidentally preserve machine-local credentials
- Files: `sdkconfig`, `components/app_config_service/Kconfig.projbuild`, `docs/*.md`
- Current mitigation: committed baseline is `sdkconfig.defaults`, not `sdkconfig`
- Recommendations: keep doc examples sanitized, avoid committing machine-effective `sdkconfig` diffs blindly, and review any generated screenshots/log excerpts before commit

## Performance Bottlenecks

**UI memory and refresh pressure:**
- Problem: the dashboard combines LVGL, TinyTTF, large text widgets, and periodic updates
- Files: `components/ui_service/monitor_dashboard_screen.c`, `sdkconfig.defaults`
- Cause: font rendering and repeated label updates can push memory and render cost on an embedded target
- Improvement path: keep refreshes incremental, re-evaluate font sizes/assets carefully, and validate memory behavior on hardware after UI changes

**Provider fetch path is synchronous per refresh cycle:**
- Problem: each refresh performs a blocking HTTP request and lightweight manual parsing in the poll task
- Files: `components/provider_service/provider_service.c`
- Cause: single-task polling is simple but serializes network latency directly into the provider update cadence
- Improvement path: keep the cadence modest, improve failure backoff if needed, and only optimize after real-device evidence shows missed responsiveness

## Fragile Areas

**Hosted Wi-Fi integration path:**
- Files: `sdkconfig.defaults`, `main/idf_component.yml`, `components/network_service/network_service.c`
- Why fragile: the board depends on `ESP-Hosted + esp_wifi_remote` rather than a plain native Wi-Fi mental model
- Safe modification: change hosted/remote settings deliberately and always re-run `reconfigure`, `build`, and on-device validation
- Test coverage: no automated coverage; hardware bring-up remains the guardrail

**Runtime config schema and portal coupling:**
- Files: `components/app_config_service/include/app_config_service.h`, `components/app_config_service/app_config_service.c`, `components/config_web_service/config_web_service.c`
- Why fragile: adding a field touches Kconfig defaults, config assembly, validation, JSON serialization, form parsing, and downstream consumers
- Safe modification: treat config-model edits as cross-component work and verify read/save/reset behavior end to end
- Test coverage: no automated persistence or schema migration tests

## Scaling Limits

**Provider item model:**
- Current capacity: `PROVIDER_SERVICE_MAX_ITEMS` is fixed at `2`
- Limit: larger provider payloads will not be represented fully by the current snapshot model
- Scaling path: widen the snapshot model only after the UI and parsing contract are redesigned to consume more than the current top items

**History window:**
- Current capacity: `PROVIDER_SERVICE_HISTORY_CAPACITY` is `80`
- Limit: long-range trend tracking is intentionally shallow
- Scaling path: move to a more explicit history/persistence strategy instead of only increasing the static ring

## Dependencies at Risk

**Hosted + remote component stack:**
- Risk: this path is more configuration-sensitive than a plain Wi-Fi example project
- Impact: misaligned versions or config toggles can break networking even if business code is unchanged
- Migration plan: stay aligned with verified ESP-IDF / component versions and only broaden compatibility with explicit bring-up validation

## Missing Critical Features

**Automated verification harness:**
- Problem: there is no unit-test, integration-test, or CI layer to catch regressions before flashing hardware
- Blocks: safe iteration on config schema, provider parsing, hosted Wi-Fi behavior, and UI evolution

**Auth hardening for local config portal:**
- Problem: the current local portal focuses on bring-up convenience, not operator authentication/authorization
- Blocks: confidently exposing the portal outside tightly controlled local-network scenarios

## Test Coverage Gaps

**Runtime config persistence path:**
- What's not tested: save/reset/validate behavior across all config fields
- Files: `components/app_config_service/app_config_service.c`
- Risk: schema growth can silently break persistence or field bounds
- Priority: High

**Provider response parsing and error mapping:**
- What's not tested: malformed JSON, partial payloads, and alternate provider responses
- Files: `components/provider_service/provider_service.c`
- Risk: production payload changes can degrade status text or metrics silently
- Priority: High

**Board-local portal contract:**
- What's not tested: HTTP route responses and form-to-config translation
- Files: `components/config_web_service/config_web_service.c`
- Risk: UI and config API can drift or expose invalid config writes unnoticed
- Priority: High

---

*Concerns audit: 2026-05-17*
