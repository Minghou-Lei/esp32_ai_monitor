# AI Agent Monitor Proposal

## Current Project State

- Repository is a minimal ESP-IDF project.
- Build target is `esp32p4`.
- `main/main.c` contains an empty `app_main`.
- No `.planning/` directory exists yet.

## GSD Route

Based on the current repository state, the next GSD step is:

1. Run `$gsd-new-project`
2. Then run `$gsd-plan-phase 1`
3. Then run `$gsd-execute-phase 1`

Reason:

- `$gsd-next` routes repos without `.planning/` to `$gsd-new-project`.
- This project is not ready for direct implementation work under the GSD workflow yet.

## Board Facts That Matter

Board: Waveshare `ESP32-P4-WIFI6-Touch-LCD-4B`

- Recommended framework: ESP-IDF, version `5.3.1+`
- Display: `4-inch`, `720x720`, touch, `MIPI-DSI`
- Wireless: `ESP32-C6` coprocessor for `Wi-Fi 6` and `Bluetooth 5`
- Multimedia: microphone, speaker, optional camera via `MIPI-CSI`
- Storage and IO: `Micro SD`, `USB OTG`, `Ethernet`
- Vendor examples exist for:
  - LCD bring-up
  - LVGL HMI
  - Camera-to-LCD
  - MP4 playback
  - ESP-Phone style UI

## Recommended Product Positioning

Recommended first version:

- The board is a dedicated monitoring and control terminal for an AI Agent that runs elsewhere.
- The AI Agent itself should run on a PC, local server, NAS, or cloud backend.
- The ESP32-P4 board should handle:
  - network connectivity
  - local touch UI
  - health/status display
  - event and log display
  - simple remote control actions

Not recommended for V1:

- full local LLM inference on the board
- complex on-device multimodal reasoning
- trying to make the board both the agent runtime and the rich UI frontend

Reason:

- ESP32-P4 is strong for HMI, connectivity, camera/display pipelines, and edge orchestration.
- It is not the right place for serious LLM execution.

## Recommended V1 Scope

### UI

- Full-screen dashboard in LVGL
- Status header: online/offline, backend latency, Wi-Fi/Ethernet mode
- Agent card: current state, active task, last heartbeat
- Queue card: pending jobs, running jobs, failed jobs
- Logs panel: latest events, warnings, failures
- Action bar: reconnect, acknowledge alert, restart session, open details

### Data

- Agent heartbeat
- Current task metadata
- Last error and error count
- CPU or runtime load from the backend
- Memory usage from the backend
- Token or request counters if available
- Network quality

### Connectivity

Preferred order:

1. HTTP polling for V1 bring-up
2. WebSocket for live updates in V2
3. MQTT only if the backend already uses it

Reason:

- HTTP polling is simplest to debug on embedded targets.
- WebSocket is better after the data model is stable.

## Proposed Firmware Architecture

## Modules

- `app_main`
  - boot flow
  - task startup
- `display_service`
  - LCD init
  - LVGL tick and render loop
- `touch_service`
  - touch input adapter
- `network_service`
  - Wi-Fi or Ethernet setup
  - reconnect logic
- `backend_client`
  - REST polling first
  - JSON decode
- `agent_state_store`
  - shared latest monitor model
- `ui_screens`
  - dashboard
  - logs
  - details
  - settings
- `actions_service`
  - send remote commands to backend
- `settings_store`
  - saved endpoint, auth token, refresh interval

## Phase Proposal

### Phase 1

Bring up board UI foundation

- add board display and touch dependencies
- light the LCD
- render LVGL dashboard shell
- prove stable frame refresh

### Phase 2

Bring up connectivity and backend contract

- connect via Wi-Fi first
- define monitor JSON schema
- fetch mock or real backend status
- show heartbeat and connection state

### Phase 3

Build usable monitoring dashboard

- task state
- event log
- alert state
- manual refresh and auto refresh

### Phase 4

Control actions and resilience

- restart agent or workflow
- acknowledge alarms
- retry failed job
- offline fallback states

### Phase 5

Optional multimodal expansion

- camera preview
- microphone path
- voice trigger
- richer device-to-agent interaction

## First Technical Decision To Approve

Recommended default:

- backend-hosted AI Agent
- ESP32 board as remote monitor terminal
- LVGL UI
- Wi-Fi first
- HTTP polling first

If approved, the next concrete workflow is:

1. initialize `.planning/` with `$gsd-new-project`
2. make Phase 1 about LCD, touch, LVGL, and mock monitor data
3. then implement board bring-up in code

## Open Questions

These answers decide the roadmap:

1. Is this board only a monitor/control terminal, or must it also capture voice/camera input for the agent?
2. Where does the AI Agent run now: PC, local server, NAS, cloud, or not built yet?
3. Do you want V1 to use Wi-Fi only, or must Ethernet also be supported from the start?
