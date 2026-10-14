#pragma once
// The content pack: decks, fonts and audio, in a flash partition of their own
// ("content" in partitions.csv), so that content can be exchanged without
// touching the program. tools/import_decks.py builds a pack,
// tools/flash_content.py writes it to the watch.
//
// Layout (little-endian), see tools/contentpack.py:
//   header, 64 bytes: "KCP1", version, file count, total size, CRC-32 of
//                     everything after the header, reserved, name[40]
//   one entry per file, 32 bytes: name[24], offset, size
//   the files, each starting on a 4-byte boundary

#include <stddef.h>
#include <stdint.h>

struct ContentFile {
    const uint8_t *data;  // nullptr: the pack has no such file
    size_t size;
};

// Maps the pack into memory. False if the partition holds no complete pack.
bool content_mount();
const char *content_name();
ContentFile content_file(const char *name);
