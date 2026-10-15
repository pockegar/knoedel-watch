#include "M5Unified.h"
#include "board_pins.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <driver/i2s_std.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {
constexpr uint32_t rate = 24000;
constexpr size_t chunk = 256;
struct Playback {
    int16_t *pcm;
    size_t count;
    uint32_t generation;
    uint32_t repeat;
};
QueueHandle_t queue = nullptr;
SemaphoreHandle_t control = nullptr;
esp_codec_dev_handle_t codec = nullptr;
std::atomic<uint32_t> generation{0};
std::atomic<uint8_t> volume{128};

void clear_pending() {
    Playback old;
    while (xQueueReceive(queue, &old, 0) == pdTRUE)
        free(old.pcm);
}
void worker(void *) {
    Playback req;
    int16_t pcm[chunk];
    for (;;) {
        xQueueReceive(queue, &req, portMAX_DELAY);
        if (req.generation != generation.load()) {
            free(req.pcm);
            continue;
        }
        ESP_LOGI("ws-audio", "play %u samples at %uHz", unsigned(req.count), unsigned(rate));
        gpio_set_level(WS_PA_EN, 1);
        esp_codec_dev_set_out_mute(codec, false);
        bool ok = true;
        for (uint32_t r = 0; r < req.repeat && req.generation == generation.load() && ok; ++r) {
            for (size_t off = 0; off < req.count && req.generation == generation.load(); off += chunk) {
                size_t n = std::min(chunk, req.count - off);
                int gain = volume.load();
                for (size_t i = 0; i < n; ++i)
                    pcm[i] = int32_t(req.pcm[off + i]) * gain / 255;
                if (esp_codec_dev_write(codec, pcm, n * sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
                    ok = false;
                    break;
                }
            }
        }
        // Drain the final DMA samples before muting the amplifier.
        std::fill(std::begin(pcm), std::end(pcm), 0);
        for (int i = 0; i < 4; ++i)
            esp_codec_dev_write(codec, pcm, sizeof(pcm));
        esp_codec_dev_set_out_mute(codec, true);
        gpio_set_level(WS_PA_EN, 0);
        ESP_LOGI("ws-audio", "%s",
                 !ok                                   ? "write failed"
                 : req.generation == generation.load() ? "complete"
                                                       : "cancelled");
        free(req.pcm);
    }
}
} // namespace
namespace ws {
bool speaker_begin() {
    gpio_reset_pin(WS_PA_EN);
    gpio_set_direction(WS_PA_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(WS_PA_EN, 0);
    i2s_chan_handle_t tx = nullptr;
    i2s_chan_config_t cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    cfg.auto_clear = true;
    cfg.dma_desc_num = 4;
    cfg.dma_frame_num = 256;
    if (i2s_new_channel(&cfg, &tx, nullptr) != ESP_OK)
        return false;
    i2s_std_config_t standard = {};
    standard.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
    standard.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    standard.gpio_cfg.mclk = WS_I2S_MCLK;
    standard.gpio_cfg.bclk = WS_I2S_BCLK;
    standard.gpio_cfg.ws = WS_I2S_WS;
    standard.gpio_cfg.dout = WS_I2S_DOUT;
    standard.gpio_cfg.din = GPIO_NUM_NC;
    if (i2s_channel_init_std_mode(tx, &standard) != ESP_OK || i2s_channel_enable(tx) != ESP_OK)
        return false;
    audio_codec_i2s_cfg_t data = {};
    data.port = I2S_NUM_1;
    data.tx_handle = tx;
    audio_codec_i2c_cfg_t ctrl = {};
    ctrl.port = I2C_NUM_0;
    ctrl.addr = ES8311_CODEC_DEFAULT_ADDR;
    ctrl.bus_handle = i2c_bus();
    es8311_codec_cfg_t es = {};
    es.ctrl_if = audio_codec_new_i2c_ctrl(&ctrl);
    es.gpio_if = audio_codec_new_gpio();
    es.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    es.pa_pin = -1;
    es.master_mode = false;
    es.use_mclk = true;
    es.hw_gain.pa_voltage = 5.0;
    es.hw_gain.codec_dac_voltage = 3.3;
    esp_codec_dev_cfg_t dev = {};
    dev.dev_type = ESP_CODEC_DEV_TYPE_OUT;
    dev.codec_if = es8311_codec_new(&es);
    dev.data_if = audio_codec_new_i2s_data(&data);
    if (!dev.codec_if || !dev.data_if)
        return false;
    codec = esp_codec_dev_new(&dev);
    esp_codec_dev_sample_info_t format = {};
    format.sample_rate = rate;
    format.channel = 1;
    format.bits_per_sample = 16;
    if (!codec || esp_codec_dev_open(codec, &format) != ESP_CODEC_DEV_OK)
        return false;
    // This board's DAC/amp was verified at codec volume 100. User volume is a
    // software gain, preserving the known-good analog gain configuration.
    esp_codec_dev_set_out_vol(codec, 100);
    esp_codec_dev_set_out_mute(codec, true);
    queue = xQueueCreate(1, sizeof(Playback));
    control = xSemaphoreCreateMutex();
    if (!queue || !control || xTaskCreate(worker, "ws-speaker", 4096, nullptr, 5, nullptr) != pdPASS)
        return false;
    ESP_LOGI("ws-audio", "ES8311 ready, 24kHz mono, MCLK GPIO16, PA GPIO46");
    return true;
}
} // namespace ws
void m5::Speaker_Class::setVolume(uint8_t v) {
    volume.store(v);
}
void m5::Speaker_Class::stop() {
    if (!queue)
        return;
    xSemaphoreTake(control, portMAX_DELAY);
    ++generation;
    clear_pending();
    xSemaphoreGive(control);
}
bool m5::Speaker_Class::playRaw(const int16_t *samples, size_t count, uint32_t input_rate, bool stereo, uint32_t repeat,
                                int, bool) {
    if (!queue || !samples || !count || !input_rate || !repeat)
        return false;
    size_t frames = count / (stereo ? 2 : 1);
    if (!frames || input_rate < 8000 || input_rate > 96000 || frames > input_rate * 60ULL)
        return false;
    size_t output_frames = (uint64_t(frames) * rate + input_rate - 1) / input_rate;
    auto *output = static_cast<int16_t *>(heap_caps_malloc(output_frames * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!output)
        return false;
    auto sample = [&](size_t i) -> int32_t {
        return stereo ? (int32_t(samples[i * 2]) + samples[i * 2 + 1]) / 2 : samples[i];
    };
    for (size_t i = 0; i < output_frames; ++i) {
        uint64_t pos = uint64_t(i) * input_rate;
        size_t a = std::min<size_t>(pos / rate, frames - 1), b = std::min(a + 1, frames - 1);
        int32_t frac = pos % rate;
        output[i] = (sample(a) * (int32_t(rate) - frac) + sample(b) * frac) / int32_t(rate);
    }
    // Own the PCM copy: the caller may decode the next card immediately after stop().
    xSemaphoreTake(control, portMAX_DELAY);
    Playback req{output, output_frames, ++generation, std::min<uint32_t>(repeat, 16)};
    clear_pending();
    bool ok = xQueueSend(queue, &req, 0) == pdTRUE;
    xSemaphoreGive(control);
    if (!ok)
        free(output);
    return ok;
}
void m5::Speaker_Class::tone(uint32_t frequency, uint32_t duration, int channel) {
    if (!frequency)
        return;
    duration = std::min<uint32_t>(duration, 2000);
    size_t count = uint64_t(rate) * duration / 1000;
    auto *pcm = static_cast<int16_t *>(heap_caps_malloc(count * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pcm)
        return;
    for (size_t i = 0; i < count; ++i)
        pcm[i] = std::sin(6.283185307179586 * frequency * i / rate) * 10000;
    playRaw(pcm, count, rate, false, 1, channel, true);
    free(pcm);
}
