#!/usr/bin/env python3
"""Run patched UserDemo AX900 diagnostics and decode its LVGL snapshot.

Reset lines are not deliberately toggled, but opening CDC may reset Tab5 on macOS.
"""
import argparse
import re
import struct
import time
import zlib
from pathlib import Path
import serial


def decode_snapshot(log, target):
    match = re.search(rb"AXSNAP BEGIN (\d+) (\d+) RGB565-RLE", log)
    if not match:
        raise ValueError("No snapshot header received")
    width, height = map(int, match.groups())
    if not (0 < width <= 4096 and 0 < height <= 4096):
        raise ValueError("Invalid dimensions")
    records = dict((int(y), bytes.fromhex(data.decode())) for y, data in
                   re.findall(rb"AXSNAP ROW (\d+) ([0-9a-f]+)", log[match.end():]))
    if set(records) != set(range(height)):
        raise ValueError(f"Incomplete snapshot: {len(records)}/{height} rows")
    pixels = bytearray()
    for y in range(height):
        data = records[y]
        if len(data) % 3:
            raise ValueError("Truncated RLE record")
        row = bytearray()
        for i in range(0, len(data), 3):
            count, lo, hi = data[i:i+3]
            color = lo | hi << 8
            r, g, b = (color >> 11) & 31, (color >> 5) & 63, color & 31
            row.extend(bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))) * count)
        if len(row) != width * 3:
            raise ValueError(f"Invalid pixel count in row {y}")
        pixels.extend(b"\0" + row)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b"")
    target.write_bytes(png)
    print(f"Saved {width}x{height} device snapshot: {target}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--port", default="/dev/cu.usbmodem1101")
    parser.add_argument("--command", action="append", default=[])
    parser.add_argument("--delay", type=float, default=6)
    parser.add_argument("--startup-delay", type=float, default=16,
                        help="Allow macOS CDC reopen/reset and the official splash screen to finish")
    parser.add_argument("--seconds", type=float, default=20)
    parser.add_argument("--png", type=Path)
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=0.1)
    port.dtr = port.rts = False
    port.port = args.port
    port.open()
    started = time.monotonic()
    index = 0
    try:
        with args.output.open("wb") as output:
            while time.monotonic() - started < args.seconds:
                if index < len(args.command) and time.monotonic() - started >= args.startup_delay + index * args.delay:
                    port.write((args.command[index] + "\n").encode())
                    index += 1
                data = port.read(16384)
                if data:
                    output.write(data)
                    output.flush()
    finally:
        port.close()
    if args.png:
        decode_snapshot(args.output.read_bytes(), args.png)
    else:
        print(f"Captured {args.output.stat().st_size} bytes: {args.output}")


if __name__ == "__main__":
    main()
