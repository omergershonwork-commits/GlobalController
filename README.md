# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-008 Xiaomi multi-profile support

This milestone keeps the physically verified **LG 37LD450-ZA** profile and adds limited infrared support for the Xiaomi Android TV hardware identifier **MiTV-MSSP3**.

Press `T` to cycle between the installed television profiles:

```text
LG 37LD450-ZA
Xiaomi MiTV-MSSP3
```

The active television is displayed below the `GlobalController` heading.

## Xiaomi implementation

Physical testing showed that the MiTV-MSSP3 accepts the Xiaomi power command through infrared, but the normal navigation, volume, mute, home, back, OK, input, and channel commands do not react to infrared. These controls are therefore treated as Bluetooth or network controls rather than guessed IR codes.

The Xiaomi IR profile now contains only:

- power, sent as a 20-bit Xiaomi RC-MM-style frame at 36 kHz
- one additional complete frame to improve borderline reception

Every non-power key displays `UNAVAILABLE` in Xiaomi mode and sends no infrared signal.

The Xiaomi power command remains `Provisional` until repeated physical testing confirms reliable operation without double toggling. The screen therefore displays `TEST SIGNAL` in yellow after transmission.

Full Xiaomi control is planned as a separate transport using the Android TV Remote protocol over Wi-Fi or a Bluetooth-compatible implementation. Voice control is outside the current IR milestone.

## Keyboard layout

| Cardputer key | LG action | Xiaomi IR action |
|---|---|---|
| `T` | Select next TV profile | Select next TV profile |
| `P` | Power | Power test signal |
| `M` | Mute | Unavailable over IR |
| `U` / `J` | Volume up / down | Unavailable over IR |
| `R` / `F` | Channel up / down | Unavailable over IR |
| `W` / `A` / `S` / `D` | Navigation | Unavailable over IR |
| `Enter` or `O` | OK | Unavailable over IR |
| `Delete` or `B` | Back | Unavailable over IR |
| `H` | Home/menu | Unavailable over IR |
| `I` | Input source | Unavailable over IR |

LG repeatable commands send immediately, wait 450 ms, and then repeat every 150 ms while the key remains held.

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
- Xiaomi MiTV-MSSP3 power reacted through infrared but was inconsistent with one frame.
- Xiaomi non-power commands did not react through infrared and were removed from the profile.
- The revised two-frame Xiaomi power signal is pending hardware verification.

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

## Revised TV-008 acceptance test

1. Start with LG selected and confirm one previously verified LG command still works.
2. Press `T` and confirm the heading changes to `Xiaomi MiTV-MSSP3`.
3. Aim directly at the Xiaomi receiver and press `P` ten times, waiting for each power transition to finish.
4. Record the number of successful power actions out of ten.
5. Confirm that one press never produces two power transitions.
6. Press a non-power key and confirm the display shows `UNAVAILABLE` and the TV does not react.
7. Press `T` again and confirm the controller returns to the LG profile.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [x] TV-002: built-in IR emitter and one verified LG TV power command
- [x] TV-003: TV command and IR-code domain model
- [x] TV-004: first complete TV profile
- [x] TV-005: keyboard command mapping and hold/repeat behavior
- [x] TV-006: remote application coordinator
- [x] TV-007: main remote UI and feedback states
- [ ] TV-008: Xiaomi MiTV-MSSP3 profile selection and reliable IR power
- [ ] TV-009: Android TV Wi-Fi remote transport for Xiaomi controls
- [ ] TV-010: numeric channel entry and native tests

## Project configuration

- Espressif32 platform `6.7.0`
- `esp32-s3-devkitc-1` board definition
- Arduino framework
- USB CDC enabled at boot
- `M5Cardputer` pinned to release `1.1.1`
- Arduino-IRremote provided through the M5Cardputer dependency graph
