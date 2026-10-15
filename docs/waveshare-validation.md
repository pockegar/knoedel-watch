# Waveshare 1.75C validation

Hardware acceptance on **2026-10-15**, one ESP32-S3-Touch-AMOLED-1.75C board,
ESP-IDF v6.0.1, 32 MB Flash and 8 MB octal PSRAM. The existing M5Stack build remains
a separate target; M5Stack hardware was not available for regression testing.

The flashed application identified itself as `0.1.0-ws.1`. Its ELF SHA-256 was
`8297686088fa15e31d200312db79199b43db7f2dccab3674c6d9d5431c421a55`.
This identifies the hardware-tested artifact; later builds can have different
hashes due to build timestamps and publication/build-tool changes.

| Check | Result |
| --- | --- |
| Independent Waveshare configuration/build | Passed with ESP-IDF v6.0.1, 32 MB, version 0.1.0-ws.1 |
| Default M5Stack build regression | Fresh ESP-IDF v6.0.1 build passed; M5Stack selected, 16 MB, version 0.1.0 |
| Host flashing safeguards | Four offline unittest cases passed, including missing/ambiguous devices, wrong board, bundle identity/layout/hash failures |
| Flash and boot identity | All written regions verified by esptool; boot ELF digest matched the application |
| Content pack | 8 decks, 96 cards, 261 existing audio clips, 6,725,964 bytes; pack CRC valid |
| Layout | Chinese and English pixel-fit checks passed at 466×466 |
| Display, swipe, tap | Confirmed by the person operating the physical board |
| Speech and switching cards during playback | Confirmed clear speech and normal card changes on the physical board |
| Settings persistence | Changed volume/brightness, rebooted, read back both new values, then restored the original settings |
| Learning persistence | Reboot restored 12 browsed cards; the scripted test did not grade practice answers |
| Playback cancellation | Repeated navigation/flips logged 22 cancellations without write failures, assertions, panic or watchdog errors |
| Clock / sensors | USB time setting worked; subsequent software reset preserved ticking time; acceleration, battery and USB measurements responded |
| Framebuffer capture | All 466 rows captured, Chinese/German rendering visually inspected |

![A card captured from the device framebuffer](img/waveshare-card.png)

This is an image read from the framebuffer, not a photograph of the panel.
Raw device identities, full-device backups and machine-specific logs are not
included in the public repository.

## Remaining limits

- No independent RTC: after full power loss, correct the system clock. The current
  board clock uses UTC+8.
- Battery percentage uses the PMIC gauge, with a voltage-based estimate as a
  fallback; no discharge-curve or endurance calibration was performed.
- Overnight battery runtime, repeated battery power-off/wake cycles and systematic
  gesture testing in every orientation remain unverified.
- The locked touch dependency emits a deprecation warning for
  `esp_lcd_touch_get_coordinates`; the tested version works.
- The course retains the upstream content/review limitations and licenses. Hardware
  acceptance is not a review of the German course text.

See [build, flash and operation](../ADAPTATION.md).
