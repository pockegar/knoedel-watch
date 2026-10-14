#pragma once
// What the learner knows: spaced repetition for the practice mode.
//
// A card enters practice once it has been seen in a deck. Each card sits in a
// box; a right answer moves it up a box and it comes back after 1, 2, 4, ...
// 64 days, a wrong answer puts it back to the start. Progress is kept by card
// id, so it survives a change of content pack.
//
// Days are counted by the real-time clock, which only has to keep running: its
// date is never shown and need not be right. Without a usable clock, every
// finished session counts as a day.

namespace progress {

constexpr int SESSION_CARDS = 8;

struct Pick {
    unsigned char deck, card;
};

// `clock_day`: a day number from the clock, or -1 if there is none. Call at
// start-up and again before each session, so that a new day is noticed.
void load(int clock_day);
void set_clock_day(int clock_day);
// The clock was set to another date: today stays today.
void clock_moved(int old_day, int new_day);
void save();  // writes only if something changed

int today();
int known();  // cards of the current pack that have been seen

// Marks a card of the current pack as seen, so that practice may ask for it.
void mark_seen(int deck, int card);

// Up to `max` cards for one session, in the order to ask them: those that are
// due first, the rest filled with the ones due soonest. 0: nothing seen yet.
int pick_session(Pick *out, int max);

// Records an answer. Right answers only count when the card was due.
void answer(int deck, int card, bool knew);
void session_done();

// For tests: box (0..7) and due day of a card, -1 if it has not been seen.
int box_of(int deck, int card);
int due_of(int deck, int card);

}  // namespace progress
