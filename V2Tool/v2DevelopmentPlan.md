# PrimusV2 Development Plan

> Evolve the V1 costume lighting system (Processing → ESP32 via ArtNet) into a modular V2 system on ESP32-S3 + NeoPXL8 with configurable outputs, TFT status, button interaction, WebGUI device config, and eventually a web-based control platform.

---

## Stage 1: Short-Term — V2 Receiver + V1 Compatibility
**Goal**: Get physical LEDs working on the new V2 hardware using the existing Processing controller with minimal sender changes.

### Phase 1A: V2 Receiver Core (Arduino)
- [x] Create `V2Tool/Arduino/primusV2_receiver/` project structure
- [x] Port WiFi logic from V1 `costumeLighting_receiver.ino` to ESP32-S3
- [x] Replace `Adafruit_NeoPixel` with `Adafruit_NeoPXL8` using flat-array addressing (`strip * MAX_LEDS + pixel`)
- [x] Define `OutputConfig` struct: `type` (SHORT_STRIP 33px / LONG_STRIP 72px / GRID 4×8 32px / OFF), `colorOrder` (RGB / RGBW), `pxl8Port` (0-2)
- [x] Store config as array of 3 outputs; defaults match V1 layout for initial testing
- [x] Map each output to its own ArtNet universe (universe 0→port 0, 1→1, 2→2)
- [x] Per-universe byte layout: `[brightness][R][G][B]([W])...` — 3 bytes/pixel RGB, 4 bytes/pixel RGBW
- [x] Port V1 optimizations: brightness hysteresis, change detection, 16ms rate limiting, frame counting

### Phase 1B: TFT Status Display
- [x] Startup screen: device name + firmware version
- [x] Connection screen: WiFi SSID, IP address, RSSI signal strength
- [x] Running screen: per-output status (type, pixel count, universe, active/idle), FPS, brightness
- [x] Error screen: connection failures, reconnection attempts
- [x] Partial screen updates to avoid flicker

### Phase 1C: Button Interaction
- [x] D0 (active-LOW): Cycle through TFT info screens (connection / status / debug)
- [x] D1 (active-HIGH): Toggle built-in test mode (local animations without ArtNet)
- [x] D2 (active-HIGH): Brightness preset cycle (25% / 50% / 75% / 100%)
- [x] Show active button function on TFT

### Phase 1D: Minimal Processing Sender Updates
- [x] Update `ArtNetDevice.pde` to support sending across multiple universes (one per output)
- [x] Add device profile toggle in `Controls.pde`: V1 legacy (single universe) vs V2 (multi-universe)
- [x] Update default IP and output layout config

### Phase 1 Verification Checklist
- [ ] Flash V2 receiver, connect to WiFi, send grid animation to universe 0 → port 0 LEDs respond
- [ ] Send strip animation to universe 1 → port 1 LEDs respond
- [ ] TFT shows IP, connection status, FPS
- [ ] Buttons cycle screens and trigger test mode
- [ ] Test RGBW output on long strip if available

---

## Stage 2: Medium-Term — Standardized API + WebGUI Config
**Goal**: Create a documented ArtNet API any language can target, and add browser-based device configuration.

### Phase 2A: ArtNet API Specification
- [ ] Document protocol: universe-per-port mapping, byte layout, RGB vs RGBW handling
- [ ] Implement `ArtPoll`/`ArtPollReply` — devices respond to discovery with name, IP, port count, types, pixel counts
- [ ] Optional control universe (e.g., universe 15) for device-level commands

