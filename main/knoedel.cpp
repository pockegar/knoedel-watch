#include "knoedel.h"

#include <string.h>

#include "gfx_util.h"

using namespace gfxu;
using namespace character;

namespace knoedel {

namespace {

constexpr float PI_F = 3.14159265f;
constexpr float DEG = PI_F / 180.0f;

// Character palette.
constexpr uint32_t K_BASE = 0xEECB8C, K_EDGE = 0xCFA563, K_SPOT = 0xC99A55, K_HERB = 0x5E9E45;
constexpr uint32_t K_FOOT = 0xD6AC6A, K_ARM = 0xE2BB7B, K_INK = 0x3A2420, K_MOUTH = 0x5B2D2A, K_BLUSH = 0xF59A9A;
constexpr uint32_t WHITE = 0xFFFFFF, DROP = 0x6FB8E6, STAR = 0xFFC93C;

// The preview draws on a 200 x 200 disc; the character and its effects are
// scaled by CHAR_SCALE around the feet (1.16 there; larger here, because the
// watch is read from further away than a web page). "Stage units" below are the units of
// that scaled group, so coordinates can be copied from knoedel.js as they are.
constexpr float FEET_X = 90, FEET_Y = 166, CHAR_SCALE = 1.3f;
constexpr float BUBBLE_AT = 2.8f;  // seconds into the loop

constexpr int POSE_PARAMS = 20;

// Pose parameters. X and Y are internal (the player positions the character);
// the rest are the registry's parameters, in the same order as POSE_NAMES.
enum {
    X, Y, JUMP, ROT, SX, SY, OPEN, LID, HAPPY, SHUT, LOOKX, LOOKY, BROWY, BROWT,
    SMILE, MOPEN, MW, WOB, BLUSH, TINT, ARML, ARMR, P_COUNT
};
static_assert(P_COUNT - JUMP == POSE_PARAMS && POSE_PARAMS <= MAX_POSE, "pose table out of step");

const char *const POSE_NAMES[POSE_PARAMS] = {"jump", "rot", "sx", "sy", "open", "lid", "happy", "shut", "lookX", "lookY",
                                             "browY", "browT", "smile", "mopen", "mw", "wob", "blush", "tint", "armL", "armR"};

struct Pose {
    float v[P_COUNT];
};

const Pose NEUTRAL_POSE = {{FEET_X, FEET_Y, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0.25f, 0, 1, 0, 0, 0, 14, 14}};

float ease(float u) { return -(cosf(PI_F * u) - 1.0f) / 2.0f; }
float back_out(float u)
{
    const float c = 1.7f;
    return 1 + (c + 1) * powf(u - 1, 3) + c * powf(u - 1, 2);
}
float frand(int i, int k)
{
    float x = sinf(i * 127.1f + k * 311.7f) * 43758.5453f;
    return x - floorf(x);
}
float fract(float v) { return v - floorf(v); }

// Neutral, ease into the target pose, hold, ease back out.
Pose pose_at(const Anim &anim, float t)
{
    float u;
    if (t < 0.5f) {
        u = 0;
    } else if (t < 1.1f) {
        u = ease((t - 0.5f) / 0.6f);
    } else if (t < LOOP_SECONDS - 0.6f) {
        u = 1;
    } else {
        u = 1.0f - ease(clampf((t - (LOOP_SECONDS - 0.6f)) / 0.6f));
    }
    Pose p = NEUTRAL_POSE;
    for (int i = 0; i < POSE_PARAMS; i++) {
        p.v[JUMP + i] += (anim.pose[i] - NEUTRAL_POSE.v[JUMP + i]) * u;
    }
    return p;
}

// ---- motions: layered on top of the pose; `w` fades them in and out ----------

// Sneeze timing, shared by the motion and the effect of that name (2.8 s cycle).
void sneeze_phase(float c, float &build, float &burst)
{
    const float u = fmodf(c, 2.8f) / 2.8f;
    build = u < 0.55f ? ease(u / 0.55f) : u < 0.62f ? 1 : fmaxf(0, 1 - (u - 0.62f) / 0.25f);
    burst = (u >= 0.55f && u < 0.85f) ? (u - 0.55f) / 0.3f : 0;
}
// Yawn timing (3.4 s cycle, mouth open for the first 45 %).
float yawn_phase(float c)
{
    const float u = fmodf(c, 3.4f) / 3.4f;
    return u < 0.45f ? sinf(PI_F * u / 0.45f) : 0;
}

using MotionFn = void (*)(float *v, float c, float w);

void m_bounce(float *v, float c, float w)
{
    float s = fabsf(sinf(c * 5));
    v[JUMP] += w * s * 9;
    v[SY] *= 1 - .06f * w * (1 - s);
    v[SX] *= 1 + .05f * w * (1 - s);
}
void m_sway_slow(float *v, float c, float w) { v[ROT] += sinf(c * 1.2f) * 1.5f * w; }
void m_shiver(float *v, float c, float w) { v[X] += sinf(c * 45) * 1.1f * w; }
void m_eye_dart(float *v, float c, float w) { v[LOOKX] += sinf(c * 2.6f) * 2.6f * w; }
void m_jump_wave(float *v, float c, float w)
{
    float s = fabsf(sinf(c * 7));
    v[JUMP] += w * s * 15;
    v[ARML] += sinf(c * 14) * 12 * w;
    v[ARMR] += sinf(c * 14 + 1) * 12 * w;
    v[SY] *= 1 - .07f * w * (1 - s);
}
void m_tremble(float *v, float c, float w) { v[X] += sinf(c * 30) * .7f * w; }
void m_sway(float *v, float c, float w) { v[ROT] += sinf(c * 1.1f) * 4 * w; }
void m_breathe(float *v, float c, float w)
{
    float k = 1 + sinf(c * 1.8f) * .015f * w;
    v[SX] *= k;
    v[SY] *= k;
}
void m_shake(float *v, float c, float w) { v[X] += sinf(c * 50) * 1.4f * w; }
void m_exhale(float *v, float c, float w)
{
    float k = fract(c * .45f);
    v[SY] *= 1 + (k < .4f ? k / .4f * .04f : (1 - (k - .4f) / .6f) * .04f) * w - .02f * w;
}
void m_hop(float *v, float c, float w) { v[JUMP] += w * fmaxf(0, sinf(c * 2.4f)) * 3; }
void m_twitch(float *v, float c, float w) { v[JUMP] += w * (sinf(c * 9) > .7f ? 1.5f : 0); }
void m_yawn(float *v, float c, float w)
{
    const float e = yawn_phase(c) * w;
    v[MOPEN] = fmaxf(v[MOPEN], 1.15f * e);
    v[MW] += (0.72f - v[MW]) * e;
    v[SHUT] = fmaxf(v[SHUT], e);
    v[ROT] -= 3 * e;
    v[SY] *= 1 + 0.04f * e;
}
void m_sneeze(float *v, float c, float w)
{
    float build, burst;
    sneeze_phase(c, build, burst);
    const float kick = burst > 0 ? sinf(PI_F * fminf(1, burst * 2)) : 0;
    v[ROT] += (-7 * build + 16 * kick) * w;
    v[SHUT] = fmaxf(v[SHUT], fmaxf(build, burst > 0 ? 1.0f : 0.0f) * w);
    v[MOPEN] = fmaxf(v[MOPEN], (0.5f * build + (burst > 0 ? 0.4f : 0)) * w);
    v[JUMP] += 4 * kick * w;
}
void m_fan(float *v, float c, float w)
{
    v[ARMR] += sinf(c * 12) * 28 * w;
    v[ROT] += sinf(c * 12) * 1.2f * w;
}

// Typing on a keyboard: the arms drum in turn, eyes on the keys.
void m_type(float *v, float c, float w)
{
    v[ARML] += sinf(c * 16) * 14 * w;
    v[ARMR] += sinf(c * 16 + PI_F) * 14 * w;
    v[JUMP] += fabsf(sinf(c * 16)) * 0.8f * w;
    v[LOOKY] += 1.5f * w;
}

// a2-reaktionen
void m_nod(float *v, float c, float w)
{
    const float s = fmaxf(0, sinf(c * 6));
    v[SY] *= 1 - 0.05f * s * w;
    v[LOOKY] += 1.6f * s * w;
    v[BROWY] += s * w;
}
void m_shrug(float *v, float c, float w)
{
    const float s = ease(clampf(sinf(c * 2.2f) * 1.2f));
    v[ARML] += 30 * s * w;
    v[ARMR] += 30 * s * w;
    v[SY] *= 1 - 0.04f * s * w;
    v[BROWY] -= 2 * s * w;
}
void m_wave(float *v, float c, float w)
{
    v[ARMR] += sinf(c * 8) * 25 * w;
    v[ROT] += sinf(c * 8) * w;
}

// Celebrations: a pirouette on the spot, seen as the body turning edge-on.
void m_spin(float *v, float c, float w)
{
    const float k = fract(c * 0.55f), turn = ease(clampf(k * 1.8f));
    v[SX] *= 1 - (1 - fmaxf(0.15f, fabsf(cosf(PI_F * turn)))) * w;
    v[JUMP] += sinf(PI_F * clampf(k * 1.8f)) * 10 * w;
}

struct MotionDef {
    const char *id;
    MotionFn fn;
};
const MotionDef MOTIONS[] = {
    {"bounce", m_bounce}, {"sway-slow", m_sway_slow}, {"shiver", m_shiver}, {"eye-dart", m_eye_dart},
    {"jump-wave", m_jump_wave}, {"tremble", m_tremble}, {"sway", m_sway}, {"breathe", m_breathe},
    {"shake", m_shake}, {"exhale", m_exhale}, {"hop", m_hop}, {"twitch", m_twitch},
    {"yawn", m_yawn}, {"sneeze", m_sneeze}, {"fan", m_fan}, {"type", m_type},
    {"nod", m_nod}, {"shrug", m_shrug}, {"wave", m_wave}, {"spin", m_spin},
};
constexpr int MOTION_COUNT = sizeof(MOTIONS) / sizeof(MOTIONS[0]);

// Character-local units -> screen pixels (scale, then rotate about the feet).
struct Xf {
    float ox, oy, s, cs, sn, sx, sy;
    void pt(float x, float y, float &px, float &py) const
    {
        float lx = x * sx, ly = y * sy;
        px = ox + (lx * cs - ly * sn) * s;
        py = oy + (lx * sn + ly * cs) * s;
    }
};

void draw_arm(G *g, const Xf &xf, float shoulder_x, float side, float angle_deg)
{
    float a = angle_deg * DEG;
    float sx0, sy0, hx, hy, ex, ey;
    xf.pt(shoulder_x, -36, sx0, sy0);
    xf.pt(shoulder_x + side * 16 * sinf(a), -36 + 16 * cosf(a), ex, ey);
    xf.pt(shoulder_x + side * 18 * sinf(a), -36 + 18 * cosf(a), hx, hy);
    line(g, sx0, sy0, ex, ey, 6.5f * xf.s, K_ARM);
    circle(g, hx, hy, 5.5f * xf.s, K_EDGE);
    circle(g, hx, hy, 4.6f * xf.s, K_ARM);
}

void draw_character(G *g, const Xf &xf, const Pose &p, uint32_t body)
{
    const float *v = p.v;
    const float S = xf.s;
    float x, y, x2, y2;

    xf.pt(-13, -3, x, y);
    ellipse(g, x, y, 10 * S, 5.5f * S, K_FOOT);
    xf.pt(13, -3, x, y);
    ellipse(g, x, y, 10 * S, 5.5f * S, K_FOOT);

    xf.pt(0, -37, x, y);
    ellipse(g, x, y, 36.8f * S * xf.sx, 36.8f * S * xf.sy, K_EDGE);
    ellipse(g, x, y, 35.2f * S * xf.sx, 35.2f * S * xf.sy, body);

    static const float SPOTS[][4] = {{-21, -54, 3, 2}, {19, -21, 2.6f, 1.8f}, {-9, -13, 2.2f, 1.6f},
                                     {25, -50, 2.4f, 1.7f}, {-27, -28, 2.2f, 1.5f}, {6, -66, 2, 1.4f}};
    for (auto &s : SPOTS) {
        xf.pt(s[0], s[1], x, y);
        ellipse(g, x, y, s[2] * S, s[3] * S, K_SPOT, 0.55f);
    }
    static const float HERBS[][2] = {{-14, -62}, {12, -60}, {24, -33}, {-23, -19}, {2, -10}};
    for (int i = 0; i < 5; i++) {
        float a = i * 47 * DEG;
        float dx = 3 * cosf(a) + 1.5f * sinf(a), dy = 3 * sinf(a) - 1.5f * cosf(a);
        xf.pt(HERBS[i][0], HERBS[i][1], x, y);
        xf.pt(HERBS[i][0] + dx, HERBS[i][1] + dy, x2, y2);
        line(g, x, y, x2, y2, 2 * S, K_HERB);
    }
    // Soft highlight on the upper left.
    xf.pt(-19.5f, -54.2f, x, y);
    xf.pt(-10.5f, -59.8f, x2, y2);
    line(g, x, y, x2, y2, 8 * S, mix(body, WHITE, 0.36f));

    draw_arm(g, xf, -31, -1, v[ARML]);
    draw_arm(g, xf, 31, 1, v[ARMR]);

    if (v[BLUSH] > 0.02f) {
        xf.pt(-19, -34, x, y);
        ellipse(g, x, y, 6 * S, 3.4f * S, K_BLUSH, v[BLUSH] * 0.75f);
        xf.pt(19, -34, x, y);
        ellipse(g, x, y, 6 * S, 3.4f * S, K_BLUSH, v[BLUSH] * 0.75f);
    }

    // Eyes. Face origin is (0,-42), each eye sits at (+-10,-3) from it.
    const float vis = 1.0f - fmaxf(v[HAPPY], v[SHUT]);
    const float ry = 4.6f * v[OPEN];
    for (int side = -1; side <= 1; side += 2) {
        const float ex = side * 10.0f, ey = -45.0f;
        if (vis > 0.03f) {
            xf.pt(ex + v[LOOKX], ey + v[LOOKY], x, y);
            ellipse(g, x, y, 3.5f * S, ry * S, K_INK, vis);
            xf.pt(ex + v[LOOKX] + 1.1f, ey + v[LOOKY] - ry * .35f, x, y);
            circle(g, x, y, 1.15f * S, WHITE, vis);
            if (v[LID] > 0.02f) {
                xf.pt(ex - 5, ey + v[LOOKY] - ry - .6f, x, y);
                xf.pt(ex + 5, ey + v[LOOKY] - ry - .6f + v[LID] * ry * 2 + .6f, x2, y2);
                g->fillRect((int)x, (int)y, (int)(x2 - x) + 1, (int)(y2 - y) + 1, body);
            }
        }
        if (v[HAPPY] > 0.03f) {
            float ax, ay, bx, by, cx, cy;
            xf.pt(ex - 4.2f, ey + 1.6f, ax, ay);
            xf.pt(ex, ey - 4.4f, cx, cy);
            xf.pt(ex + 4.2f, ey + 1.6f, bx, by);
            quad(g, ax, ay, cx, cy, bx, by, 2.2f * S, mix(body, K_INK, v[HAPPY]));
        }
        if (v[SHUT] > 0.03f) {
            float ax, ay, bx, by, cx, cy;
            xf.pt(ex - 4.2f, ey - .8f, ax, ay);
            xf.pt(ex, ey + 3.4f, cx, cy);
            xf.pt(ex + 4.2f, ey - .8f, bx, by);
            quad(g, ax, ay, cx, cy, bx, by, 2.2f * S, mix(body, K_INK, v[SHUT]));
        }
    }

    // Brows.
    const float by = -42 - 14 + v[BROWY], bt = v[BROWT];
    xf.pt(-15, by + bt * 1.6f, x, y);
    xf.pt(-5, by - bt * 3, x2, y2);
    line(g, x, y, x2, y2, 2.3f * S, K_INK);
    xf.pt(15, by + bt * 1.6f, x, y);
    xf.pt(5, by - bt * 3, x2, y2);
    line(g, x, y, x2, y2, 2.3f * S, K_INK);

    // Mouth: the area between two quadratic curves that share their end points.
    const float mw = 8 * v[MW], my = -42 + 12;
    const float c1 = my + v[SMILE] * 7, c2 = c1 + v[MOPEN] * 11;
    // Below half, the wobble only reads as noise, so show one mouth or the other.
    const float wob = clampf((v[WOB] - 0.5f) * 4 + 0.5f);
    if (wob < 0.97f) {
        const uint32_t mc = mix(body, K_MOUTH, 1.0f - wob);
        const int N = 7;
        float ux[N + 1], uy[N + 1], lx[N + 1], ly[N + 1];
        for (int i = 0; i <= N; i++) {
            float u = (float)i / N, q = 1 - u;
            float px = -mw * q * q + mw * u * u;
            xf.pt(px, q * q * my + 2 * q * u * c1 + u * u * my, ux[i], uy[i]);
            xf.pt(px, q * q * my + 2 * q * u * c2 + u * u * my, lx[i], ly[i]);
        }
        for (int i = 0; i < N; i++) {
            if (v[MOPEN] > 0.05f) {
                g->fillTriangle(ux[i], uy[i], ux[i + 1], uy[i + 1], lx[i], ly[i], mc);
                g->fillTriangle(lx[i], ly[i], ux[i + 1], uy[i + 1], lx[i + 1], ly[i + 1], mc);
                line(g, lx[i], ly[i], lx[i + 1], ly[i + 1], 2.2f * S, mc);
            }
            line(g, ux[i], uy[i], ux[i + 1], uy[i + 1], 2.2f * S, mc);
        }
    }
    if (wob > 0.03f) {
        const uint32_t wc = mix(body, K_MOUTH, wob);
        xf.pt(-mw, my, x, y);
        for (int i = 1; i <= 6; i++) {
            xf.pt(-mw + mw / 3 * i, my - ((i & 1) ? 2.4f : 0), x2, y2);
            line(g, x, y, x2, y2, 2 * S, wc);
            x = x2;
            y = y2;
        }
    }
}

// ---- effects --------------------------------------------------------------

struct Fx {
    G *g;
    const Stage *st;
    float s;           // pixels per stage unit
    float ox, oy;      // screen position of the stage point (FEET_X, FEET_Y)
    float hx, hy;      // top of the head, stage units
    float my;          // mouth height, stage units
    float c, w;

