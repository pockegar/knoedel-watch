"""The content pack: one file with the decks, fonts and audio of the watch.

It lives in the flash partition "content" (partitions.csv), apart from the program,
so content can be exchanged without rebuilding or reflashing the firmware.
Layout: main/content.h. Built by import_decks.py, written by flash_content.py.
"""
import struct, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER, ENTRY = 64, 32


def partition():
    """(offset, size) of the content partition."""
    for line in (ROOT / "partitions.csv").read_text(encoding="utf-8").splitlines():
        cols = [c.strip() for c in line.split("#")[0].split(",")]
        if cols[0] == "content":
            return int(cols[3], 0), int(cols[4], 0)
    raise SystemExit("partitions.csv has no 'content' partition")


def build(name, files):
    """files: {file name: bytes}. Returns the pack."""
    if len(name.encode()) > 39 or any(len(n.encode()) > 23 for n in files):
        raise SystemExit("content pack: a name is too long")
    offset, entries, body = HEADER + ENTRY * len(files), b"", b""
    for n, data in files.items():
        entries += struct.pack("<24s2I", n.encode(), offset + len(body), len(data))
        body += data + b"\0" * (-len(data) % 4)
    rest = entries + body
    head = b"KCP1" + struct.pack("<5I40s", 1, len(files), HEADER + len(rest), zlib.crc32(rest), 0, name.encode())
    return head + rest


def read(data):
    """(name, {file name: bytes}) of a pack; raises SystemExit if it is not one."""
    if len(data) < HEADER or data[:4] != b"KCP1":
        raise SystemExit("not a content pack")
    version, count, total, crc, _, name = struct.unpack_from("<5I40s", data, 4)
    if version != 1 or total > len(data) or zlib.crc32(data[HEADER:total]) != crc:
        raise SystemExit("content pack is damaged or of another version")
    files = {}
    for i in range(count):
        n, off, size = struct.unpack_from("<24s2I", data, HEADER + ENTRY * i)
        files[n.rstrip(b"\0").decode()] = data[off:off + size]
    return name.rstrip(b"\0").decode(), files
