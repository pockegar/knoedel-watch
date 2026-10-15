"""Offline checks for selecting the intended board and validating flash inputs.

Run with ESP-IDF Python: python -m unittest discover -s tools -p 'test_device_ws.py'
No test opens a serial connection.
"""
import argparse
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import contentpack
import device_ws as device

TARGET = 'AA:BB:CC:DD:EE:FF'


class SerialBoundary(Exception):
    pass


class DeviceGuardTests(unittest.TestCase):
    def setUp(self):
        self.mac_patch = patch.object(device, 'MAC', TARGET)
        self.mac_patch.start()
        self.addCleanup(self.mac_patch.stop)

    def test_mac_requires_complete_address(self):
        self.assertEqual(device.mac_address(TARGET.lower()), TARGET)
        for invalid in ('', 'auto', 'AA:BB:CC', 'AA:BB:CC:DD:EE:FF:00'):
            with self.assertRaises(argparse.ArgumentTypeError):
                device.mac_address(invalid)

    def test_port_never_falls_back_to_another_board(self):
        other = SimpleNamespace(device='/dev/other', serial_number='00:11:22:33:44:55')
        intended = SimpleNamespace(device='/dev/intended', serial_number=TARGET.lower())
        with patch.object(device.list_ports, 'comports', return_value=[other, intended]):
            self.assertEqual(device.port(), '/dev/intended')
        for ports in ([], [other], [intended, intended]):
            with patch.object(device.list_ports, 'comports', return_value=ports):
                with self.assertRaises(RuntimeError):
                    device.port()
        with patch.object(device, 'MAC', None):
            with self.assertRaises(RuntimeError):
                device.port()

    def test_m5_build_is_rejected_before_serial(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'config').mkdir()
            (root/'config/sdkconfig.json').write_text(json.dumps({
                'KNOEDEL_BOARD_M5_STOPWATCH': True, 'ESPTOOLPY_FLASHSIZE': '16MB'}))
            with patch.object(device, 'port', side_effect=AssertionError('serial opened')):
                with self.assertRaisesRegex(RuntimeError, 'different board'):
                    device.flash(root/'output', build=root)

    def test_bundle_identity_layout_and_hashes_before_serial(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bundle = root/'bundle'
            bundle.mkdir()
            files = [
                ('bootloader.bin', 0, b'bootloader'),
                ('partition-table.bin', 0x8000, b'partitions'),
                ('knoedel_watch.bin', 0x10000, b'application'),
                ('a2-start.bin', 0x310000, contentpack.build('test', {'test.bin': b'content'})),
            ]
            manifest = {'mac': TARGET, 'files': []}
            for name, offset, data in files:
                (bundle/name).write_bytes(data)
                manifest['files'].append({'name': name, 'offset': hex(offset), 'bytes': len(data),
                                          'sha256': hashlib.sha256(data).hexdigest()})
            saved = json.dumps(manifest)
            (bundle/'manifest.json').write_text(saved)
            with patch.object(device, 'port', side_effect=SerialBoundary):
                with self.assertRaises(SerialBoundary):
                    device.flash(root/'valid', bundle=bundle)
            for fault in ('mac', 'offset', 'hash', 'missing_file'):
                manifest = json.loads(saved)
                if fault == 'mac':
                    manifest['mac'] = '00:11:22:33:44:55'
                elif fault == 'offset':
                    manifest['files'][0]['offset'] = '0x9000'
                elif fault == 'hash':
                    manifest['files'][0]['sha256'] = '0'*64
                else:
                    manifest['files'].pop()
                (bundle/'manifest.json').write_text(json.dumps(manifest))
                with self.subTest(fault=fault), patch.object(device, 'port', side_effect=AssertionError('serial opened')):
                    with self.assertRaises(RuntimeError):
                        device.flash(root/fault, bundle=bundle)


if __name__ == '__main__':
    unittest.main()
