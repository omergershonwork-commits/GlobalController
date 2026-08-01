# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is being implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-002 LG power transmission

This milestone uses the Cardputer Adv's built-in infrared emitter to send one power command to an **LG 37LD450-ZA** television.

Current behavior:

- boots the Cardputer Adv through `M5Cardputer`
- initializes the built-in IR emitter on GPIO 44
- shows the configured TV model and instructions on the display
- sends the LG power signal when `P` is pressed
- reports the protocol, address, command, repeat count, and IR pin over serial
- preserves keyboard diagnostics for other keys
- compiles automatically in GitHub Actions with PlatformIO

The provisional power signal uses the structured Arduino-IRremote form:

- protocol: NEC
- address: `0x04`
- command: `0x08`
- repeats: `0`
- legacy MSB representation: `0x20DF10EF`

The command remains provisional until it is verified on the physical television.

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

If the device is not detected, switch it off, hold the `G0` button, connect USB-C, release `G0`, and retry the upload.

## Serial monitor

```bash
pio device monitor -b 115200
```

## TV-002 acceptance test

1. Flash the firmware.
2. Confirm that the display identifies `LG 37LD450-ZA` and shows `Status: Ready`.
3. Stand approximately 1 metre from the TV and point the Cardputer's IR edge toward the TV receiver.
4. Press `P` once.
5. Confirm that the display changes to `Status: Power IR sent`.
6. Confirm that the television toggles power.
7. Repeat the test 10 times from 1 to 3 metres and record how many attempts work.
8. Check the serial monitor for the NEC address and command diagnostics.

`Power IR sent` confirms that the firmware transmitted a signal; it does not independently prove that the television received it.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [ ] TV-002: built-in IR emitter and one verified LG TV power command
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
