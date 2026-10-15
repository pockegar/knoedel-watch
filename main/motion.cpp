#include "motion.h"

#include <esp_timer.h>
#include <math.h>

#include "board.h"

namespace motion {

namespace {

bool s_ready;
float s_gx, s_gy, s_gz = 1;  // gravity, low-pass filtered, in g
uint32_t s_down_since;
bool s_moved, s_shaken;
uint32_t s_peaks[4];  // times of the last strong jolts
int s_peak;
uint32_t s_last_peak;

uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }

}  // namespace

void update()
{
    float ax, ay, az;
    if (!board::acceleration(ax, ay, az)) {
        return;
    }
    const uint32_t now = now_ms();
    if (!s_ready) {
        s_gx = ax, s_gy = ay, s_gz = az;
        s_ready = true;
    }
    // What is left after gravity is how hard the watch is being moved.
    const float jolt = sqrtf((ax - s_gx) * (ax - s_gx) + (ay - s_gy) * (ay - s_gy) + (az - s_gz) * (az - s_gz));
    s_gx += (ax - s_gx) * 0.12f;
    s_gy += (ay - s_gy) * 0.12f;
    s_gz += (az - s_gz) * 0.12f;

    if (jolt > 0.18f) {
        s_moved = true;
    }
    // Three strong jolts within a second make a shake.
    if (jolt > 1.1f && now - s_last_peak > 90) {
        s_last_peak = now;
        s_peaks[s_peak++ % 4] = now;
        if (s_peak >= 3 && now - s_peaks[(s_peak - 3) % 4] < 1000) {
            s_shaken = true;
            s_peak = 0;
        }
    }
    if (s_gz < -0.8f) {
        s_down_since = s_down_since ? s_down_since : now;
    } else {
        s_down_since = 0;
    }
}

bool available() { return board::has_motion(); }
bool face_down() { return s_down_since && now_ms() - s_down_since > 800; }

bool moved()
{
    const bool m = s_moved;
    s_moved = false;
    return m;
}

bool shaken()
{
    const bool s = s_shaken;
    s_shaken = false;
    return s;
}

}  // namespace motion
