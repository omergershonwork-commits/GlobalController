# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is being implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-004 LG television profile

This milestone moves model-specific infrared codes into a reusable profile for the **LG 37LD450-ZA**.

Current behavior:

- boots the Cardputer Adv through `M5Cardputer`
- initializes the built-in IR emitter on GPIO 44
- loads an `Lg37Ld450Profile`
- resolves `TvCommand::Power` through the profile
- sends the same physically verified power signal when `P` is pressed
- records whether each profile entry is verified or provisional
- compiles automatically in GitHub Actions with PlatformIO

The profile contains these logical commands:

- power
- volume up and down
- mute
- channel up and down
- navigation up, down, left, and right
- OK
- back
- home
- input

Only the power command has been physically verified on this television. The remaining commands are common LG NEC codes and remain provisional until tested in TV-005.

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

Connect the Cardputer Adv by USB-C and run:

```bash
pio run -e m5stack-cardputer-adv -t upload
```

This project uses the ESP32-S3 ROM loader workaround because the temporary esptool upload stub was unreliable on the tested Cardputer connection.

If the device is not detected, switch it off, hold the `G0` button, connect USB-C, release `G0`, and retry the upload.

## Serial monitor

```bash
pio device monitor -b 115200
```

## TV-004 acceptance test

1. Flash the firmware.
2. Confirm that the display identifies `LG 37LD450-ZA` and shows `Status: Ready`.
3. Press `P` once.
4. Confirm that the display changes to `Status: Power IR sent`.
5. Confirm that the television toggles power exactly as it did in TV-003.
6. Optionally check serial output for `Profile code status: verified`.

TV-004 intentionally does not expose the provisional commands through the keyboard yet.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [x] TV-002: built-in IR emitter and one verified LG TV power command
- [x] TV-003: TV command and IR-code domain model
- [ ] TV-004: first complete TV profile
- [ ] TV-005: keyboard command mapping and hold/repeat behavior
- [ ] TV-006: remote application coordinator
- [ ] TV-007: main remote UI and feedback states
- [ ] TV-008: numeric channel entry and native tests

## Project configuration

The PlatformIO environment follows M5Stack's Cardputer Adv configuration:

- Espressif32 platform `6.7.0`
- `esp32-s3-devkitc-1` board definition
- Arduino framework
- USB CDC enabled at boot
- `M5Cardputer` pinned to release `1.1.1`
- `Arduino-IRremote` resolved through the M5Cardputer dependency graph
