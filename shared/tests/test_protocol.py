from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PC_HOST_DIR = ROOT / "pc_host"
if str(PC_HOST_DIR) not in sys.path:
    sys.path.insert(0, str(PC_HOST_DIR))

from host.protocol import (  # noqa: E402
    MSG_CMD_SET_SPEED,
    MSG_TELEMETRY,
    FrameParser,
    build_cmd_set_speed,
    encode_frame,
    parse_telemetry_payload,
)


def test_cmd_set_speed_frame_hex() -> None:
    frame = build_cmd_set_speed(30, seq=0x01)
    assert frame.hex() == "aa550110000101001e50e1"


def test_parser_roundtrip() -> None:
    payload = bytes([0x88, 0x13, 0, 0, 0x90, 0x01, 0x20, 0x03, 0x40, 0x02, 0x05, 0xEC, 0x14, 0x02, 0x00])
    encoded = encode_frame(MSG_TELEMETRY, payload, seq=0x10)
    parser = FrameParser()
    frames = parser.feed(encoded)

    assert len(frames) == 1
    assert frames[0].msg_type == MSG_TELEMETRY
    telemetry = parse_telemetry_payload(frames[0].payload)
    assert telemetry["timestamp_ms"] == 5000
    assert telemetry["emg"] == 400
    assert telemetry["fsr2"] == 800
    assert telemetry["fsr1"] == 576
    assert telemetry["hall1"] == 1
    assert telemetry["hall2"] == 0
    assert telemetry["hall3"] == 1
    assert telemetry["motor_speed_pct"] == -20
    assert telemetry["motor_duty_pct"] == 20
    assert telemetry["motor_direction"] == 2


def test_parser_rejects_bad_crc() -> None:
    encoded = bytearray(build_cmd_set_speed(-10, seq=0x05))
    encoded[-1] ^= 0xFF

    parser = FrameParser()
    frames = parser.feed(bytes(encoded))
    assert frames == []


def test_parser_accepts_fragmented_stream() -> None:
    encoded = encode_frame(MSG_CMD_SET_SPEED, bytes([0xFE]), seq=0x07)
    parser = FrameParser()
    out = []
    for b in encoded:
        frame = parser.feed_byte(b)
        if frame is not None:
            out.append(frame)

    assert len(out) == 1
    assert out[0].msg_type == MSG_CMD_SET_SPEED
    assert out[0].payload == bytes([0xFE])

