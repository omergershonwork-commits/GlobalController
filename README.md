# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-006 application coordinator

This milestone moves remote-control behavior out of `main.cpp` and into a reusable `RemoteApplication` coordinator.

The runtime flow is now:

```text
Cardputer keyboard
    -> KeyboardCommandMapper
    -> CommandBinding
    -> RemoteApplication
    -> TV profile lookup
    -> IrTransmitter
    -> built-in IR emitter
```

The coordinator owns:

- command/profile resolution
- IR transmission requests
- unavailable and unsupported-command results
- initial press behavior
- hold-to-repeat timing and state
- verified/provisional metadata in returned events

`main.cpp` now focuses on hardware polling, serial diagnostics, and display rendering.

## Keyboard layout

| Cardputer key | TV action | Hold repeats |
|---|---|---:|
| `P` | Power | No |
| `M` | Mute | No |
| `U` / `J` | Volume up / down | Yes |
| `R` / `F` | Channel up / down | Yes |
| `W` / `A` / `S` / `D` | Navigate up / left / down / right | Yes |
| `Enter` or `O` | OK | No |
| `Delete` or `B` | Back | No |
| `H` | Home/menu | No |
| `I` | Input source | No |

Repeatable commands send immediately, wait 450 ms, and then repeat every 150 ms while the key remains held.

All 14 commands were physically verified on the user's LG 37LD450-ZA during TV-005.

## Requirements

- M5Stack Cardputer Adv
- LG 37LD450-ZA television
- USB-C data cable
- PlatformIO Core 6.1.19 or the PlatformIO IDE extension

## Build

```bash
pio run -e m5stack-cardputer-adv
```

## Upload

```bash
pio run -e m5stack-cardputer-adv -t upload
```

The project uses the ESP32-S3 ROM loader workaround because the temporary esptool upload stub was unreliable on the tested Cardputer connection.

When required, enter download mode by switching the Cardputer off, holding `G0`, connecting USB-C, and then releasing `G0`.

## Serial monitor

```bash
pio device monitor -b 115200
```

## TV-006 acceptance test

This step is an internal refactor and should preserve TV-005 behavior:

1. Confirm `P` still toggles power once per press.
2. Confirm `U` and `J` work on taps and repeat while held.
3. Confirm one additional single-send command such as `M` or `I`.
4. Confirm a navigation key repeats while held in a TV menu.
5. Confirm the display and serial monitor still report the command result.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [x] TV-002: built-in IR emitter and one verified LG TV power command
- [x] TV-003: TV command and IR-code domain model
- [x] TV-004: first complete TV profile
- [x] TV-005: keyboard command mapping and hold/repeat behavior
- [ ] TV-006: remote application coordinator
- [ ] TV-007: main remote UI and feedback states
- [ ] TV-008: numeric channel entry and native tests

## Project configuration

- Espressif32 platform `6.7.0`
- `esp32-s3-devkitc-1` board definition
- Arduino framework
- USB CDC enabled at boot
- `M5Cardputer` pinned to release `1.1.1`
- Arduino-IRremote provided through the M5Cardputer dependency graph
