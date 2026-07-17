"""Persistence helpers for timestamps and offline buffering."""

from __future__ import annotations

import threading
import time
from collections.abc import Mapping

from core.config import LAST_RECEIVED_TIME_FILE, OFFLINE_CSV_FILE, SENSOR_CSV_ORDER


class LastReceivedTimeStore:
    """Read and normalize the timestamp used by inactivity watchdog."""

    def __init__(self, path: str = LAST_RECEIVED_TIME_FILE) -> None:
        self.path = path

    def read_and_normalize(self) -> float:
        default_value = time.time()

        try:
            with open(self.path, "r", encoding="utf-8") as file:
                raw_content = file.read()
        except OSError:
            raw_content = ""

        values: list[float] = []
        for part in raw_content.replace(",", "\n").splitlines():
            part = part.strip()
            if not part:
                continue
            try:
                values.append(float(part))
            except ValueError:
                continue

        last_value = values[-1] if values else default_value

        try:
            with open(self.path, "w", encoding="utf-8") as file:
                file.write(f"{last_value}")
        except OSError as exc:
            print(f"Could not normalize {self.path}: {exc}")

        return last_value

    def write(self, timestamp: float) -> None:
        with open(self.path, "w", encoding="utf-8") as file:
            file.truncate(0)
            file.write(f"{timestamp}")


class OfflineCsvBuffer:
    """Thread-safe offline CSV buffering with read/clear support for reconnect flushing."""

    def __init__(self, path: str = OFFLINE_CSV_FILE) -> None:
        self.path = path
        self._lock = threading.Lock()

    def append(self, timestamp: float, device_index: str, device_id: int, sensors_values: Mapping[int, float]) -> None:
        with self._lock:
            with open(self.path, "a", encoding="utf-8") as file:
                row_values = [f"{timestamp}", device_index, str(device_id)] + [
                    str(sensors_values[sensor]) for sensor in SENSOR_CSV_ORDER
                ]
                file.write(",".join(row_values) + "\n")

    def read_rows(self) -> list[tuple[float, str, int, dict[int, float]]]:
        with self._lock:
            try:
                with open(self.path, "r", encoding="utf-8") as file:
                    lines = file.readlines()
            except OSError:
                return []

        expected_fields = 3 + len(SENSOR_CSV_ORDER)
        rows: list[tuple[float, str, int, dict[int, float]]] = []

        for line in lines:
            fields = line.strip().split(",")
            if len(fields) != expected_fields:
                continue

            try:
                timestamp = float(fields[0])
                device_index = fields[1]
                device_id = int(fields[2])
                sensors_values = {sensor: float(fields[3 + i]) for i, sensor in enumerate(SENSOR_CSV_ORDER)}
            except ValueError:
                continue

            rows.append((timestamp, device_index, device_id, sensors_values))

        return rows

    def clear(self) -> bool:
        try:
            with self._lock:
                with open(self.path, "w", encoding="utf-8") as file:
                    file.truncate(0)
            return True
        except OSError as exc:
            print(f"Could not clear {self.path} after reconnect: {exc}")
            return False
