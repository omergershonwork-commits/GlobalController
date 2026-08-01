# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-008 Xiaomi multi-profile support

This milestone keeps the physically verified **LG 37LD450-ZA** profile and adds a provisional infrared profile for the Xiaomi Android TV hardware identifier **MiTV-MSSP3**.

Press `T` to cycle between the installed television profiles:

```text
LG 37LD450-ZA
Xiaomi MiTV-MSSP3
```

The active television is displayed below the `GlobalController` heading.

## Xiaomi implementation

The Xiaomi profile uses a 20-bit Xiaomi RC-MM-style infrared encoder at 36 kHz. The encoder builds the device byte, function byte, four-bit checksum, and four-level symbol timings before sending the raw frame through GPIO 44.

Provisional Xiaomi commands:

- power
- volume up and down
- mute
- navigation up, down, left, and right
- OK
- back
- home
- input

Channel up and down are intentionally unavailable in the Xiaomi profile because they have not yet been identified confidently for this hardware family. Pressing `R` or `F` while Xiaomi is selected displays `UNAVAILABLE` and sends nothing.

All Xiaomi codes remain `Provisional` until tested on the physical MiTV-MSSP3 television. The screen therefore displays `TEST SIGNAL` in yellow after a Xiaomi command is transmitted.

## Keyboard layout

| Cardputer key | Action | Hold repeats |
|---|---|---:|
| `T` | Select next TV profile | No |
| `P` | Power | No |
| `M` | Mute | No |
| `U` / `J` | Volume up / down | Yes |
| `R` / `F` | Channel up / down | Yes when supported by profile |
| `W` / `A` / `S` / `D` | Navigate up / left / down / right | Yes |
| `Enter` or `O` | OK | No |
| `Delete` or `B` | Back | No |
| `H` | Home/menu | No |
| `I` | Input source | No |

Repeatable commands send immediately, wait 450 ms, and then repeat every 150 ms while the key remains held.

## Runtime flow

```text
Cardputer keyboard
    -> profile selection or KeyboardCommandMapper
    -> CommandBinding
    -> RemoteApplication
    -> active TV profile lookup
    -> IrTransmitter
    -> built-in IR emitter

RemoteEvent
    -> RemoteScreen
    -> Cardputer display
```

## Verification state

- All 14 LG commands and their hold behavior are physically verified on the user's LG 37LD450-ZA.
- The TV-006 coordinator and TV-007 dashboard regressions are physically verified.
- Xiaomi MiTV-MSSP3 commands are provisional pending hardware testing.

## Requirements

- M5Stack Cardputer Adv
- LG 37LD450-ZA and/or Xiaomi MiTV-MSSP3 television
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

## TV-008 acceptance test

1. Start with LG selected and confirm one previously verified LG command still works.
2. Press `T` and confirm the heading changes to `Xiaomi MiTV-MSSP3`.
3. Aim the Cardputer's IR edge toward the Xiaomi TV and test `P`.
4. Test `U`, `J`, `M`, `W`, `A`, `S`, `D`, `Enter`, `Delete`, `H`, and `I`.
5. For each Xiaomi command, record `worked`, `wrong action`, or `no reaction`.
6. Confirm `R` and `F` display `UNAVAILABLE` in Xiaomi mode and do not transmit.
7. Hold volume and navigation keys and confirm repeat behavior is usable.
8. Press `T` again and confirm the controller returns to the LG profile.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [x] TV-002: built-in IR emitter and one verified LG TV power command
- [x] TV-003: TV command and IR-code domain model
- [x] TV-004: first complete TV profile
- [x] TV-005: keyboard command mapping and hold/repeat behavior
- [x] TV-006: remote application coordinator
- [x] TV-007: main remote UI and feedback states
- [ ] TV-008: Xiaomi MiTV-MSSP3 profile and multi-TV selection
- [ ] TV-009: numeric channel entry and native tests

## Project configuration

- Espressif32 platform `6.7.0`
- `esp32-s3-devkitc-1` board definition
- Arduino framework
- USB CDC enabled at boot
- `M5Cardputer` pinned to release `1.1.1`
- Arduino-IRremote provided through the M5Cardputer dependency graph
