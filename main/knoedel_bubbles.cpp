// Thought-bubble icons, ported one by one from BUBBLE in tools/preview/knoedel.js
// (second half of the table) and from the Ep.4 web cards (first half).
// Every icon is drawn around the origin, inside about +-28 x +-22 units.

#include "gfx_util.h"
#include "knoedel.h"

using namespace gfxu;
using character::Stage;

namespace knoedel {

namespace {

constexpr float DEG = 3.14159265f / 180.0f;
constexpr uint32_t WHITE = 0xFFFFFF, OUTLINE = 0xC9D3DE;

// A tiny vector canvas: icon units in, screen pixels out.
struct Ico {
    G *g;
    const Stage *st;
    float ox, oy, k;

    float X(float x) const { return ox + x * k; }
    float Y(float y) const { return oy + y * k; }
    float W(float w) const { return fmaxf(1.2f, w * k); }

    void ln(float x0, float y0, float x1, float y1, float w, uint32_t c) const
    {
        line(g, X(x0), Y(y0), X(x1), Y(y1), W(w), c);
    }
    void ell(float cx, float cy, float rx, float ry, uint32_t c, float alpha = 1) const
    {
        ellipse(g, X(cx), Y(cy), rx * k, ry * k, c, alpha);
    }
    // Filled ellipse with an outline of about one unit.
    void ell_o(float cx, float cy, float rx, float ry, uint32_t fill, uint32_t stroke) const
    {
        ell(cx, cy, rx + 0.5f, ry + 0.5f, stroke);
        ell(cx, cy, rx - 0.5f, ry - 0.5f, fill);
    }
    void ring(float cx, float cy, float r, float w, uint32_t c) const
    {
        const int N = 20;
        for (int i = 0; i < N; i++) {
            float a0 = i * 2 * 3.14159265f / N, a1 = (i + 1) * 2 * 3.14159265f / N;
            ln(cx + cosf(a0) * r, cy + sinf(a0) * r, cx + cosf(a1) * r, cy + sinf(a1) * r, w, c);
        }
    }
    void rect(float x, float y, float w, float h, float r, uint32_t c) const
    {
        const int px = (int)lroundf(X(x)), py = (int)lroundf(Y(y));
        const int pw = (int)lroundf(X(x + w)) - px, ph = (int)lroundf(Y(y + h)) - py;
        if (r > 0) {
            g->fillSmoothRoundRect(px, py, pw, ph, (int)lroundf(r * k), c);
        } else {
            g->fillRect(px, py, pw, ph, c);
        }
    }
    // Convex polygon; `stroke_w` > 0 also outlines it.
    void poly(const float (*p)[2], int n, uint32_t fill, uint32_t stroke = 0, float stroke_w = 0) const
    {
        for (int i = 1; i + 1 < n; i++) {
            g->fillTriangle(X(p[0][0]), Y(p[0][1]), X(p[i][0]), Y(p[i][1]), X(p[i + 1][0]), Y(p[i + 1][1]), fill);
        }
        for (int i = 0; i < n; i++) {
            const int j = (i + 1) % n;
            ln(p[i][0], p[i][1], p[j][0], p[j][1], stroke_w > 0 ? stroke_w : 0.8f, stroke_w > 0 ? stroke : fill);
        }
    }
    void qd(float x0, float y0, float cx, float cy, float x1, float y1, float w, uint32_t c) const
    {
        quad(g, X(x0), Y(y0), X(cx), Y(cy), X(x1), Y(y1), W(w), c);
    }
    void cubic(float x0, float y0, float ax, float ay, float bx, float by, float x1, float y1, float w, uint32_t c) const
    {
        const int N = 9;
        float px = x0, py = y0;
        for (int i = 1; i <= N; i++) {
            float u = (float)i / N, v = 1 - u;
            float x = v * v * v * x0 + 3 * v * v * u * ax + 3 * v * u * u * bx + u * u * u * x1;
            float y = v * v * v * y0 + 3 * v * v * u * ay + 3 * v * u * u * by + u * u * u * y1;
            ln(px, py, x, y, w, c);
            px = x;
            py = y;
        }
    }
    void polyline(const float (*p)[2], int n, float w, uint32_t c) const
    {
        for (int i = 0; i + 1 < n; i++) {
            ln(p[i][0], p[i][1], p[i + 1][0], p[i + 1][1], w, c);
        }
    }
    // Rectangle with an outline of about one unit.
    void rect_o(float x, float y, float w, float h, float r, uint32_t fill, uint32_t stroke) const
    {
        rect(x - 0.6f, y - 0.6f, w + 1.2f, h + 1.2f, r > 0 ? r + 0.6f : 0, stroke);
        rect(x + 0.6f, y + 0.6f, w - 1.2f, h - 1.2f, r > 0.6f ? r - 0.6f : 0, fill);
    }
    void text(const lgfx::IFont *font, const char *str, float x, float y, uint32_t c) const
    {
        if (font == nullptr) {
            return;
        }
        g->setFont(font);
        g->setTextDatum(middle_center);
        g->setTextColor(c);
        g->drawString(str, (int)lroundf(X(x)), (int)lroundf(Y(y)));
    }
    void letter_z(float x, float baseline, float size, uint32_t c) const
    {
        const float a = size * 0.25f, cx = x + a, cy = baseline - a, w = size * 0.17f;
        ln(cx - a, cy - a, cx + a, cy - a, w, c);
        ln(cx + a, cy - a, cx - a, cy + a, w, c);
        ln(cx - a, cy + a, cx + a, cy + a, w, c);
    }
};

// ---- Ep.4, "Wie fühlst du dich?" ---------------------------------------------------

void b_house(const Ico &d)
{
    static const float ROOF[][2] = {{-17, 2}, {0, -14}, {17, 2}};
    d.rect(-13, 2, 26, 17, 0, 0xF2D7A6);
    d.poly(ROOF, 3, 0xE06A4E);
    d.rect(-4, 9, 8, 10, 0, 0x8E5F3A);
    d.rect(5, 5, 12, 3, 0, 0x8E5F3A);
    static const float HEART[][2] = {{11.6f, -13.8f}, {22.4f, -13.8f}, {17, -8.6f}};
    d.ell(14.3f, -15.2f, 3, 3, 0xF06C8B);
    d.ell(19.7f, -15.2f, 3, 3, 0xF06C8B);
    d.poly(HEART, 3, 0xF06C8B);
}

void b_sign_berlin(const Ico &d)
{
    d.ln(-8, -4, -8, 19, 2.4f, 0x7A5B43);
    static const float SIGN[][2] = {{-20, -17}, {15, -17}, {23, -9}, {15, -1}, {-20, -1}};
    d.poly(SIGN, 5, 0x3B6BB0);
    d.text(d.st->font_tiny, "Berlin", -2, -9, WHITE);
    d.rect(2, 5, 18, 14, 3, 0xC97B4A);
    static const float HANDLE[][2] = {{7, 5}, {7, 2}, {15, 2}, {15, 5}};
    d.polyline(HANDLE, 4, 1.6f, 0x8E5432);
}

void sheet(const Ico &d)
{
    d.rect_o(-14, -19, 28, 37, 2, WHITE, 0xC9CED6);
    for (float y = -11; y <= 1; y += 6) {
        d.ln(-9, y, 6, y, 1.6f, 0xC9CED6);
    }
}

void b_exam(const Ico &d)
{
    sheet(d);
    d.text(d.st->font_small, "?", 4, 9, 0x7A4FD6);
}

void b_exam_passed(const Ico &d)
{
    sheet(d);
    static const float CHECK[][2] = {{-6, 8}, {-1, 13}, {10, -1}};
    d.polyline(CHECK, 3, 4, 0x3E8E2F);
}

void b_beach(const Ico &d)
{
    d.ell(14, -11, 7, 7, 0xFFC93C);
    d.ln(-4, -8, -4, 16, 2, 0x7A5B43);
    float shade[9][2];
    for (int i = 0; i < 9; i++) {
        const float u = i / 8.0f, v = 1 - u;
        shade[i][0] = v * v * -18 + 2 * v * u * -4 + u * u * 10;
        shade[i][1] = 2 * v * u * -18;
    }
    d.poly(shade, 9, 0xE5484D);
    for (int i = 0; i < 4; i++) {
        const float x = -22 + i * 10;
        d.qd(x, 16, x + 5, (i & 1) ? 20 : 12, x + 10, 16, 2.4f, 0x4BA3D9);
    }
}

void b_calendar(const Ico &d)
{
    static const uint32_t CELLS[] = {0xF4A261, 0x7A9CC6, 0xC2413B, 0x9C7AD6};
    d.rect_o(-20, -17, 40, 35, 4, WHITE, 0xC9CED6);
    d.rect(-20, -17, 40, 8, 3, 0xC2413B);
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            d.rect(-17 + c * 9, -6 + r * 8, 7, 6, 0, CELLS[(r + c) % 4]);
        }
    }
}

