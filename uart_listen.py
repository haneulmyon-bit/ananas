#!/usr/bin/env python3
"""
Simple USART listener for STM32 logs.

Example:
  python uart_listen.py --port COM5 --baud 115200
"""

from __future__ import annotations

import argparse
import sys
from datetime import datetime

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Missing dependency: pyserial")
    print("Install with: pip install pyserial")
    sys.exit(1)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Listen to USART and print incoming lines.")
    parser.add_argument("--port", help="Serial port (e.g. COM5, /dev/ttyUSB0).")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate. Default: 115200")
    parser.add_argument("--timeout", type=float, default=1.0, help="Read timeout in seconds. Default: 1.0")
    parser.add_argument("--encoding", default="utf-8", help="Decode encoding. Default: utf-8")
    parser.add_argument("--timestamp", action="store_true", help="Prefix lines with timestamp.")
    parser.add_argument("--list", action="store_true", help="List available serial ports and exit.")
    return parser


def print_ports() -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return

    print("Available serial ports:")
    for p in ports:
        desc = p.description or "n/a"
        print(f"  {p.device} - {desc}")


def main() -> int:
    args = build_parser().parse_args()

    if args.list:
        print_ports()
        return 0

    if not args.port:
        print("Error: --port is required (or use --list).")
        return 2

    try:
        ser = serial.Serial(args.port, args.baud, timeout=args.timeout)
    except serial.SerialException as exc:
        print(f"Cannot open {args.port}: {exc}")
        return 1

    print(f"Listening on {args.port} @ {args.baud} baud. Press Ctrl+C to stop.")

    try:
        while True:
            data = ser.readline()
            if not data:
                continue

            line = data.decode(args.encoding, errors="replace").rstrip("\r\n")
            if args.timestamp:
                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                print(f"[{ts}] {line}")
            else:
                print(line)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
