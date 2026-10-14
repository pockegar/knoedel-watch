#pragma once
// Plays MP3 clips from the content pack's audio file through the speaker.

#include <stdint.h>

#include "deck.h"

void audio_init(const uint8_t *pak);

// Plays the clips back to back, replacing whatever is playing.
void audio_play(const Clip *clips, int count);
void audio_stop();
