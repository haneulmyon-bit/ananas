from __future__ import annotations

import threading
import time
from collections import deque
from typing import Callable, Deque, Dict, List, Optional

TelemetryCallback = Callable[[dict], None]


class TelemetryStore:
    def __init__(self, history_size: int = 2000) -> None:
        self._lock = threading.Lock()
        self._latest: Dict = {}
        self._history: Deque[dict] = deque(maxlen=history_size)
        self._subscribers: List[TelemetryCallback] = []
        self._last_update_monotonic: Optional[float] = None

    def update(self, sample: dict) -> None:
        callbacks: List[TelemetryCallback]
        with self._lock:
            self._latest = dict(sample)
            self._history.append(dict(sample))
            self._last_update_monotonic = time.monotonic()
            callbacks = list(self._subscribers)

        for callback in callbacks:
            try:
                callback(dict(sample))
            except Exception:
                continue

    def get_latest(self) -> dict:
        with self._lock:
            return dict(self._latest)

    def get_history(self, limit: int = 200) -> List[dict]:
        with self._lock:
            if limit <= 0:
                return []
            return list(self._history)[-limit:]

    def last_frame_age_ms(self) -> Optional[int]:
        with self._lock:
            if self._last_update_monotonic is None:
                return None
            age = (time.monotonic() - self._last_update_monotonic) * 1000.0
        return int(age)

    def subscribe(self, callback: TelemetryCallback) -> Callable[[], None]:
        with self._lock:
            self._subscribers.append(callback)

        def unsubscribe() -> None:
            with self._lock:
                if callback in self._subscribers:
                    self._subscribers.remove(callback)

        return unsubscribe