void b_heart_broken(const Ico &d)
{
    // Each half is a fan around the bottom tip, along the two curves of the outline.
    for (int side = -1; side <= 1; side += 2) {
        float p[18][2] = {{0, 16}};
        int n = 1;
        static const float C[2][8] = {{0, 16, -16, 4, -20, -6, -12, -12}, {-12, -12, -7, -16, -2, -12, 0, -8}};
        for (auto &c : C) {
            for (int i = 1; i <= 8; i++) {
                const float u = i / 8.0f, v = 1 - u;
                p[n][0] = side * -(v * v * v * c[0] + 3 * v * v * u * c[2] + 3 * v * u * u * c[4] + u * u * u * c[6]);
                p[n][1] = v * v * v * c[1] + 3 * v * v * u * c[3] + 3 * v * u * u * c[5] + u * u * u * c[7];
                n++;
            }
        }
        d.poly(p, n, 0xE5484D);
    }
    static const float CRACK[][2] = {{0, -8}, {-3, -1}, {2, 3}, {-2, 9}, {0, 16}};
    d.polyline(CRACK, 5, 2.4f, WHITE);
}

void b_no_mail(const Ico &d)
{
    d.rect_o(-16, -11, 32, 22, 3, WHITE, 0x7A8696);
    static const float FLAP[][2] = {{-15, -9}, {0, 3}, {15, -9}};
    d.polyline(FLAP, 3, 1.6f, 0x7A8696);
    d.ring(0, 0, 17, 3, 0xD23A2E);
    d.ln(-12, 12, 12, -12, 3, 0xD23A2E);
}

