#!/usr/bin/env python3
"""Reset the watch and print its log for a few seconds:  serial_log.py [port] [seconds]

Without a port, the watch's USB port is found by itself (the only USB modem port).
"""
import glob, sys, time
import serial

if len(sys.argv) > 1:
    port = sys.argv[1]
else:
    ports = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*")
    if len(ports) != 1:
        sys.exit(f"give the port: {'no USB modem port found' if not ports else 'several found: ' + ', '.join(ports)}")
    port = ports[0]
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 10
s = serial.Serial(port, 115200, timeout=0.2)
s.dtr = False
s.rts = True
time.sleep(0.1)
s.rts = False
end = time.time() + seconds
while time.time() < end:
    data = s.read(4096)
    if data:
        sys.stdout.write(data.decode("utf-8", "replace"))
        sys.stdout.flush()