### Phase 2B: WebGUI Integration
- [ ] Integrate [WebGUI library](https://github.com/npuckett/WebGUI) into receiver firmware
- [ ] Config page at `http://<device-ip>/`: WiFi credentials, static IP/DHCP, device name
- [ ] Per-port output config: type (Short Strip / Long Strip / Grid / Off), color order (RGB/RGBW)
- [ ] ArtNet settings: starting universe, default brightness
- [ ] Persist config to ESP32-S3 NVS flash (survives reboots)
- [ ] `/api/info` JSON endpoint: device name, IP, firmware version, output configs, status

### Phase 2C: Firmware Reliability
- [ ] Replace WiFi polling with `WiFiEvent` callbacks
- [ ] Add mDNS (e.g., `primus-stage-left.local`)
- [ ] Per-output frame timeout: no data for 5s → fade to black
- [ ] AP fallback mode: create own AP if configured WiFi unavailable

### Phase 2D: Processing Multi-Device Support
- [ ] Refactor `ArtNetDevice` from single device to `ArrayList<ArtNetDevice>`
- [ ] Device management panel: add/remove devices, per-device output assignment
- [ ] (Stretch) ArtPoll listener for auto-discovery of V2 devices

### Phase 2 Verification
- [ ] WebGUI: configure outputs in browser → settings persist after reboot
- [ ] ArtPoll: standard ArtNet monitor tool finds device
- [ ] Multi-device: control 2+ V2 receivers simultaneously from Processing
- [ ] Timeout: disconnect controller → LEDs fade to black

---

## Stage 3: Long-Term — Web-Based Control Platform
**Goal**: Replace Processing with a browser-based tool for multi-device management, clip/scene editing, and operator show control.

### Phase 3A: Architecture
- [ ] **Frontend**: Browser UI (HTML/CSS/JS + Canvas for LED visualization)
- [ ] **Local relay server**: Node.js — bridges browser WebSocket commands to ArtNet UDP
- [ ] Relay also handles: ArtPoll discovery, device `/api/info` fetching, show file storage
- [ ] Flow: Browser ↔ WebSocket ↔ Node.js relay ↔ ArtNet UDP ↔ V2 devices

### Phase 3B: Core Web UI
- [ ] LED visualization canvas matching device output configs
- [ ] Per-output effect controls: dual color pickers, effect type, speed, playback mode
- [ ] Port 7 effects + color utilities from Processing to JavaScript
- [ ] Device panel: list discovered devices, connection status, output assignment
- [ ] Global controls: master brightness, blackout, FPS display

### Phase 3C: Clip Library & Scene Editor
- [ ] **Clip**: Saved effect settings for one output (JSON)
- [ ] **Look**: Saved state across all outputs on one device (collection of clips)
- [ ] **Scene**: Collection of looks across multiple devices (full stage state)
- [ ] **Cue list**: Ordered scenes with GO button, optional timed crossfade
- [ ] Clip editor with real-time LED preview
- [ ] Scene composer: drag-and-drop clips onto device outputs

### Phase 3D: Operator Interface
- [ ] Simplified show-control view: large GO button, current/next scene, manual overrides
- [ ] Keyboard shortcuts: spacebar = GO, escape = blackout

### Phase 3 Verification
- [ ] Relay + web UI: auto-discovery populates device list
- [ ] Clip creation → real-time preview matches physical LEDs
- [ ] Multi-device scene → step through cue list → all devices respond
- [ ] Operator triggers cues via keyboard, transitions work

---

## Key Architecture Decisions

| Decision | Rationale |
|----------|-----------|
| Universe-per-port mapping | Simplest, most ArtNet-standard; each output gets clean 512-byte universe |
| RGBW communicated via ArtPollReply / `/api/info` | Sender must know byte width; device advertises its config |
| Config header (Stage 1) → WebGUI + NVS (Stage 2) | Start simple, graduate to runtime config |
| Processing lives through Stage 2 | Incremental updates buy time for the web platform |
| Node.js for relay server | Best WebSocket ecosystem, single language with frontend |
| Output types as enum + struct | Adding new strip lengths/grid sizes is a config change |

## Output Type Reference

| Type | Pixel Count | Default Color Order | Notes |
|------|-------------|-------------------|-------|
| SHORT_STRIP | 33 | RGB | Short NeoPixel strip |
| LONG_STRIP | 72 | RGBW | Long NeoPixel strip |
| GRID | 32 (4×8) | RGB | LED matrix panel |
| OFF | 0 | — | Port disabled |

## Hardware Reference

| Component | Spec |
|-----------|------|
| MCU | Adafruit ESP32-S3 Reverse TFT Feather |
| LED Driver | NeoPXL8 Friend breakout (3.3V→5V level shift) |
| Screen | Built-in ST7789 240×135 TFT |
| Buttons | D0 (active-LOW), D1/D2 (active-HIGH) |
| Strip 0 | GPIO18 (A0) → NeoPXL8 Port 0 |
| Strip 1 | GPIO17 (A1) → NeoPXL8 Port 1 |
| Strip 2 | GPIO16 (A2) → NeoPXL8 Port 2 |
| Max LEDs per port | 72 (NeoPXL8 allocates uniformly) |

## File Map

### V1 Reference
- `V1Tool/Arduino/costumeLighting_receiver/costumeLighting_receiver.ino` — ArtNet receive, WiFi, optimizations
- `V1Tool/Arduino/costumeLighting_receiver/WifiSettings.h` — Network config
- `V1Tool/Processing/costumeLighting_controls/*.pde` — Processing controller (7 files)

### V2 Hardware Test
- `Arduino/hardwareTest/hardwareTest.ino` — NeoPXL8, TFT, buttons validation

### V2 Tool (created in Stage 1)
- `V2Tool/Arduino/primusV2_receiver/primusV2_receiver.ino` — Main receiver firmware
- `V2Tool/Arduino/primusV2_receiver/config.h` — Output types, network defaults
- `V2Tool/Arduino/primusV2_receiver/display.h` — TFT display functions
- `V2Tool/Arduino/primusV2_receiver/buttons.h` — Button handling

### Future (Stage 2+)
- `V2Tool/docs/artnet-api.md` — ArtNet protocol spec
- `V2Tool/web/` — Web-based control platform
