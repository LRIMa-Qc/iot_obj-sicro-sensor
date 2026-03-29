"""Payload decoding and presentation helpers."""

from __future__ import annotations

from datetime import datetime
from typing import Mapping

from core.config import BOLD, GRAY, GREEN, MAGENTA, RED, RESET, SENSOR_DEFAULTS


class SensorPayloadDecoder:
    """Decode sensor payloads from BLE packets into typed values."""

    def __init__(self, sensor_defaults: Mapping[int, float] | None = None) -> None:
        self._sensor_defaults = dict(sensor_defaults or SENSOR_DEFAULTS)

    def decode(self, device_data: list[int]) -> dict[int, float]:
        sensors_values = dict(self._sensor_defaults)

        # Payload after packet id is encoded in triplets: [sensor_id, whole, decimal].
        for i in range(2, len(device_data), 3):
            if i + 2 >= len(device_data):
                break

            sensor = device_data[i]
            if sensor not in sensors_values:
                continue

            whole_val = device_data[i + 1]
            decimal_val = device_data[i + 2]

            if decimal_val > 99:
                decimal_val = decimal_val - 100
                whole_val = whole_val * -1

            decoded_value = round(whole_val + (decimal_val / 100), 2)

            # CO2 is packed as ppm/10 on the broadcaster side.
            if sensor == 6:
                decoded_value = round(decoded_value * 10, 2)

            sensors_values[sensor] = decoded_value

        return sensors_values

    def format_console_line(self, device_index: str, sensors_values: Mapping[int, float]) -> str:
        current_time = datetime.now().strftime("%d/%m/%Y %H:%M:%S")
        return (
            f"{BOLD}{MAGENTA}[{current_time}] {GREEN}ID: {device_index} {RESET}| T: {sensors_values[1]}°C"
            f" {GRAY}| H: {sensors_values[2]}% {RESET}| L: {sensors_values[3]}% {GRAY}|"
            f" GT: {sensors_values[4]}°C {RESET}| GH: {sensors_values[5]}% | {RED}B: {sensors_values[254]}V{RESET}"
        )

    def build_doc_payload(
        self, device_index: str, device_id: int, sensors_values: Mapping[int, float]
    ) -> dict[str, float | int]:
        path = f"/doc/{device_index}"
        return {
            f"{path}/humidity": sensors_values[2],
            f"{path}/temperature": sensors_values[1],
            f"{path}/co2": sensors_values[6],
            f"{path}/luminosite": sensors_values[3],
            f"{path}/gnd_temperature": sensors_values[4],
            f"{path}/gnd_humidity": sensors_values[5],
            f"{path}/batterie": sensors_values[254],
            f"{path}/id": device_id,
        }
