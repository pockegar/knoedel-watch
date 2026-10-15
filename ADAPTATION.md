# Waveshare ESP32-S3-Touch-AMOLED-1.75C

This port runs the existing Knödel Watch application on the **1.75C** board:
466×466 AMOLED, touch gestures, German speech, motion input and power reporting.
M5Stack StopWatch remains the default build target. The two boards use separate
build directories and configurations.

The port was built with **ESP-IDF v6.0.1** and tested on one physical 1.75C board.
Display, swipes, taps, speech and switching cards during playback were confirmed
on the device. See [validation and remaining limits](docs/waveshare-validation.md).
Other Waveshare display sizes/models are not interchangeable with this board.

## Build

Activate an ESP-IDF v6.0.1 environment, then run from the repository root:

```sh
bash tools/build_ws.sh
```

This creates `build-waveshare/` and `sdkconfig.waveshare`, using
`sdkconfig.defaults` plus `sdkconfig.defaults.waveshare`. The build identifies
itself as `0.1.0-ws.1`; the default M5Stack build retains its original version.
An existing generated configuration is preserved, so use a separate build
folder/configuration when changing boards. Component versions are recorded in
`dependencies.lock`.

Build the content pack in a Python environment with Pillow, fonttools and
jsonschema, with CMake and SDL2 available:

```sh
python tools/import_decks.py --board waveshare --locale zh-Hans
```

The `--board waveshare` option checks both languages against the actual 466px
layout. This pack starts in Chinese and also includes English. Existing audio
files are reused when unchanged. See the main README for content/audio tooling.

## Flash a selected board

Use ESP-IDF's Python environment, including esptool 5.5 and pyserial. Supply the
intended board's USB serial / ROM MAC explicitly; there is no automatic fallback
to the first serial port. Replace the example address with your device's address:

```sh
export KNOEDEL_MAC=AA:BB:CC:DD:EE:FF
python tools/device_ws.py port
python tools/device_ws.py flash validation/my-install
```

Alternatively, pass `--mac AA:BB:CC:DD:EE:FF` to each command. The tool checks the
USB identity, ROM MAC, Waveshare build configuration, 32 MB Flash setting and
content-pack CRC before writing. It snapshots the exact binaries and records
SHA-256 hashes, offsets, flash output and the matching boot ELF digest in the
chosen output directory. Use a new output directory for each flash.

The normal update writes the bootloader, partition table, application and content;
it preserves NVS settings and learning progress. `--app-only` omits the content
pack. The partition layout still uses the first 16 MB of the 32 MB device, with a
3 MB application slot and a 13,248 KiB content partition.

To restore an earlier complete snapshot to the same device:

```sh
python tools/device_ws.py flash validation/restored --bundle validation/my-install
```

The snapshot must include all four binaries and `manifest.json`; their hashes,
offsets and device identity are checked again. This restores firmware/content,
not the historical NVS state. Keep any full-device backups separately.

The upstream web installer and `tools/make_release.py` target **M5Stack**.
Use this board-specific flashing path for Waveshare.

## Controls

| Input | Action |
| --- | --- |
| Swipe left / short PWR press | Next item; "knew it" when answering practice |
| Swipe right / short BOOT press | Previous item; "not yet" when answering practice |
| Tap | Enter, turn a card over, or change the selected setting |
| Hold BOOT | Back |

Language, volume, brightness and time are available under Settings. On USB power,
Power off returns to the application; on battery it requests AXP2101 shutdown.
The original motion features receive QMI8658 acceleration: lift to brighten,
face down to darken/start the timer, and shake on the clock to change the word.

## Board implementation

The facade in `main/board/waveshare_1_75c/` provides the M5 APIs used by the app.
M5Canvas still draws the UI. A PSRAM framebuffer feeds DMA stripes to the official
`esp_lcd_co5300` driver, which exclusively owns SPI2 and the panel protocol.
Each stripe waits for its completion callback before its memory is reused.

| Peripheral | Driver / wiring |
| --- | --- |
| CO5300 AMOLED | 466×466 RGB565, 40 MHz QSPI; CLK 38, D0–3 4/5/6/7, CS 12, reset 1; column gap 6 |
| Shared I²C | SDA 15, SCL 14 |
| CST9217 touch | Official component, reset 2 / interrupt 11; adapted to M5's touch state machine |
| ES8311 audio | 24 kHz mono; MCLK 16, BCLK 9, WS 45, DOUT 8, amplifier enable 46 |
| QMI8658 | 4g range, 125 Hz acceleration |
| AXP2101 | Battery/USB voltage, charge state, short PWR IRQ, ALDO1 audio power, battery shutdown |
| BOOT button | GPIO0 |

Panel commands and pins follow Waveshare BSP 3.0.0. Audio playback owns its PCM
buffer, resamples 22.05 kHz clips and cancels superseded playback when cards change.
Vendored M5 button/touch code keeps its copyright and MIT license; see
[LICENSES.md](LICENSES.md).

## Clock and diagnostics

There is no independent battery-backed RTC. The ESP32 system clock runs while
powered; NVS stores the last explicitly set time and the time at normal shutdown.
A complete power loss cannot account for elapsed time. The board defaults to
UTC+8 and needs manual or USB time correction after losing power. No Wi-Fi time
service or backend is added.

```sh
python tools/device_ws.py command status
python tools/device_ws.py command "time $(date +%s)"
python tools/device_ws.py screen /tmp/knoedel.ppm
```

Other USB commands are `tone`, `next`, `prev`, `tap` and `back`. Opening native USB
may reset the board on some hosts; keep a single connection for continuous
interaction tests. Frame capture retries incomplete transfers caused by logs
interleaving with the long text dump. Captures are framebuffer images, not photos
of the physical panel. Device-specific logs, backups and images stay untracked
under `validation/`, `backups/` and `release/`.
