#!/usr/bin/env python3
"""Passively capture USB Serial/JTAG logs without toggling reset lines."""
import argparse
import time
from pathlib import Path

import serial

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
parser.add_argument("--seconds", type=float, default=20)
parser.add_argument("--port", default="/dev/cu.usbmodem1101")
args = parser.parse_args()

port = serial.Serial(port=None, baudrate=115200, timeout=0.25)
port.dtr = False
port.rts = False
port.port = args.port
try:
    with args.output.open("wb") as output:
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            try:
                if not port.is_open:
                    port.open()
                chunk = port.read(4096)
            except (serial.SerialException, OSError):
                port.close()
                time.sleep(0.25)
                continue
            if chunk:
                output.write(chunk)
                output.flush()
finally:
    port.close()
print(f"Captured {args.output.stat().st_size} bytes to {args.output}")