void b_clock_late(const Ico &d)
{
    d.ell(-4, 0, 17, 17, 0x3A4554);
    d.ell(-4, 0, 15, 15, WHITE);
    d.ln(-4, 0, -4, -11, 2.4f, 0x3A4554);
    d.ln(-4, 0, 4, 4, 2.4f, 0xD23A2E);
    d.text(d.st->font_small, "!", 21, -10, 0xD23A2E);
}

void b_greeting(const Ico &d)
{
    d.text(d.st->font_tiny, "Grüß Gott!", 0, -8, 0xC23D7A);
    d.text(d.st->font_tiny, "Wie geht's?", 0, 8, 0x5F6B7D);
}

void b_phone_ok(const Ico &d)
{
    d.rect(-10, -19, 20, 37, 4, 0x2B3340);
    d.rect(-7, -14, 14, 25, 1.5f, 0xDDF3D2);
    static const float CHECK[][2] = {{-4, -1}, {-1, 3}, {5, -6}};
    d.polyline(CHECK, 3, 2.4f, 0x3E8E2F);
}

void b_noise(const Ico &d)
{
    d.text(d.st->font_small, "BRRRR", 0, 4, 0x7A7A2A);
    static const float L[][2] = {{-28, -20}, {-23, -16}, {-27, -13}, {-21, -9}};
    static const float R[][2] = {{28, -20}, {23, -16}, {27, -13}, {21, -9}};
    d.polyline(L, 4, 1.8f, 0x7A7A2A);
    d.polyline(R, 4, 1.8f, 0x7A7A2A);
}

// ---- a2-gesundheit-zustaende ---------------------------------------------------------

void b_bed(const Ico &d)
{
    d.rect(-27, -18, 6, 30, 2, 0x8C5B3A);
    d.rect(-24, 2, 50, 8, 2, 0x8C5B3A);
    d.rect(-21, -6, 46, 9, 3, 0xEFE8DC);
    d.ell_o(-13, -8, 7, 4, WHITE, 0xD7CBB8);
    static const float BLANKET[][2] = {{-5, -11}, {2.5f, -12.1f}, {10, -12.3f}, {17.5f, -11.3f}, {25, -9}, {25, 2}, {-5, 2}};
    d.poly(BLANKET, 7, 0x8FB3D9);
    d.letter_z(10, -14, 10, 0x6B5FA8);
}

void b_coffee_cups(const Ico &d)
{
    for (float x = -12; x <= 12; x += 24) {
        d.qd(x - 2, -8, x - 5, -12, x - 2, -16, 1.5f, OUTLINE);
        d.qd(x + 3, -8, x, -12, x + 3, -16, 1.5f, OUTLINE);
        d.qd(x + 7.5f, -1, x + 12.5f, -1, x + 12.5f, 3, 1.6f, OUTLINE);
        d.qd(x + 12.5f, 3, x + 12.5f, 7, x + 6.9f, 7, 1.6f, OUTLINE);
        const float cup[][2] = {{x - 8, -4}, {x + 8, -4}, {x + 6.5f, 12}, {x - 6.5f, 12}};
        d.poly(cup, 4, WHITE, OUTLINE, 1);
        d.rect(x - 7.3f, -3.5f, 14.6f, 3, 0, 0x6B3E22);
    }
}