    float sx(float x) const { return ox + (x - FEET_X) * s; }
    float sy(float y) const { return oy + (y - FEET_Y) * s; }
    // The disc, in stage units.
    float disc_x() const { return FEET_X + (st->cx - ox) / s; }
    float disc_y() const { return FEET_Y + (st->cy - oy) / s; }
    float disc_r() const { return st->r / s; }
    bool inside(float x, float y, float margin) const
    {
        float dx = sx(x) - st->cx, dy = sy(y) - st->cy, r = st->r - margin * s;
        return r > 0 && dx * dx + dy * dy < r * r;
    }
    uint32_t fade(uint32_t color, float a) const { return mix(st->bg, color, a); }

    void drop(float x, float y, float a, uint32_t color = DROP) const
    {
        if (a < 0.05f || !inside(x, y, 6)) {
            return;
        }
        wedge(g, sx(x), sy(y - 4.5f), sx(x), sy(y + 2.5f), 0.4f * s, 3 * s, fade(color, a));
    }
    void puff(float x, float y, float r, float a, uint32_t color = WHITE) const
    {
        if (a < 0.05f || r < 0.5f || !inside(x, y, r)) {
            return;
        }
        circle(g, sx(x), sy(y), r * s, color, a);
    }
    void sparkle(float x, float y, float scale, float a) const
    {
        if (a < 0.05f || !inside(x, y, 6 * scale)) {
            return;
        }
        const uint32_t col = fade(STAR, a);
        const float px = sx(x), py = sy(y), r = 6 * scale * s, core = 1.9f * scale * s;
        wedge(g, px, py, px + r, py, core, 0.3f, col);
        wedge(g, px, py, px - r, py, core, 0.3f, col);
        wedge(g, px, py, px, py + r, core, 0.3f, col);
        wedge(g, px, py, px, py - r, core, 0.3f, col);
    }
    void text(const lgfx::IFont *font, const char *str, float x, float y, lgfx::textdatum_t datum, uint32_t color) const
    {
        if (font == nullptr) {
            return;
        }
        g->setFont(font);
        g->setTextDatum(datum);
        g->setTextColor(color);
        g->drawString(str, (int)lroundf(sx(x)), (int)lroundf(sy(y)));
    }
};

using FxFn = void (*)(const Fx &f);

void fx_confetti(const Fx &f)
{
    static const uint32_t COLORS[] = {0xF06C8B, 0xFFC93C, 0x4BA3D9, 0x5E9E45};
    const float r = f.disc_r();
    for (int i = 0; i < 16; i++) {
        float k = fract(f.c * .6f + frand(i, 1));
        float x = f.disc_x() + (frand(i, 2) - .5f) * 1.9f * r + sinf(f.c * 3 + i) * 6, y = f.disc_y() - r + k * 2 * r;
        if (!f.inside(x, y, 5)) {
            continue;
        }
        float a = (f.c * 200 + i * 50) * DEG;
        float dx = sinf(a) * 1.8f, dy = cosf(a) * 1.8f;
        line(f.g, f.sx(x - dx), f.sy(y - dy), f.sx(x + dx), f.sy(y + dy), 4 * f.s, f.fade(COLORS[i % 4], f.w * .9f));
    }
}

void fx_rain_cloud(const Fx &f)
{
    float cx = f.hx + sinf(f.c) * 3, cy = f.hy - 20;
    for (int i = 0; i < 6; i++) {
        float k = fract(f.c * 1.8f + i / 6.0f), x = cx - 12 + i * 5, y = cy + 8 + k * 20;
        line(f.g, f.sx(x), f.sy(y), f.sx(x - 1), f.sy(y + 5), 1.6f * f.s, f.fade(0x6FA1D6, f.w * (1 - k)));
    }
    f.puff(cx - 10, cy, 9, f.w, 0x8E9AAB);
    f.puff(cx + 11, cy, 9, f.w, 0x8E9AAB);
    f.puff(cx, cy - 5, 11, f.w, 0x8E9AAB);
}

void fx_tear(const Fx &f)
{
    float k = fract(f.c * .5f);
    f.drop(f.hx - 10, f.hy + 34 + k * 14, f.w * (1 - k));
}

void fx_sweat(const Fx &f)
{
    for (int i = 0; i < 2; i++) {
        float k = fract(f.c * .7f + i * .5f);
        f.drop(f.hx + (i ? 27 : -27), f.hy + 12 + k * 16, f.w * (1 - k));
    }
}

void fx_star_orbit(const Fx &f)
{
    for (int i = 0; i < 5; i++) {
        float a = i / 5.0f * 2 * PI_F + f.c * .8f, r = 46 + sinf(f.c * 4 + i) * 4;
        f.sparkle(f.hx + cosf(a) * r, f.hy + 34 + sinf(a) * r * .72f, .8f + .4f * fabsf(sinf(f.c * 5 + i)), f.w);
    }
}

void fx_scribble(const Fx &f)
{
    float px = 0, py = 0;
    for (int i = 0; i < 14; i++) {
        float a = i * .9f + f.c * 6, r = 4 + i * .9f;
        float x = f.sx(f.hx + cosf(a) * r * 1.3f), y = f.sy(f.hy - 12 + sinf(a) * r * .5f);
        if (i) {
            line(f.g, px, py, x, y, 1.6f * f.s, f.fade(K_INK, f.w));
        }
        px = x;
        py = y;
    }
}

void fx_paper_orbit(const Fx &f)
{
    for (int i = 0; i < 4; i++) {
        float a = f.c * 1.6f + i * PI_F / 2;
        float x = f.hx + cosf(a) * 50, y = f.hy + 38 + sinf(a) * 22;
        if (!f.inside(x, y, 7)) {
            continue;
        }
        // A sheet of paper: rotated rectangle with two text lines.
        float r = (f.c * 90 + i * 40) * DEG, cs = cosf(r), sn = sinf(r);
        auto corner = [&](float lx, float ly, float &ox, float &oy) {
            ox = f.sx(x + lx * cs - ly * sn);
            oy = f.sy(y + lx * sn + ly * cs);
        };
        float ax, ay, bx, by, cx2, cy2, dx, dy;
        corner(-5, -6, ax, ay);
        corner(5, -6, bx, by);
        corner(5, 6, cx2, cy2);
        corner(-5, 6, dx, dy);
        const uint32_t paper = f.fade(WHITE, f.w), ink = f.fade(0xB4BBC6, f.w);
        f.g->fillTriangle(ax, ay, bx, by, cx2, cy2, paper);
        f.g->fillTriangle(ax, ay, cx2, cy2, dx, dy, paper);
        for (float ly = -2; ly <= 1; ly += 3) {
            float x0, y0, x1, y1;
            corner(-3, ly, x0, y0);
            corner(3, ly, x1, y1);
            line(f.g, x0, y0, x1, y1, 1 * f.s, ink);
        }
    }
}

void fx_sigh_puffs(const Fx &f)
{
    float k = fract(f.c * .45f);
    for (int i = 0; i < 3; i++) {
        float kk = clampf(k * 1.4f - i * .12f);
        f.puff(f.hx + 14 + kk * 30, f.hy + 50 - kk * 10 + i * 2, 2 + kk * 5 - i, f.w * (1 - kk) * .9f, 0xE3E7EC);
    }
}

void fx_music_notes(const Fx &f)
{
    G *g = f.g;
    const float s = f.s;
    for (int i = 0; i < 3; i++) {
        float k = fract(f.c * .35f + i / 3.0f);
        float x = f.hx - 30 + i * 30 + sinf(f.c + i) * 4, y = f.hy + 4 - k * 28;
        float a = f.w * sinf(k * PI_F);
        if (a < 0.05f || !f.inside(x, y, 8)) {
            continue;
        }
        const uint32_t col = f.fade(f.st->accent, a);
        if (i == 1) {  // a quaver
            ellipse(g, f.sx(x - 1.5f), f.sy(y + 4), 3.2f * s, 2.4f * s, col);
            line(g, f.sx(x + 1.2f), f.sy(y + 4), f.sx(x + 1.2f), f.sy(y - 6), 1.5f * s, col);
            quad(g, f.sx(x + 1.2f), f.sy(y - 6), f.sx(x + 5), f.sy(y - 5), f.sx(x + 5.5f), f.sy(y - 1), 1.5f * s, col);
        } else {  // a tilde
            quad(g, f.sx(x - 5), f.sy(y), f.sx(x - 2.5f), f.sy(y - 4), f.sx(x), f.sy(y), 1.8f * s, col);
            quad(g, f.sx(x), f.sy(y), f.sx(x + 2.5f), f.sy(y + 4), f.sx(x + 5), f.sy(y), 1.8f * s, col);
        }
    }
}

void fx_steam(const Fx &f)
{
    for (int i = 0; i < 4; i++) {
        float k = fract(f.c * 1.1f + i / 4.0f), side = (i % 2) ? 1 : -1;
        f.puff(f.hx + side * (14 + k * 10), f.hy + 2 - k * 24, 3 + k * 5, f.w * (1 - k) * .95f);
    }
}

// Four little corners, pulsing.
void fx_anger_vein(const Fx &f)
{
    const float vx = f.hx + 22, vy = f.hy + 12, k = 1 + .15f * sinf(f.c * 8);
    const uint32_t col = mix(K_BASE, f.st->accent, f.w);
    static const float V[4][6] = {{-5, -1, -1, -1, -1, -5}, {1, -5, 1, -1, 5, -1}, {5, 1, 1, 1, 1, 5}, {-1, 5, -1, 1, -5, 1}};
    for (auto &q : V) {
        quad(f.g, f.sx(vx + q[0] * k), f.sy(vy + q[1] * k), f.sx(vx + q[2] * k), f.sy(vy + q[3] * k),
             f.sx(vx + q[4] * k), f.sy(vy + q[5] * k), 2 * f.s, col);
    }
}

void fx_puh_cloud(const Fx &f)
{
    float k = fract(f.c * .45f), kk = clampf((k - .4f) / .6f);
    float a = f.w * (k < .4f ? 0 : 1 - kk * .8f), sc = .7f + kk * .5f;
    float x = f.hx + 14 + kk * 14, y = f.hy + 50 - kk * 8;
    if (a > 0.05f && f.inside(x + 8 * sc, y, 16 * sc)) {
        circle(f.g, f.sx(x), f.sy(y), 7 * sc * f.s, WHITE, a);
        circle(f.g, f.sx(x + 16 * sc), f.sy(y), 7 * sc * f.s, WHITE, a);
        circle(f.g, f.sx(x + 8 * sc), f.sy(y - 2 * sc), 9 * sc * f.s, WHITE, a);
        if (a > 0.5f) {
            f.text(f.st->font_small, "puh", x + 8 * sc, y - 1.5f * sc, middle_center, mix(WHITE, f.st->accent, a));
        }
    }
}

void fx_sweat_fly(const Fx &f)
{
    float d = fract(f.c * .6f);
    f.drop(f.hx - 26 - d * 14, f.hy + 10 - d * 8 + d * d * 30, f.w * (1 - d));
}

void fx_exclamation(const Fx &f)
{
    f.text(f.st->font_big, "!", f.hx + sinf(f.c * 6) * 2.5f, f.hy - 2 - fabsf(sinf(f.c * 6)) * 2, bottom_center,
           f.fade(f.st->accent, f.w));
}

void fx_sparkles(const Fx &f)
{
    static const float POS[4][2] = {{-40, 20}, {38, 8}, {-30, 62}, {44, 56}};
    for (int i = 0; i < 4; i++) {
        float tw = fabsf(sinf(f.c * 3 + i * 1.3f));
        f.sparkle(f.hx + POS[i][0], f.hy + POS[i][1], .4f + tw * .8f, f.w * tw);
    }
}

// Zigzags to the left of the body: noise, or a rumbling stomach.
void fx_noise_waves(const Fx &f)
{
    for (int i = 0; i < 3; i++) {
        float k = fract(f.c * 1.5f + i / 3.0f);
        float x = f.hx - 60 - k * 8, y = f.hy + 24 + i * 14;
        const uint32_t col = f.fade(f.st->accent, f.w * (1 - k));
        for (int j = 0; j < 4; j++) {
            float y0 = y - ((j & 1) ? 4 : 0), y1 = y - ((j & 1) ? 0 : 4);
            if (f.inside(x + j * 4, y, 3)) {
                line(f.g, f.sx(x + j * 4), f.sy(y0), f.sx(x + j * 4 + 4), f.sy(y1), 2 * f.s, col);
            }
        }
    }
}

// A lower-case z from three strokes, so it stays sharp at any size.
void letter_z(const Fx &f, float x, float baseline, float size, uint32_t color)
{
    const float a = size * 0.25f, cx = x + a, cy = baseline - a, wd = size * 0.17f * f.s;
    line(f.g, f.sx(cx - a), f.sy(cy - a), f.sx(cx + a), f.sy(cy - a), wd, color);
    line(f.g, f.sx(cx + a), f.sy(cy - a), f.sx(cx - a), f.sy(cy + a), wd, color);
    line(f.g, f.sx(cx - a), f.sy(cy + a), f.sx(cx + a), f.sy(cy + a), wd, color);
}

void fx_zzz(const Fx &f)
{
    static const float SIZES[3] = {9, 12, 15};
    for (int i = 0; i < 3; i++) {
        float k = fract(f.c * 0.45f + i / 3.0f);
        float x = f.hx + 24 + k * 16 + i * 3, y = f.hy + 8 - k * 30;
        float a = f.w * sinf(k * PI_F) * (1 - yawn_phase(f.c));
        if (a < 0.05f || !f.inside(x + 3, y - 3, 6)) {
            continue;
        }
        letter_z(f, x, y, SIZES[i], f.fade(f.st->accent, a));
    }
}

// A fever thermometer in the mouth.
void fx_thermometer(const Fx &f)
{
    const float r = sinf(f.c * 2) * 3 * DEG, cs = cosf(r), sn = sinf(r);
    const float bx = f.hx + 3, by = f.my;
    auto px = [&](float x, float y) { return f.sx(bx + x * cs - y * sn); };
    auto py = [&](float x, float y) { return f.sy(by + x * sn + y * cs); };
    line(f.g, px(0, 0), py(0, 0), px(22, -8), py(22, -8), 5 * f.s, mix(K_BASE, 0x9AA4B1, f.w));
    line(f.g, px(0, 0), py(0, 0), px(22, -8), py(22, -8), 3.2f * f.s, mix(K_BASE, WHITE, f.w));
    line(f.g, px(1, -.4f), py(1, -.4f), px(14, -5.1f), py(14, -5.1f), 1.6f * f.s, mix(K_BASE, 0xE5484D, f.w));
    const float a = f.w * (0.6f + 0.4f * fabsf(sinf(f.c * 3)));
    if (a > 0.3f) {
        f.text(f.st->font_small, "39°", bx + 21, by - 12, bottom_left, f.fade(0xE5484D, a));
    }
}

void fx_sneeze(const Fx &f)
{
    float build, burst;
    sneeze_phase(f.c, build, burst);
    if (burst <= 0) {
        return;
    }
    for (int i = 0; i < 5; i++) {
        f.puff(f.hx + 10 + burst * 26 * (1 + i * 0.18f), f.my + (i - 2) * 3 * burst + burst * burst * 8, 1.6f,
               f.w * (1 - burst), 0x8FC3E6);
    }
    const float a = f.w * (1 - burst * 0.5f);
    if (a > 0.3f) {
        // To the left of the head: the right side belongs to the thought bubble.
        f.text(f.st->font_small, "Hatschi!", f.hx - 34, f.hy + 6 - burst * 8, bottom_center, f.fade(f.st->accent, a));
    }
}

void fx_snowflakes(const Fx &f)
{
    for (int i = 0; i < 18; i++) {
        float y = fmodf(frand(i, 2) * 220 + f.c * 14 * (0.8f + frand(i, 3) * 0.4f), 220) - 10;
        float x = frand(i, 1) * 200 + sinf(f.c + i) * 4;
        if (!f.inside(x, y, 3)) {
            continue;
        }
        circle(f.g, f.sx(x), f.sy(y), 2.3f * f.s, 0x9CC3E6, f.w * 0.95f);
        circle(f.g, f.sx(x), f.sy(y), 1.7f * f.s, WHITE, f.w * 0.95f);
    }
}

// A spiral turning above the head: dizzy, queasy.
void fx_swirl(const Fx &f)
{
    const float rot = fmodf(f.c * 160, 360) * DEG, cs = cosf(rot), sn = sinf(rot);
    const uint32_t col = f.fade(f.st->accent, f.w);
    float px = 0, py = 0;
    for (int i = 0; i <= 40; i += 2) {
        float a = i * 0.42f, r = 1 + i * 0.32f;
        float lx = cosf(a) * r, ly = sinf(a) * r * 0.55f;
        float x = f.sx(f.hx + lx * cs - ly * sn), y = f.sy(f.hy - 10 + lx * sn + ly * cs);
        if (i) {
            line(f.g, px, py, x, y, 1.8f * f.s, col);
        }
        px = x;
        py = y;
    }
}

// Laughter: three "ha" around the head, one after the other.
void fx_haha(const Fx &f)
{
    // Lower on the right than in the preview: the thought bubble sits up there.
    static const float POS[3][2] = {{36, 24}, {-38, 16}, {42, 42}};
    for (int i = 0; i < 3; i++) {
        const float k = fract(f.c * 1.2f + i / 3.0f), a = f.w * sinf(k * PI_F);
        if (a > 0.3f) {
            f.text(f.st->font_small, "ha", f.hx + POS[i][0], f.hy + POS[i][1] - k * 6, middle_center, f.fade(f.st->accent, a));
        }
    }
}

// Two question marks pop up beside the head, one after the other.
void fx_question(const Fx &f)
{
    for (int i = 0; i < 2; i++) {
        const float k = fract(f.c * 0.7f + i * 0.5f), a = f.w * (k < 0.85f ? 1 : (1 - k) / 0.15f);
        if (k > 0.06f && a > 0.3f) {
            f.text(i ? f.st->font_small : f.st->font_big, "?", f.hx + (i ? -30 : 32), f.hy + (i ? 4 : -8), middle_center,
                   f.fade(f.st->accent, a));
        }
    }
}

struct FxDef {
    const char *id;
    FxFn fn;
};
const FxDef EFFECTS[] = {
    {"confetti", fx_confetti}, {"rain-cloud", fx_rain_cloud}, {"tear", fx_tear}, {"sweat", fx_sweat},
    {"star-orbit", fx_star_orbit}, {"scribble", fx_scribble}, {"paper-orbit", fx_paper_orbit},
    {"sigh-puffs", fx_sigh_puffs}, {"music-notes", fx_music_notes}, {"steam", fx_steam},
    {"anger-vein", fx_anger_vein}, {"puh-cloud", fx_puh_cloud}, {"sweat-fly", fx_sweat_fly},
    {"exclamation", fx_exclamation}, {"sparkles", fx_sparkles}, {"noise-waves", fx_noise_waves},
    {"zzz", fx_zzz}, {"thermometer", fx_thermometer}, {"sneeze", fx_sneeze}, {"snowflakes", fx_snowflakes},
    {"swirl", fx_swirl}, {"haha", fx_haha}, {"question", fx_question},
};
constexpr int FX_COUNT = sizeof(EFFECTS) / sizeof(EFFECTS[0]);

int k_count(Kind kind)
{
    return kind == POSE ? POSE_PARAMS : kind == MOTION ? MOTION_COUNT : kind == FX ? FX_COUNT : kind == BUBBLE ? bubble_count() : 0;
}

const char *k_name(Kind kind, int i)
{
    return kind == POSE ? POSE_NAMES[i] : kind == MOTION ? MOTIONS[i].id : kind == FX ? EFFECTS[i].id : bubble_name(i);
}

float k_pose_neutral(int index) { return NEUTRAL_POSE.v[JUMP + index]; }

void k_draw(G *g, const Stage &st, const Anim &anim, float t, float c)
{
    const float u = st.r / 100.0f;  // pixels per unit of the 200 x 200 disc
    const float s = u * CHAR_SCALE;
    const float ox = st.cx + (FEET_X - 100) * u, oy = st.cy + (FEET_Y - 100) * u;

    Pose p = pose_at(anim, t);
    const float w = clampf((t - .9f) / .4f) * clampf((LOOP_SECONDS - .35f - t) / .45f);
    float *v = p.v;
    for (int i = 0; i < anim.n_motion; i++) {
        MOTIONS[anim.motion[i]].fn(v, c, w);
    }

    const float shadow_x = ox + (v[X] - FEET_X) * s;
    ellipse(g, shadow_x, oy + 1 * s, (30 * v[SX] - v[JUMP] * .4f) * s, 4.5f * s, 0x000000, 0.09f);

    uint32_t body = K_BASE;
    if (anim.has_tint && v[TINT] > 0) {
        body = mix(K_BASE, anim.tint, v[TINT] * .6f);
    }
    Xf xf;
    xf.ox = ox + (v[X] - FEET_X) * s;
    xf.oy = oy + (v[Y] - v[JUMP] - FEET_Y) * s;
    xf.s = s;
    xf.cs = cosf(v[ROT] * DEG);
    xf.sn = sinf(v[ROT] * DEG);
    xf.sx = v[SX];
    xf.sy = v[SY];
    draw_character(g, xf, p, body);

    if (w >= 0.03f) {
        Fx f;
        f.g = g;
        f.st = &st;
        f.s = s;
        f.ox = ox;
        f.oy = oy;
        f.hx = v[X];
        f.hy = v[Y] - v[JUMP] - 74 * v[SY];
        f.my = f.hy + 44 * v[SY];
        f.c = c;
        f.w = w;
        for (int i = 0; i < anim.n_fx; i++) {
            EFFECTS[anim.fx[i]].fn(f);
        }
    }

    // Thought bubble: pops up at the upper right, half outside the disc.
    if (anim.bubble >= 0) {
        const float b = clampf((t - BUBBLE_AT - 0.1f) / 0.35f) * clampf((LOOP_SECONDS - 0.6f - t) / 0.3f);
        if (b > 0.02f) {
            const float k = (b < 1 ? fmaxf(0.01f, back_out(b)) : 1) * u;
            const float bx = st.cx + 60 * u, by = st.cy + (-50 + sinf(c * 2) * 1.5f) * u;
            const float a = fminf(1, b * 1.5f);
            circle(g, st.cx + 32 * u, st.cy - 16 * u, 3.5f * u, WHITE, a);
            circle(g, st.cx + 25 * u, st.cy - 8 * u, 2.2f * u, WHITE, a);
            g->fillSmoothRoundRect((int)lroundf(bx - 32 * k), (int)lroundf(by - 27 * k), (int)lroundf(64 * k),
                                   (int)lroundf(52 * k), (int)lroundf(16 * k), WHITE);
            if (b > 0.3f) {
                draw_bubble_icon(g, st, anim.bubble, bx, by - 1 * k, k);
            }
        }
    }
}

}  // namespace

const Impl IMPL = {"knoedel", k_count, k_name, k_pose_neutral, {{38, 46, 22}, {168, 150, 32}, {160, 30, 12}}, false, k_draw};

}  // namespace knoedel
