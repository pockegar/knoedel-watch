#pragma once
// Knödel, the dumpling mascot: the character of the adjective and expression
// decks. Drawing reference: tools/preview/knoedel.js.

#include "character.h"

namespace knoedel {

extern const character::Impl IMPL;

// knoedel_bubbles.cpp: the icon inside a thought bubble, centred on (x, y).
// `unit` is the size in pixels of one unit of the 64 x 52 bubble.
void draw_bubble_icon(lgfx::LovyanGFX *g, const character::Stage &stage, int id, float x, float y, float unit);
int bubble_count();
const char *bubble_name(int index);

}  // namespace knoedel
