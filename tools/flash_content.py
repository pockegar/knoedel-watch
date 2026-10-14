#!/usr/bin/env python3
"""Write a content pack to the watch. The program on the watch is not touched.

  python3 tools/flash_content.py                    # the pack of content/library.json
  python3 tools/flash_content.py packs/a2.bin
  python3 tools/flash_content.py packs/a2.bin -p /dev/cu.usbmodem101

Needs esptool (it comes with ESP-IDF; otherwise: pip install esptool).
"""
import argparse, glob, json, subprocess, sys
from pathlib import Path
import contentpack

ROOT = contentpack.ROOT


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("pack", nargs="?", help="default: packs/<pack of content/library.json>.bin")
    ap.add_argument("-p", "--port", help="serial port; default: the only USB modem port, else esptool's own search")
    a = ap.parse_args()

    path = Path(a.pack) if a.pack else ROOT / "packs" / (json.loads((ROOT / "content" / "library.json").read_text())["pack"] + ".bin")
    if not path.exists():
        sys.exit(f"{path} does not exist: build it with tools/import_decks.py")
    data = path.read_bytes()
    name, files = contentpack.read(data)
    offset, size = contentpack.partition()
    if len(data) > size:
        sys.exit(f"{path.name} is {len(data) // 1024} KB, the content partition holds {size // 1024} KB")
    ports = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*")
    port = a.port or (ports[0] if len(ports) == 1 else None)
    print(f"pack \"{name}\": {len(files)} files, {len(data) // 1024} KB of {size // 1024} KB, to {hex(offset)} on {port or 'the first port found'}")
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32s3"] + (["-p", port] if port else [])
    cmd += ["-b", "921600", "write_flash", hex(offset), str(path)]
    sys.exit(subprocess.run(cmd).returncode)


if __name__ == "__main__":
    main()
