# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is being implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-001 bootstrap

This first milestone establishes the firmware toolchain and verifies the Cardputer display and keyboard before adding infrared transmission.

Current behavior:

- boots the Cardputer Adv through `M5Cardputer`
- renders a basic GlobalController status screen
- captures keyboard state changes
- displays the latest printable, Enter, Delete, or special-key event
- writes the same input event to the serial monitor
- compiles automatically in GitHub Actions with PlatformIO

## Requirements

- M5Stack Cardputer Adv
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

If the device is not detected, switch it off, hold the `G0` button, connect USB-C, release `G0`, and retry the upload.

## Serial monitor

```bash
pio device monitor -b 115200
```

## Initial acceptance test

1. Flash the firmware.
2. Confirm that the display shows `GlobalController` and `TV remote bootstrap`.
3. Press printable keys and verify that the latest input appears on the display.
4. Press Enter and Delete and verify their labels.
5. Confirm that the same events appear in the serial monitor.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [ ] TV-002: built-in IR emitter and one verified TV power command
- [ ] TV-003: TV command and IR-code domain model
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