void b_brezel(const Ico &d)
{
    const uint32_t c = 0xA55A1F;
    d.cubic(-15, 9, -26, 0, -21, -15, -8, -14, 5.5f, c);
    d.cubic(-8, -14, 1, -13, 4, -3, 0, 5, 5.5f, c);
    d.cubic(0, 5, -4, -3, -1, -13, 8, -14, 5.5f, c);
    d.cubic(8, -14, 21, -15, 26, 0, 15, 9, 5.5f, c);
    d.qd(-15, 9, 0, 2, 15, 9, 5.5f, c);
    static const float SALT[][2] = {{-16, -5}, {-7, -11}, {7, -11}, {16, -5}, {0, 6}, {-10, 5}, {10, 5}};
    for (auto &s : SALT) {
        d.ell(s[0], s[1], 1.1f, 1.1f, WHITE);
    }
}

void b_water_glass(const Ico &d)
{
    static const float GLASS[][2] = {{-11, -16}, {11, -16}, {8, 15}, {-8, 15}};
    static const float WATER[][2] = {{-9.5f, -2}, {9.5f, -2}, {8, 15}, {-8, 15}};
    d.poly(GLASS, 4, 0xEEF7FD, 0x9CC3E6, 1.4f);
    d.poly(WATER, 4, 0x7DB8E2);
    wedge(d.g, d.X(19), d.Y(-13.4f), d.X(19), d.Y(-5), 0.5f * d.k, 3.6f * d.k, 0x7DB8E2);
}

void b_plate_empty(const Ico &d)
{
    d.ell_o(0, 4, 25, 10, WHITE, 0xC9CED6);
    d.ell_o(0, 4, 15, 5.5f, WHITE, 0xE3E7EC);
    static const float CRUMBS[][2] = {{-6, 3}, {4, 6}, {8, 2}};
    for (auto &c : CRUMBS) {
        d.ell(c[0], c[1], 1.3f, 1.3f, 0xC8955A);
    }
    d.ln(-7, -12, -2, -7, 3, 0x3E8E2F);
    d.ln(-2, -7, 8, -17, 3, 0x3E8E2F);
}

// A capsule: one half coloured, the other white with a thin outline.
void capsule(const Ico &d, float x, float y, float rot_deg, uint32_t color)
{
    const float cs = cosf(rot_deg * DEG), sn = sinf(rot_deg * DEG);
    auto at = [&](float lx, float ly, float &px, float &py) {
        px = x + lx * cs - ly * sn;
        py = y + lx * sn + ly * cs;
    };
    float ax, ay, bx, by, mx, my;
    at(-7, 0, ax, ay);
    at(7, 0, bx, by);
    at(-0.5f, 0, mx, my);
    d.ln(ax, ay, bx, by, 11.2f, OUTLINE);
    d.ln(ax, ay, bx, by, 9.6f, WHITE);
    d.ln(ax, ay, ax, ay, 10.4f, color);
    float p[4][2];
    at(-7, -5.2f, p[0][0], p[0][1]);
    at(0, -5.2f, p[1][0], p[1][1]);
    at(0, 5.2f, p[2][0], p[2][1]);
    at(-7, 5.2f, p[3][0], p[3][1]);
    d.poly(p, 4, color);
}

void b_pills(const Ico &d)
{
    capsule(d, -8, -6, -25, 0xE5484D);
    capsule(d, 9, 7, 20, 0x4BA3D9);
    d.ell_o(14, -10, 6, 6, WHITE, OUTLINE);
    d.ln(9.5f, -10, 18.5f, -10, 1, OUTLINE);
}

void b_apple(const Ico &d)
{
    d.ell(-6, 3, 12, 12, 0xE5484D);
    d.ell(6, 3, 12, 12, 0xE5484D);
    d.qd(0, -9, 1, -16, 4, -18, 2, 0x6B4423);
    d.ln(6.1f, -12.6f, 11.9f, -15.4f, 6, 0x5E9E45);
    d.ell(-9, -2, 3, 5, WHITE, 0.35f);
}

void b_tissues(const Ico &d)
{
    d.cubic(-5, -2, -9, -12, -3, -18, 0, -13, 1.2f, 0xAEBBCB);
    d.cubic(0, -13, 3, -19, 10, -13, 6, -2, 1.2f, 0xAEBBCB);
    d.rect(-18, -3, 36, 18, 2, 0x8FB3D9);
    d.ell(0, -3, 8, 2, 0x6F93BA);
    d.ln(-17, 9, 17, 9, 1.2f, 0x7AA0C9);
}

