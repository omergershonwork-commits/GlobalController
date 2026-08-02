# GlobalController

Portable universal-controller firmware for the **M5Stack Cardputer Adv**.

The project is implemented incrementally. Every implementation step is developed on a dedicated branch and delivered through a pull request.

## Current milestone: TV-009 Xiaomi hybrid remote

GlobalController supports two television profiles:

```text
LG 37LD450-ZA
Xiaomi MiTV-MSSP3
```

Press `T` to switch profiles.

The LG profile remains fully infrared. The Xiaomi profile is hybrid:

```text
Power
    -> built-in infrared emitter

Volume, mute, channels, navigation, OK, Back, Home, and Input
    -> Wi-Fi
    -> Android TV Remote Service
```

The Wi-Fi controls use Android TV Remote protocol v2. They do not require ADB or Android developer mode, but they do require the Cardputer and television to be connected to the same local network.

## Local network configuration

Wi-Fi credentials are not stored in Git.

Copy the template:

```powershell
Copy-Item include/local_config.example.h include/local_config.h
```

Edit `include/local_config.h` locally:

```cpp
#pragma once

#define GC_WIFI_SSID "YOUR_WIFI_NAME"
#define GC_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

`include/local_config.h` is ignored by Git. Do not commit it.

The Xiaomi television IP address is not configured manually. After Wi-Fi connects, the Cardputer searches the local network for the Android TV Remote v2 mDNS service:

```text
_androidtvremote2._tcp.local
```

The discovered IPv4 address and service port are used at runtime. A router-assigned IP change therefore does not require rebuilding or editing the firmware.

When local Wi-Fi configuration is missing, the firmware still builds and all LG controls plus Xiaomi infrared power remain available. Xiaomi non-power commands display `WIFI SETUP`.

## First Xiaomi pairing

1. Build and upload the firmware with `include/local_config.h` present.
2. Turn the Xiaomi TV on and keep it connected to the same network as the Cardputer.
3. Press `T` until `Xiaomi MiTV-MSSP3` is selected.
4. Wait while the display shows `WIFI CONNECT`, `TV SEARCH`, and `TV CONNECT`.
5. The TV should display a six-character hexadecimal pairing code.
6. Type that code on the Cardputer keyboard.
7. Use `Delete` to correct the last character when needed.
8. Press `Enter` after all six characters are entered.
9. Wait for `WIFI READY`.

During pairing, normal remote commands are temporarily disabled so typed code characters cannot accidentally control the television.

## Keyboard layout

| Cardputer key | TV action | LG route | Xiaomi route |
|---|---|---|---|
| `T` | Select next TV profile | Local UI | Local UI |
| `P` | Power | IR | IR fallback |
| `M` | Mute | IR | Wi-Fi |
| `U` / `J` | Volume up / down | IR | Wi-Fi |
| `R` / `F` | Channel up / down | IR | Wi-Fi |
| `W` / `A` / `S` / `D` | Navigation | IR | Wi-Fi |
| `Enter` or `O` | OK | IR | Wi-Fi |
| `Delete` or `B` | Back | IR | Wi-Fi |
| `H` | Home/menu | IR | Wi-Fi |
| `I` | Input source | IR | Wi-Fi |

Repeatable commands send immediately, wait 450 ms, and then repeat every 150 ms while the key remains held.

## Runtime architecture

```text
Cardputer keyboard
    -> KeyboardCommandMapper
    -> RemoteApplication
       - shared first-press and hold-repeat timing
    -> HybridTvCommandSender
       - active profile chooses IR or Wi-Fi

IR route
    -> TvProfile IR lookup
    -> ArduinoIrTransmitter
    -> built-in emitter on GPIO 44

Wi-Fi route
    -> AndroidTvRemoteAdapter
    -> Wi-Fi connection
    -> mDNS discovery of _androidtvremote2._tcp
    -> TLS pairing / Android TV Remote Service
    -> Xiaomi television

RemoteEvent
    -> RemoteScreen
    -> Cardputer display
```

## Status messages

| Display state | Meaning |
|---|---|
| `WIFI SETUP` | Local Wi-Fi credentials are missing |
| `WIFI CONNECT` | Cardputer is joining the configured network |
| `TV SEARCH` | Searching the local network for Android TV Remote v2 |
| `TV CONNECT` | Opening the discovered Android TV remote connection |
| `PAIRING` | Pairing handshake is running |
| `PAIR CODE` | Type the six-character code displayed by the TV |
| `WIFI READY` | Xiaomi network controls are available |
| `WIFI SENT` | A command was sent over Wi-Fi |
| `WIFI REPEAT` | A held command was repeated over Wi-Fi |
| `WIFI WAIT` | Command was requested before pairing/connection completed |
| `WIFI ERROR` | Pairing or network transport failed; inspect serial output |

## Verification state

- All 14 LG commands and hold behavior are physically verified on the user's LG 37LD450-ZA.
- Xiaomi infrared power reacted physically but remains under reliability testing.
- Xiaomi non-power infrared attempts did not work and were removed.
- Xiaomi Wi-Fi transport and automatic mDNS discovery are implemented and require physical pairing and command verification.

## Requirements

- M5Stack Cardputer Adv
- LG 37LD450-ZA and/or Xiaomi MiTV-MSSP3 television
- 2.4 GHz-compatible Wi-Fi network for the Cardputer
- Xiaomi TV and Cardputer on the same local network
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

## TV-009 acceptance test

1. Confirm one LG command still works.
2. Select Xiaomi and confirm infrared power still reacts.
3. Confirm the Cardputer connects to the configured Wi-Fi network.
4. Confirm `TV SEARCH` finds an Android TV service without a configured IP address.
5. Complete the six-character TV pairing flow.
6. Confirm `WIFI READY` appears.
7. Test volume up/down and mute.
8. Test `W`, `A`, `S`, `D`, OK, Back, and Home.
9. Test channel up/down and Input; record unsupported actions separately because Android TV models may handle these keys differently.
10. Hold volume and navigation keys and verify repeat behavior.
11. Restart the router or allow the TV address to change, then confirm discovery reconnects without editing firmware.
12. Switch back to LG and confirm its IR controls remain unchanged.

## Planned implementation sequence

- [x] TV-001: PlatformIO project, display, keyboard smoke test, and CI build
- [x] TV-002: built-in IR emitter and one verified LG TV power command
- [x] TV-003: TV command and IR-code domain model
- [x] TV-004: first complete TV profile
- [x] TV-005: keyboard command mapping and hold/repeat behavior
- [x] TV-006: remote application coordinator
- [x] TV-007: main remote UI and feedback states
- [ ] TV-008: Xiaomi MiTV-MSSP3 profile selection and reliable IR power
- [ ] TV-009: Xiaomi Android TV Wi-Fi pairing, discovery, and remote controls
- [ ] TV-010: numeric channel entry and native tests

## Project configuration

- Espressif32 platform `6.7.0`
- `esp32-s3-devkitc-1` board definition
- Arduino framework
- USB CDC enabled at boot
- `M5Cardputer` pinned to release `1.1.1`
- Arduino-IRremote provided through the M5Cardputer dependency graph
- Android TV Remote protocol reference pinned to an exact Git commit
- Arduino wolfSSL and Crypto dependencies pinned through PlatformIO
