#include "progress.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "deck.h"

#ifdef ESP_PLATFORM
#include <esp_log.h>
#include <esp_random.h>
#include <nvs.h>
#endif

namespace progress {

namespace {

constexpr int MAX_CARDS = 400;  // about 3 KB of the settings storage
constexpr int BOXES = 8;
constexpr uint16_t INTERVAL_DAYS[BOXES] = {0, 1, 2, 4, 8, 16, 32, 64};
constexpr uint32_t MAGIC = 0x31475250;  // "PRG1"

struct Entry {
    uint32_t key;  // hash of "<deck id>/<card id>"
    uint16_t due;  // day it should be asked again
    uint8_t box;
    uint8_t reserved;
};

struct Store {
    uint32_t magic;
    int32_t last_clock_day;  // to notice a clock that was reset
    int32_t shift;           // today = clock day + shift
    uint16_t sessions;
    uint16_t count;
    Entry cards[MAX_CARDS];
};

Store s_store;
int s_clock_day = -1;
bool s_dirty = false;

size_t stored_size() { return offsetof(Store, cards) + s_store.count * sizeof(Entry); }

uint32_t key_of(int deck, int card)
{
    uint32_t h = 2166136261u;  // FNV-1a
    auto add = [&h](const char *s) {
        for (; *s; s++) {
            h = (h ^ (uint8_t)*s) * 16777619u;
        }
    };
    add(deck_info(deck).id);
    add("/");
    add(deck_card_id(deck, card));
    return h;
}

Entry *find(uint32_t key)
{
    for (int i = 0; i < s_store.count; i++) {
        if (s_store.cards[i].key == key) {
            return &s_store.cards[i];
        }
    }
    return nullptr;
}

uint32_t random_number()
{
#ifdef ESP_PLATFORM
    return esp_random();
#else
    return (uint32_t)rand();
#endif
}

}  // namespace

void set_clock_day(int clock_day)
{
    s_clock_day = clock_day;
    if (clock_day < 0) {
        return;
    }
    // A clock that lost power starts again in the past: carry on from the last day seen.
    if (clock_day < s_store.last_clock_day) {
        s_store.shift += s_store.last_clock_day - clock_day;
    }
    if (clock_day != s_store.last_clock_day) {
        s_store.last_clock_day = clock_day;
        s_dirty = true;
    }
}

void clock_moved(int old_day, int new_day)
{
    if (old_day < 0 || new_day < 0) {
        return;
    }
    s_store.shift += old_day - new_day;
    s_store.last_clock_day = new_day;
    s_clock_day = new_day;
    s_dirty = true;
}

void load(int clock_day)
{
    memset(&s_store, 0, sizeof(s_store));
    s_store.magic = MAGIC;
    s_store.last_clock_day = clock_day < 0 ? 0 : clock_day;
#ifdef ESP_PLATFORM
    nvs_handle_t h;
    if (nvs_open("watch", NVS_READONLY, &h) == ESP_OK) {
        static Store saved;
        size_t size = sizeof(saved);
        if (nvs_get_blob(h, "progress", &saved, &size) == ESP_OK && size >= offsetof(Store, cards) && saved.magic == MAGIC &&
            saved.count <= MAX_CARDS && size == offsetof(Store, cards) + saved.count * sizeof(Entry)) {
            memcpy(&s_store, &saved, size);
        }
        nvs_close(h);
    }
#endif
    s_dirty = false;
    set_clock_day(clock_day);
}

void save()
{
    if (!s_dirty) {
        return;
    }
#ifdef ESP_PLATFORM
    nvs_handle_t h;
    if (nvs_open("watch", NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_set_blob(h, "progress", &s_store, stored_size()) != ESP_OK || nvs_commit(h) != ESP_OK) {
            ESP_LOGW("progress", "could not be saved");
        }
        nvs_close(h);
    }
#endif
    s_dirty = false;
}

int today() { return s_clock_day < 0 ? s_store.sessions : s_clock_day + s_store.shift; }

int known()
{
    int n = 0;
    for (int d = 0; d < deck_count(); d++) {
        for (int c = 0; c < deck_info(d).cards; c++) {
            n += find(key_of(d, c)) != nullptr;
        }
    }
    return n;
}

void mark_seen(int deck, int card)
{
    const uint32_t key = key_of(deck, card);
    if (find(key) != nullptr || s_store.count >= MAX_CARDS) {
        return;
    }
    s_store.cards[s_store.count++] = {key, (uint16_t)today(), 0, 0};
    s_dirty = true;
}

int pick_session(Pick *out, int max)
{
    struct Candidate {
        Pick pick;
        int due, box;
    };
    static Candidate all[MAX_CARDS];
    int n = 0;
    for (int d = 0; d < deck_count() && d < 256; d++) {
        for (int c = 0; c < deck_info(d).cards && c < 256 && n < MAX_CARDS; c++) {
            if (const Entry *e = find(key_of(d, c))) {
                all[n++] = {{(unsigned char)d, (unsigned char)c}, e->due, e->box};
            }
        }
    }
    // Longest overdue first; among those, the less well known.
    qsort(all, n, sizeof(Candidate), [](const void *a, const void *b) {
        const Candidate *x = (const Candidate *)a, *y = (const Candidate *)b;
        return x->due != y->due ? x->due - y->due : x->box - y->box;
    });
    n = n < max ? n : max;
    for (int i = 0; i < n; i++) {
        out[i] = all[i].pick;
    }
    for (int i = n - 1; i > 0; i--) {  // asked in a different order every time
        const int j = (int)(random_number() % (uint32_t)(i + 1));
        const Pick swap = out[i];
        out[i] = out[j];
        out[j] = swap;
    }
    return n;
}

void answer(int deck, int card, bool knew)
{
    Entry *e = find(key_of(deck, card));
    if (e == nullptr) {
        return;
    }
    const int now = today();
    if (!knew) {
        e->box = 0;
        e->due = (uint16_t)now;
    } else if (e->due <= now) {
        e->box = e->box + 1 < BOXES ? e->box + 1 : BOXES - 1;
        e->due = (uint16_t)(now + INTERVAL_DAYS[e->box]);
    }
    s_dirty = true;
}

void session_done()
{
    s_store.sessions++;
    s_dirty = true;
}

int box_of(int deck, int card)
{
    const Entry *e = find(key_of(deck, card));
    return e ? e->box : -1;
}

int due_of(int deck, int card)
{
    const Entry *e = find(key_of(deck, card));
    return e ? e->due : -1;
}

}  // namespace progress
