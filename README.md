# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-007 remote interface

This milestone adds a dedicated `RemoteScreen` presentation layer and replaces the development-oriented text screen with a stable remote-control dashboard.

The display now contains:

- the active television brand and model
- the complete keyboard control map
- a dedicated status area that updates without redrawing the full screen
- distinct feedback for ready, command sent, hold repeat, unmapped input, unavailable commands, and IR protocol errors
- automatic return to `READY` when the pressed key is released

The runtime flow remains:

```text
Cardputer keyboard
    -> KeyboardCommandMapper
    -> CommandBinding
    -> RemoteApplication
    -> TV profile lookup
    -> IrTransmitter
    -> built-in IR emitter

RemoteEvent
    -> RemoteScreen
    -> Cardputer display
```

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

All 14 commands were physically verified on the user's LG 37LD450-ZA during TV-005. TV-006 verified that the coordinator refactor preserved the same behavior.

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

## TV-007 acceptance test

1. Confirm the dashboard fits on screen and all four control rows are readable.
2. Confirm the bottom status area initially displays `READY`.
3. Press `P` and confirm the status changes to `COMMAND SENT` and `Power`.
4. Hold `U` or `J` and confirm the status changes to `HOLD REPEAT` during repeated transmissions.
5. Release the key and confirm the status returns to `READY`.
6. Press an unmapped key such as `Q` and confirm `UNMAPPED KEY` appears.
7. Confirm power, one volume command, and one navigation command still control the television correctly.
8. Confirm repeated commands update only the bottom status area without obvious full-screen flashing.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [x] TV-002: built-in IR emitter and one verified LG TV power command
- [x] TV-003: TV command and IR-code domain model
- [x] TV-004: first complete TV profile
- [x] TV-005: keyboard command mapping and hold/repeat behavior
- [x] TV-006: remote application coordinator
- [ ] TV-007: main remote UI and feedback states
- [ ] TV-008: numeric channel entry and native tests

## Project configuration

- Espressif32 platform `6.7.0`
- `esp32-s3-devkitc-1` board definition
- Arduino framework
- USB CDC enabled at boot
- `M5Cardputer` pinned to release `1.1.1`
- Arduino-IRremote provided through the M5Cardputer dependency graph