void b_bike(const Ico &d)
{
    const uint32_t frame = 0xD9532F, dark = 0x3A4554;
    d.ring(-14, 6, 9, 2.2f, dark);
    d.ring(14, 6, 9, 2.2f, dark);
    static const float F[][4] = {{-14, 6, -3, 6}, {-3, 6, 6, -6}, {6, -6, -6, -6}, {-6, -6, -14, 6},
                                 {-3, 6, -8, -10}, {6, -6, 14, 6}, {5, -9, 9, -12}};
    for (auto &s : F) {
        d.ln(s[0], s[1], s[2], s[3], 2.4f, frame);
    }
    d.ln(-11, -10, -5, -10, 2.6f, dark);
}

void b_window_open(const Ico &d)
{
    const uint32_t frame = 0x9AA4B1;
    d.rect(-21, -19, 28, 36, 0, frame);
    d.rect(-19, -17, 24, 32, 0, 0xCFE8F7);
    static const float SASH[][2] = {{6, -18}, {20, -12}, {20, 22}, {6, 16}};
    d.poly(SASH, 4, 0xEAF5FC, frame, 2);
    for (float y = -6; y <= 4; y += 10) {
        d.qd(-16, y, -11, y - 3, -6, y, 1.8f, WHITE);
        d.qd(-6, y, -1, y + 3, 4, y, 1.8f, WHITE);
    }
    const uint32_t flake = 0x7DB8E2;
    d.ln(-8, -12, -8, -4, 1.3f, flake);
    d.ln(-12, -8, -4, -8, 1.3f, flake);
    d.ln(-11, -11, -5, -5, 1.3f, flake);
    d.ln(-11, -5, -5, -11, 1.3f, flake);
}

void b_heater(const Ico &d)
{
    for (int i = 0; i < 5; i++) {
        d.rect(-19.5f + i * 8, -6.5f, 7.5f, 23, 3, 0xB5BDC8);
        d.rect(-18.6f + i * 8, -5.6f, 5.7f, 21.2f, 2.5f, 0xE3E7EC);
    }
    for (float x = -10; x <= 10; x += 10) {
        d.qd(x, -9, x - 3, -13, x, -17, 1.8f, 0xF28C28);
        d.qd(x, -17, x + 3, -21, x, -25, 1.8f, 0xF28C28);
    }
}

void b_cake(const Ico &d)
{
    d.ln(-24, 14, 24, 14, 2, 0xC9CED6);
    static const float SLICE[][2] = {{-18, 12}, {18, 12}, {18, -2}, {-18, -10}};
    d.poly(SLICE, 4, 0xF6D7A7, 0xD9B27A, 1);
    d.ln(-17, 2, 17, 6, 2.4f, 0xF2A0B4);
    d.ln(-18, -10, 18, -2, 3.4f, 0xD9B27A);
    d.ln(-18, -10, 18, -2, 2.2f, WHITE);
    d.ell(4, -9, 4, 4, 0xE5484D);
}

// ---- a2-charakter, "Wie bist du?" --------------------------------------------------

void b_clock_ok(const Ico &d)
{
    d.ell(-4, 0, 16, 16, 0x3A4554);
    d.ell(-4, 0, 14, 14, WHITE);
    d.ln(-4, 0, -4, -10, 2.4f, 0x3A4554);
    d.ln(-4, 0, -4, 9, 2.4f, 0x3A4554);
    static const float CHECK[][2] = {{12, -2}, {16, 3}, {24, -8}};
    d.polyline(CHECK, 3, 3, 0x3E8E2F);
}

void b_books(const Ico &d)
{
    d.rect(-11, -20, 22, 7, 1.5f, 0x2F7DB8);
    d.rect(-10, -12, 20, 7, 1.5f, 0xE5484D);
    d.rect(-12, -4, 24, 7, 1.5f, 0xF2C14E);
    d.rect(-14, 4, 28, 8, 1.5f, 0x5E9E45);
    // A bookmark ribbon with a notch.
    static const float A[][2] = {{10, -22}, {24, -22}, {24, 2}, {17, -3}}, B[][2] = {{10, -22}, {17, -3}, {10, 2}};
    static const float EDGE[][2] = {{10, -22}, {24, -22}, {24, 2}, {17, -3}, {10, 2}, {10, -22}};
    d.poly(A, 4, WHITE);
    d.poly(B, 3, WHITE);
    d.polyline(EDGE, 6, 1, 0x9AA4B1);
}

