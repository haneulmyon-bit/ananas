from __future__ import annotations

import threading
from typing import Optional

import serial


class SerialTransport:
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 0.02) -> None:
        self.port = port
        self.baudrate = int(baudrate)
        self.timeout = float(timeout)
        self._ser: Optional[serial.Serial] = None
        self._write_lock = threading.Lock()

    def open(self) -> None:
        if self._ser is not None and self._ser.is_open:
            return
        self._ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)

    def close(self) -> None:
        if self._ser is not None:
            try:
                self._ser.close()
            finally:
                self._ser = None

    @property
    def is_open(self) -> bool:
        return self._ser is not None and self._ser.is_open

    def read(self, size: int = 256) -> bytes:
        if not self.is_open or self._ser is None:
            return b""
        return self._ser.read(size)

    def write(self, data: bytes) -> None:
        if not self.is_open or self._ser is None:
            raise RuntimeError("Serial port is not open")
        with self._write_lock:
            self._ser.write(data)

