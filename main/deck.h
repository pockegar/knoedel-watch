#pragma once
// Card decks, read from the pack that tools/import_decks.py builds from
// content/<deck-id>/deck.json (see FORMAT.md). One deck pack holds German plus
// one locale; a content pack carries two of them.

#include <stddef.h>
#include <stdint.h>

#include "character.h"

struct Clip {
    uint32_t offset;
    uint32_t size;  // 0: this card has no such clip
};

enum GrammarKind : uint8_t { GRAMMAR_NONE, GRAMMAR_VERB, GRAMMAR_NOUN, GRAMMAR_PATTERN };
enum Kasus : uint8_t { KASUS_NONE, KASUS_AKK, KASUS_DAT, KASUS_GEN };

// Fixed grammar colours (theme.json, "dark" set), RGB888.
struct Theme {
    uint32_t gender[3];  // der, die, das
    uint32_t prefix, refl;
    uint32_t aux[2];     // haben, sein
    uint32_t kasus[3];   // Akk, Dat, Gen
};

struct DeckInfo {
    const char *id, *title, *title_local, *level;
    int cards;
    int episode;
    int character;  // character::find() of the deck's character
};

// Strings are UTF-8 with {{markup}} (FORMAT.md, "Inline markup"); a missing
// field is an empty string. The importer adds the roles der/die/das (article)
// and haben/sein (auxiliary) to the two that authors write, prefix and refl.
struct Card {
    const char *id;
    const char *lead, *headword, *gloss;       // face 1
    const char *grammar, *example, *translation;  // face 2
    const char *pattern3, *tip;                // face 3
    GrammarKind grammar_kind;
    Kasus kasus2, kasus3;                      // case chip of the pattern on face 2 / face 3
    uint32_t color, background;
    character::Anim anim;
    Clip clips[4];  // headword, face-2 grammar line, example, face-3 pattern

    bool has_face3() const { return tip[0] != 0 || pattern3[0] != 0; }
};

bool deck_init(const uint8_t *pack, size_t size);
const char *deck_locale();
const Theme &deck_theme();
int deck_count();
DeckInfo deck_info(int deck);
Card deck_card(int deck, int index);
const char *deck_card_id(int deck, int index);  // without resolving the rest of the card