void b_sofa(const Ico &d)
{
    d.rect(-24, -10, 48, 16, 6, 0xB07A4E);
    d.rect(-28, -2, 10, 16, 4, 0xC88D5E);
    d.rect(18, -2, 10, 16, 4, 0xC88D5E);
    d.rect(-19, 2, 38, 10, 3, 0xD9A374);
    d.rect(-24, 13, 4, 5, 0, 0x6B4423);
    d.rect(20, 13, 4, 5, 0, 0x6B4423);
}

void b_shelf(const Ico &d)
{
    static const uint32_t BOOKS[5] = {0x2F7DB8, 0xE5484D, 0xF2C14E, 0x5E9E45, 0x9C7AD6};
    for (int i = 0; i < 5; i++) {
        d.rect(-20 + i * 6, -15, 5, 13, 0, BOOKS[i]);
    }
    d.rect(6, 5, 14, 10, 1.5f, 0xC9976A);
    d.ell(-12, 10, 5, 5, 0x5E9E45);
    static const float FRAME[][2] = {{-24, -20}, {24, -20}, {24, 18}, {-24, 18}, {-24, -20}};
    d.polyline(FRAME, 5, 2.4f, 0x8C5B3A);
    d.ln(-24, -1, 24, -1, 2.4f, 0x8C5B3A);
}

void b_keys(const Ico &d)
{
    d.ring(-10, -4, 8, 3, 0xE0A526);
    d.ln(-3, 0, 18, 10, 3, 0xE0A526);
    d.ln(12, 7, 10, 12, 3, 0xE0A526);
    d.ln(16, 9, 14, 14, 3, 0xE0A526);
    d.text(d.st->font_small, "?", 15, -13, 0xC2553A);
}

void b_joke(const Ico &d)
{
    static const float TAIL[][2] = {{-7, 9}, {-16, 18}, {-16, 9}};
    d.rect_o(-28, -16, 56, 26, 6, 0xFFEBCB, 0xD9820F);
    d.poly(TAIL, 3, 0xFFEBCB);
    d.ln(-7, 10, -16, 18, 1.2f, 0xD9820F);
    d.ln(-16, 18, -16, 10, 1.2f, 0xD9820F);
    d.text(d.st->font_tiny, "Witz!", 0, -3, 0xD9820F);
}

void b_tie(const Ico &d)
{
    static const float SHIRT_L[][2] = {{-16, -18}, {0, -8}, {0, 18}, {-16, 18}}, SHIRT_R[][2] = {{0, -8}, {16, -18}, {16, 18}, {0, 18}};
    static const float KNOT[][2] = {{-5, -12}, {5, -12}, {3, -6}, {-3, -6}}, BLADE[][2] = {{-3, -6}, {3, -6}, {6, 14}, {0, 19}, {-6, 14}};
    static const float COLLAR_L[][2] = {{-16, -18}, {0, -8}, {-6, -2}}, COLLAR_R[][2] = {{16, -18}, {0, -8}, {6, -2}};
    d.poly(SHIRT_L, 4, 0x4A5875);
    d.poly(SHIRT_R, 4, 0x4A5875);
    d.poly(KNOT, 4, 0xC2415A);
    d.poly(BLADE, 5, 0xC2415A);
    d.poly(COLLAR_L, 3, WHITE);
    d.poly(COLLAR_R, 3, WHITE);
}

// An open book: each page in three strips between its curved top and bottom edge.
void b_book(const Ico &d)
{
    static const float TOP[4][2] = {{0, -12}, {8, -14.6f}, {16, -14.9f}, {24, -12}}, BOTTOM[4][2] = {{0, 14}, {8, 11.4f}, {16, 11.1f}, {24, 14}};
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 3; i++) {
            const float strip[4][2] = {{side * TOP[i][0], TOP[i][1]}, {side * TOP[i + 1][0], TOP[i + 1][1]},
                                       {side * BOTTOM[i + 1][0], BOTTOM[i + 1][1]}, {side * BOTTOM[i][0], BOTTOM[i][1]}};
            d.poly(strip, 4, WHITE);
        }
        for (int i = 0; i < 3; i++) {
            d.ln(side * TOP[i][0], TOP[i][1], side * TOP[i + 1][0], TOP[i + 1][1], 1.6f, 0x5B7FB0);
            d.ln(side * BOTTOM[i][0], BOTTOM[i][1], side * BOTTOM[i + 1][0], BOTTOM[i + 1][1], 1.6f, 0x5B7FB0);
        }
        d.ln(side * 24, -12, side * 24, 14, 1.6f, 0x5B7FB0);
        for (float y = -6; y <= 4; y += 5) {
            d.ln(side * 19, y, side * 5, y - 1, 1, 0xC9D3DE);
        }
    }
    d.ln(0, -12, 0, 14, 1.6f, 0x5B7FB0);
}

