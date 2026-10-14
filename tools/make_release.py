#!/usr/bin/env python3
"""Make a release: the web installer and a single image for esptool.

It packages what is built; build first, each step in its own environment:

  idf.py build                       # the firmware (ESP-IDF)
  python3 tools/import_decks.py      # the content pack
  python3 tools/make_release.py      # this; needs esptool (pip install esptool, or ESP-IDF's Python)

It writes:
  release/site/      a static web page with ESP Web Tools: open it in Chrome or Edge,
                     plug in the watch, press Install. Publish it with GitHub Pages
                     (or any HTTPS host); the firmware files sit next to the page.
  release/knoedel-watch-<version>-full.bin
                     everything in one image, for a fresh install with esptool:
                       python3 -m esptool --chip esp32s3 write-flash 0x0 <file>
                     It erases what the watch remembers (progress, settings).

The version comes from PROJECT_VER in the top CMakeLists.txt.
"""
import argparse, importlib.util, json, os, re, shutil, subprocess, sys
from pathlib import Path
import contentpack

ROOT = contentpack.ROOT
BUILD, OUT = ROOT / "build", ROOT / "release"


def run(cmd):
    print("$", " ".join(str(c) for c in cmd))
    if subprocess.run(cmd, cwd=ROOT).returncode:
        sys.exit(f"failed: {cmd[1] if len(cmd) > 1 else cmd[0]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--library", default=str(ROOT / "content" / "library.json"), help="which content pack to ship")
    a = ap.parse_args()

    version = re.search(r'set\(PROJECT_VER "([^"]+)"\)', (ROOT / "CMakeLists.txt").read_text()).group(1)
    pack_name = json.loads(Path(a.library).read_text())["pack"]
    if not (BUILD / "flash_args").exists():
        sys.exit("no firmware build: run idf.py build first")
    # esptool from this Python, or else from ESP-IDF's.
    esptool_python = sys.executable
    if importlib.util.find_spec("esptool") is None:
        idf_python = Path(os.environ.get("IDF_PYTHON_ENV_PATH", "")) / "bin" / "python"
        if not idf_python.exists():
            sys.exit("esptool is missing: pip install esptool")
        esptool_python = str(idf_python)

    # The program's parts and their offsets, as the build lists them.
    parts = []
    for line in (BUILD / "flash_args").read_text().splitlines()[1:]:
        offset, path = line.split()
        parts.append((int(offset, 16), BUILD / path))
    pack = ROOT / "packs" / f"{pack_name}.bin"
    if not pack.exists():
        sys.exit(f"{pack} does not exist: run tools/import_decks.py")
    contentpack.read(pack.read_bytes())  # refuses a damaged pack
    parts.append((contentpack.partition()[0], pack))

    if OUT.exists():
        shutil.rmtree(OUT)
    site = OUT / "site"
    (site / "firmware").mkdir(parents=True)
    manifest_parts = []
    for offset, path in parts:
        name = path.name if path != pack else f"content-{pack_name}.bin"
        shutil.copy(path, site / "firmware" / name)
        manifest_parts.append({"path": f"firmware/{name}", "offset": offset})
    manifest = {
        "name": "Knödel Watch",
        "version": version,
        # Asked at every install: erase for a fresh start, keep to update without
        # losing progress and settings.
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-S3", "parts": manifest_parts}],
    }
    (site / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    page = (ROOT / "web" / "index.html").read_text(encoding="utf-8")
    (site / "index.html").write_text(page.replace("{{VERSION}}", version).replace("{{PACK}}", pack_name), encoding="utf-8")
    if (ROOT / "web" / "img").exists():
        shutil.copytree(ROOT / "web" / "img", site / "img")

    full = OUT / f"knoedel-watch-{version}-full.bin"
    cmd = [esptool_python, "-m", "esptool", "--chip", "esp32s3", "merge-bin", "-o", full,
           "--flash-mode", "dio", "--flash-size", "16MB"]
    for offset, path in parts:
        cmd += [hex(offset), path]
    run(cmd)

    size = sum(p.stat().st_size for p in (site / "firmware").iterdir())
    print(f"\nrelease {version} in {OUT.relative_to(ROOT)}/:")
    print(f"  site/            web installer, {len(parts)} parts, {size // 1024} KB")
    print(f"  {full.name}  {full.stat().st_size // 1024} KB")
    print("Try the installer locally: python3 -m http.server -d release/site 8000, then open http://localhost:8000"
          " (Web Serial works on localhost without HTTPS).")


if __name__ == "__main__":
    main()
