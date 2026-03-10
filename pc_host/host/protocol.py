from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

SOF1 = 0xAA
SOF2 = 0x55
PROTOCOL_VERSION = 0x01
HEADER_SIZE = 6
CRC_SIZE = 2
MAX_PAYLOAD = 64

MSG_TELEMETRY = 0x01
MSG_CMD_SET_SPEED = 0x10
MSG_CMD_STOP = 0x11
MSG_CMD_PING = 0x12
MSG_PONG = 0x13

MOTOR_DIR_STOP = 0
MOTOR_DIR_FORWARD = 1
MOTOR_DIR_REVERSE = 2


@dataclass(frozen=True)
class Frame:
    version: int
    msg_type: int
    flags: int
    seq: int
    payload: bytes


def crc16_ccitt(data: bytes, init: int = 0xFFFF) -> int:
    crc = init & 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def encode_frame(
    msg_type: int,
    payload: bytes = b"",
    *,
    seq: int = 0,
    version: int = PROTOCOL_VERSION,
    flags: int = 0,
) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"Payload too large: {len(payload)} > {MAX_PAYLOAD}")

    header = bytes(
        [
            version & 0xFF,
            msg_type & 0xFF,
            flags & 0xFF,
            seq & 0xFF,
            len(payload) & 0xFF,
            (len(payload) >> 8) & 0xFF,
        ]
    )
    crc = crc16_ccitt(header + payload)
    return bytes([SOF1, SOF2]) + header + payload + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


class FrameParser:
    _WAIT_SOF1 = 0
    _WAIT_SOF2 = 1
    _READ_HEADER = 2
    _READ_PAYLOAD = 3
    _READ_CRC_L = 4
    _READ_CRC_H = 5

    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self._state = self._WAIT_SOF1
        self._header = bytearray()
        self._payload = bytearray()
        self._expected_len = 0
        self._crc_lsb = 0

    def feed(self, data: bytes) -> List[Frame]:
        frames: List[Frame] = []
        for b in data:
            frame = self.feed_byte(b)
            if frame is not None:
                frames.append(frame)
        return frames

    def feed_byte(self, byte: int) -> Optional[Frame]:
        byte &= 0xFF

        if self._state == self._WAIT_SOF1:
            if byte == SOF1:
                self._state = self._WAIT_SOF2
            return None

        if self._state == self._WAIT_SOF2:
            if byte == SOF2:
                self._state = self._READ_HEADER
                self._header.clear()
            elif byte != SOF1:
                self._state = self._WAIT_SOF1
            return None

        if self._state == self._READ_HEADER:
            self._header.append(byte)
            if len(self._header) == HEADER_SIZE:
                self._expected_len = self._header[4] | (self._header[5] << 8)
                if self._expected_len > MAX_PAYLOAD:
                    self.reset()
                    return None
                self._payload.clear()
                self._state = self._READ_CRC_L if self._expected_len == 0 else self._READ_PAYLOAD
            return None

        if self._state == self._READ_PAYLOAD:
            self._payload.append(byte)
            if len(self._payload) >= self._expected_len:
                self._state = self._READ_CRC_L
            return None

        if self._state == self._READ_CRC_L:
            self._crc_lsb = byte
            self._state = self._READ_CRC_H
            return None

        if self._state == self._READ_CRC_H:
            rx_crc = self._crc_lsb | (byte << 8)
            calc_crc = crc16_ccitt(bytes(self._header) + bytes(self._payload))
            if rx_crc == calc_crc:
                frame = Frame(
                    version=self._header[0],
                    msg_type=self._header[1],
                    flags=self._header[2],
                    seq=self._header[3],
                    payload=bytes(self._payload),
                )
                self.reset()
                return frame
            self.reset()
            return None

        self.reset()
        return None


def clamp_speed(speed: int) -> int:
    return max(-100, min(100, int(speed)))


def build_cmd_set_speed(speed: int, *, seq: int = 0) -> bytes:
    speed_i8 = clamp_speed(speed) & 0xFF
    return encode_frame(MSG_CMD_SET_SPEED, bytes([speed_i8]), seq=seq)


def build_cmd_stop(*, seq: int = 0) -> bytes:
    return encode_frame(MSG_CMD_STOP, b"", seq=seq)


def build_cmd_ping(nonce: int, *, seq: int = 0) -> bytes:
    payload = int(nonce).to_bytes(4, byteorder="little", signed=False)
    return encode_frame(MSG_CMD_PING, payload, seq=seq)


def parse_telemetry_payload(payload: bytes) -> dict:
    if len(payload) < 15:
        raise ValueError(f"Telemetry payload too short: {len(payload)}")

    timestamp_ms = int.from_bytes(payload[0:4], byteorder="little", signed=False)
    emg = int.from_bytes(payload[4:6], byteorder="little", signed=False)
    fsr2 = int.from_bytes(payload[6:8], byteorder="little", signed=False)
    fsr1 = int.from_bytes(payload[8:10], byteorder="little", signed=False)
    hall_bits = payload[10]
    signed_speed = int.from_bytes(payload[11:12], byteorder="little", signed=True)
    duty_pct = payload[12]
    direction = payload[13]
    fault_flags = payload[14]

    return {
        "timestamp_ms": timestamp_ms,
        "emg": emg,
        "fsr2": fsr2,
        "fsr1": fsr1,
        "hall1": 1 if (hall_bits & 0x01) else 0,
        "hall2": 1 if (hall_bits & 0x02) else 0,
        "hall3": 1 if (hall_bits & 0x04) else 0,
        "hall_bits": hall_bits,
        "motor_speed_pct": signed_speed,
        "motor_duty_pct": duty_pct,
        "motor_direction": direction,
        "motor_fault_flags": fault_flags,
    }
