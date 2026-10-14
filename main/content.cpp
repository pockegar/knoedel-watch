#include "content.h"

#include <string.h>

#include <esp_log.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>

namespace {

const char *TAG = "content";
constexpr uint32_t PACK_VERSION = 1;
constexpr esp_partition_subtype_t SUBTYPE = (esp_partition_subtype_t)0x40;

struct __attribute__((packed)) Header {
    char magic[4];  // "KCP1"
    uint32_t version, count, total, crc, reserved;
    char name[40];
};
struct __attribute__((packed)) Entry {
    char name[24];
    uint32_t offset, size;
};

const uint8_t *s_pack;
Header s_head;

}  // namespace

bool content_mount()
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, SUBTYPE, "content");
    if (part == nullptr) {
        ESP_LOGE(TAG, "no content partition: flash the partition table of this firmware");
        return false;
    }
    Header h;
    if (esp_partition_read(part, 0, &h, sizeof(h)) != ESP_OK || memcmp(h.magic, "KCP1", 4) != 0) {
        ESP_LOGW(TAG, "the content partition is empty");
        return false;
    }
    if (h.version != PACK_VERSION || h.total > part->size || h.total < sizeof(Header) + h.count * sizeof(Entry)) {
        ESP_LOGW(TAG, "content pack version %u, %u bytes: not usable by this firmware", (unsigned)h.version, (unsigned)h.total);
        return false;
    }
    const void *map;
    esp_partition_mmap_handle_t handle;
    if (esp_partition_mmap(part, 0, h.total, ESP_PARTITION_MMAP_DATA, &map, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "cannot map %u bytes of content", (unsigned)h.total);
        return false;
    }
    const uint8_t *pack = (const uint8_t *)map;
    // A pack that was only partly written must not be used.
    if (esp_rom_crc32_le(0, pack + sizeof(Header), h.total - sizeof(Header)) != h.crc) {
        ESP_LOGW(TAG, "the content pack is damaged or incomplete: flash it again");
        esp_partition_munmap(handle);
        return false;
    }
    h.name[sizeof(h.name) - 1] = 0;
    s_head = h;
    s_pack = pack;
    ESP_LOGI(TAG, "pack \"%s\": %u files, %u KB of %u KB", s_head.name, (unsigned)h.count, (unsigned)(h.total / 1024),
             (unsigned)(part->size / 1024));
    return true;
}

const char *content_name() { return s_pack ? s_head.name : ""; }

ContentFile content_file(const char *name)
{
    if (s_pack != nullptr) {
        const Entry *e = (const Entry *)(s_pack + sizeof(Header));
        for (uint32_t i = 0; i < s_head.count; i++) {
            if (strncmp(e[i].name, name, sizeof(e[i].name)) == 0 && e[i].offset + e[i].size <= s_head.total) {
                return {s_pack + e[i].offset, e[i].size};
            }
        }
    }
    return {nullptr, 0};
}
