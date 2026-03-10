from __future__ import annotations

import csv
import json
import threading
from pathlib import Path
from typing import Optional


class TelemetryLogger:
    CSV_FIELDS = [
        "host_rx_ms",
        "timestamp_ms",
        "emg",
        "fsr2",
        "fsr1",
        "hall1",
        "hall2",
        "hall3",
        "motor_speed_pct",
        "motor_duty_pct",
        "motor_direction",
        "motor_fault_flags",
    ]

    def __init__(self, jsonl_path: Optional[str] = None, csv_path: Optional[str] = None) -> None:
        self._lock = threading.Lock()
        self._jsonl_file = None
        self._csv_file = None
        self._csv_writer = None

        if jsonl_path:
            p = Path(jsonl_path)
            p.parent.mkdir(parents=True, exist_ok=True)
            self._jsonl_file = p.open("a", encoding="utf-8")

        if csv_path:
            p = Path(csv_path)
            p.parent.mkdir(parents=True, exist_ok=True)
            new_file = not p.exists() or p.stat().st_size == 0
            self._csv_file = p.open("a", newline="", encoding="utf-8")
            self._csv_writer = csv.DictWriter(self._csv_file, fieldnames=self.CSV_FIELDS)
            if new_file:
                self._csv_writer.writeheader()
                self._csv_file.flush()

    def log(self, sample: dict) -> None:
        with self._lock:
            if self._jsonl_file is not None:
                self._jsonl_file.write(json.dumps(sample, ensure_ascii=True) + "\n")
                self._jsonl_file.flush()

            if self._csv_writer is not None and self._csv_file is not None:
                row = {field: sample.get(field) for field in self.CSV_FIELDS}
                self._csv_writer.writerow(row)
                self._csv_file.flush()

    def close(self) -> None:
        with self._lock:
            if self._jsonl_file is not None:
                self._jsonl_file.close()
                self._jsonl_file = None
            if self._csv_file is not None:
                self._csv_file.close()
                self._csv_file = None
                self._csv_writer = None

