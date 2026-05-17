# AI Agent Monitor Proposal

## Current Codebase Reality

This repository is no longer a blank `ESP-IDF` scaffold.

Current working-tree facts:

- The project target is `esp32p4`.
- `main/main.c` boots a real UI component and a real Wi-Fi service component.
- `components/network_service` already manages Wi-Fi station lifecycle and state snapshots.
- `components/ui_service` already renders a board-local `LVGL` Wi-Fi information page using the official Waveshare `BSP`.
- `sdkconfig.defaults` already encodes a `32MB` flash baseline, `PSRAM`, the `ESP-Hosted + esp_wifi_remote` route, and `CONFIG_LV_USE_CLIB_MALLOC=y`.
- `main/idf_component.yml` now points several board dependencies at project-local override components.

So the repository has moved past “empty project planning” and is now in an early bring-up implementation phase.

## Product Positioning

The recommended product positioning still holds:

- The board is a dedicated monitoring and control terminal for an AI Agent running elsewhere.
- The AI Agent itself should run on a PC, local server, NAS, or cloud backend.
- The board should focus on:
  - network connectivity
  - local touch UI
  - status display
  - event display
  - lightweight remote control entry points

Not recommended for the current product direction:

- full on-device LLM inference
- trying to make the ESP32-P4 board both the heavy AI runtime and the polished operator console

## What Exists Today

### Board / UI bring-up

Already implemented in the current codebase:

- display startup through `waveshare/esp32_p4_wifi6_touch_lcd_4b`
- backlight enable
- `LVGL` object tree creation
- embedded `TinyTTF` font loading
- periodic screen refresh
- a richer board-style Wi-Fi diagnostics layout in the current working tree

### Wi-Fi state pipeline

Already implemented:

- Wi-Fi station initialization
- event-driven state transitions
- IP / DNS / MAC / RSSI / channel snapshot collection
- screen-side rendering of the snapshot

### Build / toolchain posture

Current confirmed facts:

- the workspace points at `ESP-IDF v6.0.1`
- the current session can read `ESP-IDF MCP` resources such as `project://config`
- `project://status` timed out in this session, so MCP should be treated as available but timeout-sensitive rather than unavailable
- the project already has recent build artifacts under `build/`

So the environment constraint is no longer “SDK unusable”; the more accurate constraint is “MCP and build validation must distinguish timeout from actual failure.”

### Missing product layers

Still missing:

- backend polling
- AI agent heartbeat model
- alert feed
- action / control flows
- a multi-page monitor dashboard

## Recommended Firmware Architecture

### Keep

- thin `app_main`
- separate `network_service`
- separate `ui_service`
- official board `BSP`
- hosted Wi-Fi configuration as the baseline network route

### Add next

- `backend_client`
- `agent_state`
- `settings_store`
- optional `touch_service` split if interaction complexity grows

## Constraints That Matter Now

### Hosted Wi-Fi route

The board should still be understood as:

- `ESP32-P4` for UI, display, touch, and application logic
- onboard `ESP32-C6` for `Wi-Fi 6 / BLE`

That means networking work should continue from the `ESP-Hosted + esp_wifi_remote` baseline, not from a “plain local `esp_wifi` board” assumption.

### UI memory and first-frame stability

The UI path now depends on:

- `PSRAM`
- `TinyTTF`
- `LVGL` heap behavior
- avoiding redundant large-font relayout during refresh

So any future dashboard work should treat font, allocator, and refresh behavior as a first-class stability concern rather than as a cosmetic afterthought.

### Local override components

The project now carries local overrides for several board-related components. That is acceptable for the current `ESP-IDF v6.0.1` compatibility posture, but it introduces a maintenance constraint:

- keep the overrides minimal
- track upstream shape closely
- avoid turning the repo into a permanent private fork of board support code

## Recommended Next Milestone

1. Keep the existing Wi-Fi page as a diagnostics screen.
2. Add a minimal `backend_client` using `HTTP polling`.
3. Introduce a compact monitor state model:
  - backend online / offline
  - last heartbeat
  - active task
  - last error
  - latency
4. Add a simple overview screen above the current Wi-Fi diagnostics page.

## V1 UI Scope

Recommended pages:

- overview page
- Wi-Fi diagnostics page
- alerts / events page
- basic settings page

The current `wifi_info_screen` should evolve into a support page, not remain the final home screen.
