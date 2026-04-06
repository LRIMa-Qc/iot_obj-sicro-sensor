"""Aliot synchronization manager."""

from __future__ import annotations

import time
from collections.abc import Mapping
from typing import Any

from core.config import DEFAULT_SLEEP_TIME, YELLOW
from core.payload import SensorPayloadDecoder
from core.storage import OfflineCsvBuffer


class AliotSyncManager:
    """Route decoded sensor data to Aliot or offline CSV depending on connectivity."""

    def __init__(self, sensor_iot: Any, decoder: SensorPayloadDecoder, csv_buffer: OfflineCsvBuffer) -> None:
        self.sensor_iot = sensor_iot
        self.decoder = decoder
        self.csv_buffer = csv_buffer

    def handle_change_sleep(self, data: dict[str, Any] | None, scanner: Any) -> None:
        if scanner is None:
            return

        if data is not None and "/doc/sleep_time" in data:
            raw_value = data["/doc/sleep_time"]
        else:
            raw_value = self.sensor_iot.get_doc("/doc/sleep_time") or DEFAULT_SLEEP_TIME

        try:
            value = int(raw_value)
        except (TypeError, ValueError):
            value = DEFAULT_SLEEP_TIME

        value = max(1, min(value, 0xFFFFFFFF))
        scanner.new_sleep_value = value

        print(f"New sleep time: {scanner.new_sleep_value}")

    def send_logs(self, msg: str) -> None:
        data = {"date": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), "text": msg}
        print(f"{YELLOW}LOG: {data['date']} - {data['text']}\033[0m")

        if self.sensor_iot.connected_to_alivecode:
            self.sensor_iot.update_component("log", data)

    def sync_device(self, device: Any, sensors_values: Mapping[int, float]) -> None:
        if self.sensor_iot.connected_to_alivecode:
            doc_json = self.decoder.build_doc_payload(
                device.index,
                device.id,
                sensors_values,
                getattr(device, "sleep", None),
            )
            self.sensor_iot.update_doc(doc_json)

            if self.csv_buffer.clear_after_reconnect():
                print("Reconnected to alivecode, cleared offline CSV buffer")
            return

        print("Not connected to alivecode, saving to CSV")
        self.csv_buffer.append(time.time(), device.index, sensors_values)
