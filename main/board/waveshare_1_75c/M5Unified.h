// M5Unified.h shim for the Waveshare ESP32-S3-Touch-AMOLED-1.75C.
//
// This header shadows the real M5Unified.h on the include path when the
// board choice CONFIG_KNOEDEL_BOARD_WAVESHARE_1_75C is selected, so the
// application sources keep using the M5 API unchanged while everything
// below the API runs on this board's hardware:
//   - Display: LGFX_Waveshare175C (CO5300 AMOLED over QSPI, via M5GFX)
//   - BtnA: BOOT on GPIO0; BtnB: AXP2101 short power-button press
//   - Touch: M5Unified's Touch_Class state machine, driving the panel's
//     CST9217 touch input through the LGFX device
//   - Speaker / Imu / Power / Rtc: see m5_types_shim.h
//
// Button_Class and Touch_Class are vendored unchanged from M5Unified
// (MIT license, Copyright (c) M5Stack).
#pragma once

#include <M5GFX.h>

#include "Button_Class.hpp"
#include "Touch_Class.hpp"
#include "m5_types_shim.h"
#include "lgfx_waveshare_1_75c.h"

class M5Unified
{
public:
    m5::config_t config(void);
    void begin(const m5::config_t& cfg);

    // Poll the buttons and touch panel.
    void update(void);
    // USB-only diagnostics; consumed by the same handlers as physical input.
    enum Action { None, Previous, Next, Tap, Back };
    Action takeAction(void);

    static std::uint32_t millis(void);
    static void delay(std::uint32_t msec);

    LGFX_Waveshare175C Display;
    m5::Button_Class BtnA, BtnB;
    m5::Touch_Class Touch;
    m5::Speaker_Class Speaker;
    m5::Imu_Class Imu;
    m5::Power_Class Power;
    m5::Rtc_Class Rtc;
};

extern M5Unified M5;
