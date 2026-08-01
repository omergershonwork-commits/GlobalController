# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-005 keyboard controls

This milestone maps the Cardputer keyboard to the **LG 37LD450-ZA** profile and adds controlled hold-to-repeat behavior.

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

## Verification state

- Power is physically verified on the user's LG 37LD450-ZA.
- All other commands remain provisional until tested on that television.
- Verified commands produce a green status message.
- Provisional commands produce a yellow `test sent` status message.

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

## TV-005 acceptance test

Flash the branch and test the controls in this order:

1. `P`: power still toggles correctly.
2. `U` and `J`: volume changes once on a tap and repeatedly while held.
3. `M`: mute toggles once.
4. `I`: input/source menu opens.
5. `W`, `A`, `S`, `D`, and `Enter`: menu navigation works.
6. `Delete`: returns to the previous screen.
7. `R` and `F`: channel changes once on a tap and repeatedly while held.
8. `H`: home or menu behavior is identified.

Record each command as `worked`, `wrong action`, or `no reaction`. Do not mark provisional profile entries as verified until the physical result is known.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [x] TV-002: built-in IR emitter and one verified LG TV power command
- [x] TV-003: TV command and IR-code domain model
- [x] TV-004: first complete TV profile
- [ ] TV-005: keyboard command mapping and hold/repeat behavior
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