void b_speaker(const Ico &d)
{
    d.rect(-14, -18, 22, 36, 4, 0x3A4554);
    d.ell(-3, 6, 7, 7, 0x59606C);
    d.ell(-3, -9, 4, 4, 0x59606C);
    d.qd(13, -8, 18, 0, 13, 8, 2, 0xC23D7A);
    d.qd(18, -13, 26, 0, 18, 13, 2, 0xC23D7A);
}

void b_handshake(const Ico &d)
{
    static const float LEFT[][2] = {{-26, -2}, {-10, -10}, {4, -4}, {-6, 4}}, RIGHT[][2] = {{26, -2}, {10, -10}, {-4, -2}, {2, 8}, {10, 4}};
    static const float HEART[][2] = {{-6, -16.5f}, {6, -16.5f}, {0, -11}};
    d.poly(LEFT, 4, 0xF3C9A2, 0xC99A75, 1);
    d.poly(RIGHT, 5, 0xE2B48A, 0xB88A62, 1);
    d.rect(-30, -6, 7, 12, 2, 0x4A7FB0);
    d.rect(23, -6, 7, 12, 2, 0xD9532F);
    d.ell(-3, -18, 3.2f, 3.2f, 0xF06C8B);
    d.ell(3, -18, 3.2f, 3.2f, 0xF06C8B);
    d.poly(HEART, 3, 0xF06C8B);
}

// Three people, the one in the middle a little taller.
void b_friends(const Ico &d)
{
    static const uint32_t SHIRTS[3] = {0xE5484D, 0xF2C14E, 0x4BA3D9};
    for (int i = 0; i < 3; i++) {
        const float x = -14 + i * 14, top = i == 1 ? -2 : 1;
        d.ell(x, -8 + (i == 1 ? -3 : 0), 6, 6, 0xF3C9A2);
        d.ell(x, top + 8, 8, 8, SHIRTS[i]);
        d.rect(x - 8, top + 8, 16, 8 - top, 0, SHIRTS[i]);
    }
}

// ---- a2-reaktionen ------------------------------------------------------------

void b_lightbulb(const Ico &d)
{
    static const float RAYS[5][4] = {{-20, -14, -26, -18}, {20, -14, 26, -18}, {0, -24, 0, -30}, {-18, 2, -24, 3}, {18, 2, 24, 3}};
    for (auto &r : RAYS) {
        d.ln(r[0], r[1], r[2], r[3], 2, 0xF2C14E);
    }
    d.rect(-6, 5, 12, 8, 2, 0x9AA4B1);
    d.ell_o(0, -6, 12, 12, 0xFFE27A, 0xE0B020);
    static const float FILAMENT[3][2] = {{-3, -6}, {0, 0}, {3, -6}};
    d.polyline(FILAMENT, 3, 1.4f, 0xE0B020);
}

// A bowl of chicken soup, steaming.
void b_soup(const Ico &d)
{
    // Two quadratic curves, from (22, -2) round the bottom to (-22, -2).
    float bowl[11][2];
    for (int i = 0; i <= 10; i++) {
        const float t = i < 5 ? i / 5.0f : (i - 5) / 5.0f, p = (1 - t) * (1 - t), q = 2 * (1 - t) * t, r = t * t;
        bowl[i][0] = i < 5 ? p * 22 + q * 20 : q * -20 + r * -22;
        bowl[i][1] = i < 5 ? p * -2 + q * 18 + r * 18 : p * 18 + q * 18 + r * -2;
    }
    d.poly(bowl, 11, WHITE, 0xC9CED6, 1.6f);
    d.ell(0, -2, 22, 4, 0xF6C356);
    d.ell(-8, -2, 2, 2, 0xE5873A);
    d.ell(6, -3, 2, 2, 0xE5873A);
    for (float x = -8; x <= 8; x += 8) {
        d.qd(x, -10, x - 3, -14, x, -18, 1.8f, OUTLINE);
    }
}

