from __future__ import annotations

import argparse
import time

from host.controller import HostController
from host.logger import TelemetryLogger
from host.serial_transport import SerialTransport
from host.telemetry_store import TelemetryStore


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="STM32 host CLI (telemetry + motor commands).")
    parser.add_argument("--port", default="COM7", help="Serial port (default: COM7)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=0.02, help="Serial timeout in seconds")
    parser.add_argument("--jsonl", default="", help="JSONL telemetry log path")
    parser.add_argument("--csv", default="", help="CSV telemetry log path")

    sub = parser.add_subparsers(dest="command", required=True)

    p_monitor = sub.add_parser("monitor", help="Print live telemetry frames")
    p_monitor.add_argument("--duration", type=float, default=0.0, help="Auto-stop after N seconds (0=forever)")

    p_speed = sub.add_parser("set-speed", help="Send speed command and keep streaming")
    p_speed.add_argument("value", type=int, help="Speed in percent (-100..100)")
    p_speed.add_argument("--duration", type=float, default=2.0, help="Monitoring time after command")

    p_stop = sub.add_parser("stop", help="Send stop command and keep streaming")
    p_stop.add_argument("--duration", type=float, default=2.0, help="Monitoring time after command")

    return parser


def build_controller(args: argparse.Namespace) -> HostController:
    transport = SerialTransport(args.port, args.baud, args.timeout)
    store = TelemetryStore()
    logger = None
    if args.jsonl or args.csv:
        logger = TelemetryLogger(jsonl_path=args.jsonl or None, csv_path=args.csv or None)
    return HostController(transport=transport, store=store, logger=logger)


def print_sample(sample: dict) -> None:
    line = (
        f"t={sample.get('timestamp_ms', 0):>8}ms "
        f"emg={sample.get('emg', 0):>4} fsr2={sample.get('fsr2', 0):>4} fsr1={sample.get('fsr1', 0):>4} "
        f"hall={sample.get('hall1', 0)}{sample.get('hall2', 0)}{sample.get('hall3', 0)} "
        f"motor={sample.get('motor_speed_pct', 0):>4}% duty={sample.get('motor_duty_pct', 0):>3}% "
        f"dir={sample.get('motor_direction', 0)}"
    )
    print(line)


def run_monitor(controller: HostController, duration: float) -> int:
    unsub = controller.subscribe(print_sample)
    started = time.time()
    try:
        while True:
            if duration > 0 and (time.time() - started) >= duration:
                return 0
            time.sleep(0.05)
    except KeyboardInterrupt:
        return 0
    finally:
        unsub()


def main() -> int:
    args = build_parser().parse_args()
    controller = build_controller(args)

    try:
        controller.start()
    except Exception as exc:
        print(f"Failed to open serial port: {exc}")
        return 1

    try:
        if args.command == "monitor":
            return run_monitor(controller, duration=args.duration)

        if args.command == "set-speed":
            controller.set_speed(args.value)
            return run_monitor(controller, duration=args.duration)

        if args.command == "stop":
            controller.stop_motor()
            return run_monitor(controller, duration=args.duration)

        return 2
    finally:
        controller.stop()


if __name__ == "__main__":
    raise SystemExit(main())
