#!/usr/bin/env python3
"""USB tools requiring an explicit board MAC; never auto-pick another board.

Run with the ESP-IDF Python environment (esptool + pyserial).
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path
import serial
from serial.tools import list_ports

ROOT = Path(__file__).resolve().parents[1]
MAC = None


def mac_address(value):
    if not re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', value):
        raise argparse.ArgumentTypeError('MAC must have the form AA:BB:CC:DD:EE:FF')
    return value.upper()


def port():
    if not MAC:
        raise RuntimeError('Specify --mac or KNOEDEL_MAC before opening a serial device')
    found = [p.device for p in list_ports.comports() if (p.serial_number or '').upper() == MAC]
    if len(found) != 1:
        raise RuntimeError(f'Expected exactly one Knödel board {MAC}; found {found}')
    return found[0]


def open_serial():
    s = serial.Serial(port=None, baudrate=115200, timeout=0.15)
    # Opening with pyserial's default RTS/DTR can reset the native USB board.
    s.dtr = False
    s.rts = False
    s.port = port()
    s.open()
    return s


def collect(s, seconds, marker=None):
    raw = bytearray()
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        raw.extend(s.read(4096))
        if marker and marker in raw:
            break
    return bytes(raw)


def reset(seconds):
    from esp_pylib.serial_reset import set_dtr, set_rts
    with open_serial() as s:
        s.reset_input_buffer()
        set_dtr(s, False)
        set_rts(s, True)
        time.sleep(0.2)
        set_rts(s, False)
        return collect(s, seconds)


def command(line, seconds=8):
    with open_serial() as s:
        return send_command(s, line, seconds)


def send_command(s, line, seconds=8):
    s.reset_input_buffer()
    s.write((line + '\n').encode())
    s.flush()
    result = collect(s, seconds, ('@OK ' + line).encode())
    if ('@OK ' + line).encode() not in result or any(e in result for e in
            (b'@ERROR', b'Guru Meditation', b'assert failed', b'task_wdt:')):
        raise RuntimeError(f'Device did not accept {line!r}: {result.decode(errors="replace")[-2000:]}')
    return result


def flash(output, content=True, build=ROOT/'build-waveshare', bundle=None):
    sys.path.insert(0, str(ROOT / 'tools'))
    import contentpack
    if bundle:
        saved = json.loads((bundle/'manifest.json').read_text())
        expected = {'bootloader.bin': 0, 'partition-table.bin': 0x8000,
                    'knoedel_watch.bin': 0x10000, 'a2-start.bin': 0x310000}
        if saved['mac'] != MAC or {f['name']: int(f['offset'], 16) for f in saved['files']} != expected:
            raise RuntimeError('Bundle is not the complete firmware for this Knödel board')
        paths = []
        for f in saved['files']:
            path = bundle/f['name']
            data = path.read_bytes()
            if len(data) != f['bytes'] or hashlib.sha256(data).hexdigest() != f['sha256']:
                raise RuntimeError(f'Bundle hash mismatch: {path.name}')
            if content or f['name'] != 'a2-start.bin':
                paths.append((int(f['offset'], 16), path))
    else:
        config = json.loads((build/'config/sdkconfig.json').read_text())
        if not config.get('KNOEDEL_BOARD_WAVESHARE_1_75C') or config.get('ESPTOOLPY_FLASHSIZE') != '32MB':
            raise RuntimeError('Refusing firmware built for a different board or flash size')
        paths = [(0, build/'bootloader/bootloader.bin'),
                 (0x8000, build/'partition_table/partition-table.bin'),
                 (0x10000, build/'knoedel_watch.bin')]
        if content:
            paths.append((0x310000, ROOT/'packs/a2-start.bin'))
    if content:
        pack = next(p for o, p in paths if o == 0x310000)
        contentpack.read(pack.read_bytes())  # CRC and table validation before opening serial.
        offset, capacity = contentpack.partition()
        if pack.stat().st_size > capacity:
            raise RuntimeError('Content does not fit the partition')
        if offset != 0x310000:
            raise RuntimeError('Unexpected content partition address')
    output.mkdir(parents=True, exist_ok=True)
    snapshots = []
    for offset, src in paths:
        data = src.read_bytes()
        dst = output/src.name
        if dst.exists():
            raise RuntimeError(f'Refusing to overwrite previous flash evidence: {dst}')
        dst.write_bytes(data)
        snapshots.append((offset, dst))
    target = port()
    identify = subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', target, 'read-mac'],
                              check=True, capture_output=True, text=True)
    if MAC.lower() not in identify.stdout.lower():
        raise RuntimeError('The ROM-reported MAC does not match the target')
    (output/'identity.log').write_text(identify.stdout)
    if port() != target:
        raise RuntimeError('USB port changed during identity verification')
    cmd = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', target, '--baud', '921600',
           '--after', 'no-reset', 'write-flash', '--flash-mode', 'dio', '--flash-size', '32MB', '--flash-freq', '80m']
    for offset, path in snapshots:
        cmd += [hex(offset), str(path)]
    with (output/'flash.log').open('w') as log:
        subprocess.run(cmd, check=True, stdout=log, stderr=subprocess.STDOUT)
    app = (output/'knoedel_watch.bin').read_bytes()
    manifest = {'mac': MAC, 'expected_elf_sha256': app[0xb0:0xd0].hex(),
                'files': [{'offset': hex(o), 'name': p.name, 'bytes': p.stat().st_size,
                           'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for o, p in snapshots]}
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    boot = reset(18)
    (output/'boot.log').write_bytes(boot)
    print(boot.decode(errors='replace'))
    observed = re.search(rb'ELF file SHA256:\s*([0-9a-f]+)', boot)
    if not observed or not manifest['expected_elf_sha256'].startswith(observed[1].decode()):
        raise RuntimeError('Written firmware has not been confirmed booting; inspect boot.log')
    if any(error in boot for error in (b'Guru Meditation', b'abort()', b'assert failed', b'task_wdt:')):
        raise RuntimeError('Firmware booted but failed at runtime; inspect boot.log')
    print('BOOT_ELF_VERIFIED', manifest['expected_elf_sha256'])


def screen(output):
    with open_serial() as s:
        for attempt in range(3):
            raw = send_command(s, 'screen', 35)
            rows = {int(y): bytes.fromhex(data.decode()) for y, data in
                    re.findall(rb'@ROW (\d+) ([0-9a-f]{1864})\r?\n', raw)}
            if set(rows) == set(range(466)) and b'@FRAME_END' in raw:
                break
            # Audio/ROM logs can interleave with the long text transfer. Retry
            # in the same connection so opening USB does not restart playback.
            print(f'Capture {attempt+1}: {len(rows)}/466 intact rows; retrying', file=sys.stderr)
        else:
            raise RuntimeError(f'Incomplete framebuffer capture: {len(rows)} rows; no image saved')
    rgb = bytearray()
    for y in range(466):
        row = rows[y]
        for x in range(0, len(row), 2):
            value = (row[x] << 8) | row[x+1]
            r, g, b = value >> 11, (value >> 5) & 63, value & 31
            rgb.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(b'P6\n466 466\n255\n' + rgb)
    print(output)


def main():
    global MAC
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action', choices=['port', 'flash', 'reset', 'command', 'screen'])
    ap.add_argument('value', nargs='?')
    ap.add_argument('--seconds', type=float, default=10)
    ap.add_argument('--app-only', action='store_true')
    ap.add_argument('--mac', type=mac_address, default=os.environ.get('KNOEDEL_MAC'),
                    help='required target USB serial / ROM MAC (or set KNOEDEL_MAC)')
    ap.add_argument('--build-dir', type=Path, default=ROOT/'build-waveshare')
    ap.add_argument('--bundle', type=Path, help='restore a complete, hash-checked firmware snapshot')
    a = ap.parse_args()
    if not a.mac:
        ap.error('--mac or KNOEDEL_MAC is required; no serial port has been opened')
    if a.action in ('flash', 'command', 'screen') and not a.value:
        ap.error(f'{a.action} requires a value')
    MAC = a.mac
    if a.action == 'port': print(port())
    elif a.action == 'flash': flash(Path(a.value), not a.app_only, a.build_dir.resolve(), a.bundle)
    elif a.action == 'reset': print(reset(a.seconds).decode(errors='replace'))
    elif a.action == 'command': print(command(a.value, a.seconds).decode(errors='replace'))
    elif a.action == 'screen': screen(Path(a.value))

if __name__ == '__main__':
    main()
