#pragma once
// What the app needs from a board, and all it may touch of its hardware.
//
// Every board is an ESP32-S3 with PSRAM, a round touch screen, at least two keys
// and a speaker; a motion sensor and a real-time clock are optional. One file
// in main/board/ implements this header for each board, chosen in Kconfig
// ("Target board"). The app draws with M5GFX (LovyanGFX) on the board's display.
//
// What does not belong here: drawing, decks, content, settings and timing.
// They are the same on every ESP32-S3 and stay in the app.

#include <M5GFX.h>
#include <stdint.h>

namespace board {

// Starts the display, touch, keys, speaker and sensors. NVS is ready by then.
void begin();

// ---- screen -------------------------------------------------------------------

// Square, at least ui::SCREEN pixels wide, 16-bit colour. Brightness (0..255)
// and sleep() / wakeup() work as in LovyanGFX.
lgfx::LGFX_Device &display();

// ---- input --------------------------------------------------------------------

// What the user asked for, independent of the key or gesture used.
enum class Event : uint8_t {
    NONE,
    PREVIOUS,  // the left key, or a swipe to the right
    NEXT,      // the right key, or a swipe to the left
    SELECT,    // a tap on the screen
    BACK,      // holding the left key
};

// Call once per frame: reads keys and touch, returns at most one event.
Event update();

// A key or the screen is being touched right now (keeps the watch awake).
bool touched();

// ---- sound --------------------------------------------------------------------

void set_volume(uint8_t volume);  // 0..255

// Plays 16-bit samples, replacing what was playing. The board reads `pcm`
// until the sound ends or stop_sound() returns.
void play_pcm(const int16_t *pcm, size_t samples, uint32_t rate, bool stereo);

// Stops all sound, the one of play_pcm() and tones.
void stop_sound();

// A beep for the app's little tunes; it plays alongside play_pcm().
void tone(uint16_t hz, uint16_t ms);

// ---- motion sensor (optional) ---------------------------------------------------

// Acceleration in g, z pointing out of the screen: a watch lying face up
// reads about (0, 0, 1). False without a sensor.
bool has_motion();
bool acceleration(float &x, float &y, float &z);

// ---- clock (optional) -----------------------------------------------------------

struct DateTime {
    int16_t year;  // 2000..2099
    int8_t month, day, hour, minute, second;
};

bool has_clock();
// False without a clock, or if it holds no valid time (it never ran, or lost power).
bool get_time(DateTime &t);
bool set_time(const DateTime &t);

// ---- power --------------------------------------------------------------------

// Powered over USB: the watch then stays on.
bool on_usb();

// Switches the watch off; returns only where it cannot be (on USB power).
void power_off();

}  // namespace board
