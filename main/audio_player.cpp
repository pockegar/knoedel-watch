#include "audio_player.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

#include <atomic>

extern "C" {
#include "third_party/minimp3.h"
}

static const char *TAG = "audio";

// Clips are mono, 24 kHz (draft audio) or 22.05 kHz (final audio); the longest
// grammar line + sentence pair is under 10 s.
static constexpr size_t PCM_CAPACITY = 24000 * 14;
static constexpr int GAP_MS = 320;
static constexpr int MAX_CLIPS = 3;

struct Request {
    Clip clips[MAX_CLIPS];
    int count;
    uint32_t generation;
};

static const uint8_t *s_pak;
static QueueHandle_t s_queue;
static std::atomic<uint32_t> s_generation;
static int16_t *s_pcm;
static mp3dec_t s_decoder;
static int16_t s_frame[MINIMP3_MAX_SAMPLES_PER_FRAME];

// Decodes one clip to the end of s_pcm. Returns false if a newer request arrived.
static bool decode_clip(const Clip &clip, uint32_t generation, size_t &samples, int &rate, int &channels)
{
    const uint8_t *p = s_pak + clip.offset;
    int left = (int)clip.size;
    mp3dec_init(&s_decoder);
    while (left > 0) {
        if (generation != s_generation) {
            return false;
        }
        mp3dec_frame_info_t info;
        int n = mp3dec_decode_frame(&s_decoder, p, left, s_frame, &info);
        if (info.frame_bytes == 0) {
            break;
        }
        p += info.frame_bytes;
        left -= info.frame_bytes;
        if (n > 0) {
            size_t count = (size_t)n * info.channels;
            if (samples + count > PCM_CAPACITY) {
                break;
            }
            memcpy(s_pcm + samples, s_frame, count * sizeof(int16_t));
            samples += count;
            rate = info.hz;
            channels = info.channels;
        }
    }
    return true;
}

static void audio_task(void *)
{
    Request req;
    while (true) {
        xQueueReceive(s_queue, &req, portMAX_DELAY);
        M5.Speaker.stop();
        if (req.generation != s_generation || req.count == 0) {
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // let the mixer drop the old buffer before reusing it

        size_t samples = 0;
        int rate = 24000, channels = 1;
        bool current = true;
        for (int i = 0; i < req.count && current; i++) {
            if (i > 0) {
                size_t gap = (size_t)rate * channels * GAP_MS / 1000;
                if (samples + gap < PCM_CAPACITY) {
                    memset(s_pcm + samples, 0, gap * sizeof(int16_t));
                    samples += gap;
                }
            }
            current = decode_clip(req.clips[i], req.generation, samples, rate, channels);
        }
        if (current && samples > 0 && req.generation == s_generation) {
            M5.Speaker.playRaw(s_pcm, samples, rate, channels == 2, 1, 0, true);
        }
    }
}

void audio_init(const uint8_t *pak)
{
    s_pak = pak;
    s_pcm = (int16_t *)heap_caps_malloc(PCM_CAPACITY * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (s_pcm == nullptr) {
        ESP_LOGE(TAG, "no memory for the PCM buffer; audio disabled");
        return;
    }
    s_queue = xQueueCreate(1, sizeof(Request));
    // minimp3 keeps about 16 KB of scratch data on the stack while decoding.
    xTaskCreatePinnedToCore(audio_task, "audio", 32 * 1024, nullptr, 4, nullptr, 0);
}

void audio_play(const Clip *clips, int count)
{
    if (s_queue == nullptr) {
        return;
    }
    Request req = {};
    req.count = count > MAX_CLIPS ? MAX_CLIPS : count;
    for (int i = 0; i < req.count; i++) {
        req.clips[i] = clips[i];
    }
    req.generation = ++s_generation;
    xQueueOverwrite(s_queue, &req);
}

void audio_stop() { audio_play(nullptr, 0); }