// A bunch of three flowers in a paper wrap.
void b_flowers(const Ico &d)
{
    d.ln(-6, 18, -2, -4, 2, 0x5E9E45);
    d.ln(2, 18, 4, -8, 2, 0x5E9E45);
    d.ln(8, 18, 12, -2, 2, 0x5E9E45);
    static const float HEADS[3][2] = {{-2, -6}, {4, -12}, {12, -5}};
    static const uint32_t COLOURS[3] = {0xF06C8B, 0xF2C14E, 0x9C7AD6};
    for (int i = 0; i < 3; i++) {
        for (int k = 0; k < 5; k++) {
            const float a = k * 72 * DEG;
            d.ell(HEADS[i][0] + sinf(a) * 3.2f, HEADS[i][1] - cosf(a) * 3.2f, 2.6f, 2.6f, COLOURS[i]);
        }
        d.ell(HEADS[i][0], HEADS[i][1], 2, 2, 0xFFF1C4);
    }
    static const float WRAP[4][2] = {{-12, 8}, {18, 8}, {10, 20}, {-4, 20}};
    d.poly(WRAP, 4, 0xBFE3F2, 0x7FA6C4, 1.4f);
}

// A four-leaf clover: each leaf a heart, two circles and a point.
void b_clover(const Ico &d)
{
    d.qd(2, 4, 6, 14, 12, 18, 2.2f, 0x3E7A2F);
    for (int k = 0; k < 4; k++) {
        const float a = k * 90 * DEG, cs = cosf(a), sn = sinf(a);
        auto rx = [&](float x, float y) { return x * cs - (y - 1) * sn; };
        auto ry = [&](float x, float y) { return x * sn + (y - 1) * cs; };
        d.ell(rx(-5, -13), ry(-5, -13), 5.2f, 5.2f, 0x5E9E45);
        d.ell(rx(5, -13), ry(5, -13), 5.2f, 5.2f, 0x5E9E45);
        const float leaf[3][2] = {{rx(-9.6f, -11), ry(-9.6f, -11)}, {rx(9.6f, -11), ry(9.6f, -11)}, {rx(0, 0), ry(0, 0)}};
        d.poly(leaf, 3, 0x5E9E45);
    }
}

// `scale`: drawing units per bubble unit. The Ep.4 icons were drawn for a
// slightly larger bubble.
struct BubbleDef {
    const char *id;
    void (*fn)(const Ico &);
    float scale;
};
constexpr float OLD = 0.78f, NEW = 0.85f;
const BubbleDef BUBBLES[] = {
    {"house", b_house, OLD}, {"sign-berlin", b_sign_berlin, OLD}, {"exam", b_exam, OLD}, {"beach", b_beach, OLD},
    {"calendar", b_calendar, OLD}, {"heart-broken", b_heart_broken, OLD}, {"no-mail", b_no_mail, OLD},
    {"clock-late", b_clock_late, OLD}, {"exam-passed", b_exam_passed, OLD}, {"greeting", b_greeting, 1.0f},
    {"phone-ok", b_phone_ok, OLD}, {"noise", b_noise, OLD},
    {"bed", b_bed, NEW}, {"coffee-cups", b_coffee_cups, NEW}, {"brezel", b_brezel, NEW}, {"water-glass", b_water_glass, NEW},
    {"plate-empty", b_plate_empty, NEW}, {"pills", b_pills, NEW}, {"apple", b_apple, NEW}, {"tissues", b_tissues, NEW},
    {"bike", b_bike, NEW}, {"window-open", b_window_open, NEW}, {"heater", b_heater, NEW}, {"cake", b_cake, NEW},
    {"clock-ok", b_clock_ok, NEW}, {"books", b_books, NEW}, {"sofa", b_sofa, NEW}, {"shelf", b_shelf, NEW},
    {"keys", b_keys, NEW}, {"joke", b_joke, NEW}, {"tie", b_tie, NEW}, {"book", b_book, NEW},
    {"speaker", b_speaker, NEW}, {"handshake", b_handshake, NEW}, {"friends", b_friends, NEW},
    {"lightbulb", b_lightbulb, NEW}, {"soup", b_soup, NEW}, {"flowers", b_flowers, NEW}, {"clover", b_clover, NEW},
};
constexpr int BUBBLE_COUNT = sizeof(BUBBLES) / sizeof(BUBBLES[0]);

}  // namespace

int bubble_count() { return BUBBLE_COUNT; }
const char *bubble_name(int index) { return BUBBLES[index].id; }

void draw_bubble_icon(G *g, const Stage &stage, int id, float x, float y, float unit)
{
    if (id >= 0 && id < BUBBLE_COUNT) {
        BUBBLES[id].fn(Ico{g, &stage, x, y, unit * BUBBLES[id].scale});
    }
}

}  // namespace knoedel
