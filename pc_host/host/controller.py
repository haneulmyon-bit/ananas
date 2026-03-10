from __future__ import annotations

import threading
import time
from typing import Callable, Optional

from .logger import TelemetryLogger
from .protocol import (
    MSG_PONG,
    MSG_TELEMETRY,
    FrameParser,
    build_cmd_ping,
    build_cmd_set_speed,
    build_cmd_stop,
    parse_telemetry_payload,
)
from .serial_transport import SerialTransport
from .telemetry_store import TelemetryStore


class HostController:
    def __init__(
        self,
        transport: SerialTransport,
        store: TelemetryStore,
        logger: Optional[TelemetryLogger] = None,
    ) -> None:
        self._transport = transport
        self._store = store
        self._logger = logger
        self._parser = FrameParser()
        self._running = threading.Event()
        self._rx_thread: Optional[threading.Thread] = None
        self._seq = 0
        self._tx_lock = threading.Lock()
        self._last_pong = None

    @property
    def is_running(self) -> bool:
        return self._running.is_set()

    @property
    def is_connected(self) -> bool:
        return self._transport.is_open

    def start(self) -> None:
        if self._running.is_set():
            return

        self._transport.open()
        self._running.set()
        self._rx_thread = threading.Thread(target=self._rx_loop, name="host-rx", daemon=True)
        self._rx_thread.start()

    def stop(self) -> None:
        self._running.clear()
        if self._rx_thread is not None:
            self._rx_thread.join(timeout=1.0)
            self._rx_thread = None
        self._transport.close()
        if self._logger is not None:
            self._logger.close()

    def subscribe(self, callback: Callable[[dict], None]) -> Callable[[], None]:
        return self._store.subscribe(callback)

    def get_latest(self) -> dict:
        return self._store.get_latest()

    def get_last_frame_age_ms(self) -> Optional[int]:
        return self._store.last_frame_age_ms()

    def set_speed(self, speed: int) -> None:
        payload = build_cmd_set_speed(speed, seq=self._next_seq())
        self._send(payload)

    def stop_motor(self) -> None:
        payload = build_cmd_stop(seq=self._next_seq())
        self._send(payload)

    def ping(self, nonce: Optional[int] = None) -> int:
        if nonce is None:
            nonce = int(time.time() * 1000) & 0xFFFFFFFF
        payload = build_cmd_ping(nonce, seq=self._next_seq())
        self._send(payload)
        return nonce

    def _next_seq(self) -> int:
        with self._tx_lock:
            seq = self._seq & 0xFF
            self._seq = (self._seq + 1) & 0xFF
            return seq

    def _send(self, frame: bytes) -> None:
        self._transport.write(frame)

    def _rx_loop(self) -> None:
        while self._running.is_set():
            try:
                data = self._transport.read(256)
                if not data:
                    continue
                for frame in self._parser.feed(data):
                    self._handle_frame(frame.msg_type, frame.payload)
            except Exception:
                time.sleep(0.1)

    def _handle_frame(self, msg_type: int, payload: bytes) -> None:
        if msg_type == MSG_TELEMETRY:
            sample = parse_telemetry_payload(payload)
            sample["host_rx_ms"] = int(time.time() * 1000)
            self._store.update(sample)
            if self._logger is not None:
                self._logger.log(sample)
            return

        if msg_type == MSG_PONG:
            self._last_pong = {
                "host_rx_ms": int(time.time() * 1000),
                "payload_hex": payload.hex(),
            }

