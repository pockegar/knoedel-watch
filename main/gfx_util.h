#pragma once
// Small anti-aliased drawing helpers on top of LovyanGFX.

#include <M5GFX.h>
#include <math.h>

namespace gfxu {

using G = lgfx::LovyanGFX;

inline float clampf(float v, float lo = 0.0f, float hi = 1.0f) { return v < lo ? lo : (v > hi ? hi : v); }

inline uint32_t mix(uint32_t a, uint32_t b, float u)
{
    u = clampf(u);
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    int r = ar + (int)lroundf((br - ar) * u);
    int g = ag + (int)lroundf((bg - ag) * u);
    int bl = ab + (int)lroundf((bb - ab) * u);
    return (uint32_t)(r << 16 | g << 8 | bl);
}

inline void span(G *g, int x, int y, int w, float alpha, uint32_t color)
{
    if (w <= 0 || alpha <= 0.02f) {
        return;
    }
    if (alpha >= 0.98f) {
        g->drawFastHLine(x, y, w, color);
    } else {
        g->fillRectAlpha(x, y, w, 1, (uint8_t)(alpha * 255.0f), color);
    }
}

// Filled ellipse with sub-pixel centre/radii, anti-aliased edges and optional opacity.
inline void ellipse(G *g, float cx, float cy, float rx, float ry, uint32_t color, float alpha = 1.0f)
{
    if (rx < 0.3f || ry < 0.3f || alpha <= 0.02f) {
        return;
    }
    int y0 = (int)floorf(cy - ry), y1 = (int)ceilf(cy + ry);
    for (int y = y0; y <= y1; y++) {
        float d = fabsf(y + 0.5f - cy);
        float vcov = clampf(ry + 0.5f - d);
        if (vcov <= 0.0f) {
            continue;
        }
        float n = d / ry;
        if (n > 0.995f) {
            n = 0.995f;
        }
        float hw = rx * sqrtf(1.0f - n * n);
        float xl = cx - hw, xr = cx + hw;
        int il = (int)ceilf(xl), ir = (int)floorf(xr);
        float a = alpha * vcov;
        if (ir <= il) {
            span(g, (int)floorf(cx), y, 1, a * clampf(xr - xl), color);
            continue;
        }
        span(g, il, y, ir - il, a, color);
        span(g, il - 1, y, 1, a * (il - xl), color);
        span(g, ir, y, 1, a * (xr - ir), color);
    }
}

inline void circle(G *g, float cx, float cy, float r, uint32_t color, float alpha = 1.0f)
{
    ellipse(g, cx, cy, r, r, color, alpha);
}

// Line whose radius changes from r0 to r1 (teardrops, sparkle rays).
//
// LovyanGFX's drawWedgeLine sets a clip rectangle of its own and clears it when
// done. So the line is trimmed here to stay inside the current clip rectangle,
// and that rectangle is put back afterwards.
inline void wedge(G *g, float x0, float y0, float x1, float y1, float r0, float r1, uint32_t color)
{
    int32_t cx, cy, cw, ch;
    g->getClipRect(&cx, &cy, &cw, &ch);
    const bool clipped = cx > 0 || cy > 0 || cw < g->width() || ch < g->height();
    if (clipped) {
        // Liang-Barsky, against the clip rectangle shrunk by the line's radius.
        const float m = fmaxf(r0, r1) + 1.0f, dx = x1 - x0, dy = y1 - y0;
        const float p[4] = {-dx, dx, -dy, dy};
        const float q[4] = {x0 - (cx + m), (cx + cw - 1 - m) - x0, y0 - (cy + m), (cy + ch - 1 - m) - y0};
        float t0 = 0, t1 = 1;
        for (int i = 0; i < 4; i++) {
            if (p[i] == 0) {
                if (q[i] < 0) {
                    return;
                }
            } else {
                const float t = q[i] / p[i];
                if (p[i] < 0) {
                    t0 = fmaxf(t0, t);
                } else {
                    t1 = fminf(t1, t);
                }
            }
        }
        if (t0 > t1) {
            return;
        }
        const float ra = r0 + (r1 - r0) * t0, rb = r0 + (r1 - r0) * t1;
        x1 = x0 + dx * t1;
        y1 = y0 + dy * t1;
        x0 += dx * t0;
        y0 += dy * t0;
        r0 = ra;
        r1 = rb;
    }
    g->drawWedgeLine((int)lroundf(x0), (int)lroundf(y0), (int)lroundf(x1), (int)lroundf(y1), r0, r1, color);
    if (clipped) {
        g->setClipRect(cx, cy, cw, ch);
    }
}

// Round-capped line of the given stroke width.
inline void line(G *g, float x0, float y0, float x1, float y1, float width, uint32_t color)
{
    wedge(g, x0, y0, x1, y1, width * 0.5f, width * 0.5f, color);
}

// Stroked quadratic Bezier.
inline void quad(G *g, float x0, float y0, float cx, float cy, float x1, float y1, float width, uint32_t color)
{
    const int N = 6;
    float px = x0, py = y0;
    for (int i = 1; i <= N; i++) {
        float u = (float)i / N, v = 1.0f - u;
        float x = v * v * x0 + 2 * v * u * cx + u * u * x1;
        float y = v * v * y0 + 2 * v * u * cy + u * u * y1;
        line(g, px, py, x, y, width, color);
        px = x;
        py = y;
    }
}

}  // namespace gfxu
