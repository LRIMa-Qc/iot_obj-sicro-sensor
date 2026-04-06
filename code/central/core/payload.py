"""Payload decoding and presentation helpers."""

from __future__ import annotations

from datetime import datetime
from typing import Mapping

from core.config import BOLD, DEFAULT_SLEEP_TIME, GRAY, GREEN, MAGENTA, RED, RESET, SENSOR_DEFAULTS


class SensorPayloadDecoder:
    """Decode sensor payloads from BLE packets into typed values."""

    COMPANY_ID = 0x0059
    PAYLOAD_SIZE = 29
    LEGACY_PAYLOAD_SIZE = 25

    TEMP_BIT = 0
    HUM_BIT = 1
    LUM_BIT = 2
    CO2_BIT = 3
    GND_TEMP_BIT = 4
    GND_HUM_BIT = 5
    BAT_BIT = 6

    def __init__(self, sensor_defaults: Mapping[int, float] | None = None) -> None:
        self._sensor_defaults = dict(sensor_defaults or SENSOR_DEFAULTS)

    def _decode_fixed_frame(self, device_data: list[int]) -> dict[int, float]:
        sensors_values = dict(self._sensor_defaults)

        if len(device_data) < self.LEGACY_PAYLOAD_SIZE:
            return sensors_values

        company_id = int.from_bytes(bytes(device_data[0:2]), byteorder="little")
        if company_id != self.COMPANY_ID:
            return sensors_values

        if len(device_data) >= self.PAYLOAD_SIZE:
            present_mask = device_data[25]
            temp_off = 9
            hum_off = 11
            lum_off = 13
            co2_off = 17
            gnd_temp_off = 19
            gnd_hum_off = 21
            bat_off = 23
        else:
            present_mask = device_data[21]
            temp_off = 5
            hum_off = 7
            lum_off = 9
            co2_off = 13
            gnd_temp_off = 15
            gnd_hum_off = 17
            bat_off = 19

        if present_mask & (1 << self.TEMP_BIT):
            sensors_values[1] = round(
                int.from_bytes(bytes(device_data[temp_off : temp_off + 2]), byteorder="little", signed=True) / 100,
                2,
            )

        if present_mask & (1 << self.HUM_BIT):
            sensors_values[2] = round(
                int.from_bytes(bytes(device_data[hum_off : hum_off + 2]), byteorder="little") / 100,
                2,
            )

        if present_mask & (1 << self.LUM_BIT):
            sensors_values[3] = float(int.from_bytes(bytes(device_data[lum_off : lum_off + 4]), byteorder="little"))

        if present_mask & (1 << self.CO2_BIT):
            sensors_values[6] = float(int.from_bytes(bytes(device_data[co2_off : co2_off + 2]), byteorder="little"))

        if present_mask & (1 << self.GND_TEMP_BIT):
            sensors_values[4] = round(
                int.from_bytes(bytes(device_data[gnd_temp_off : gnd_temp_off + 2]), byteorder="little", signed=True)
                / 100,
                2,
            )

        if present_mask & (1 << self.GND_HUM_BIT):
            sensors_values[5] = round(
                int.from_bytes(bytes(device_data[gnd_hum_off : gnd_hum_off + 2]), byteorder="little") / 100,
                2,
            )

        if present_mask & (1 << self.BAT_BIT):
            sensors_values[254] = round(
                int.from_bytes(bytes(device_data[bat_off : bat_off + 2]), byteorder="little") / 100,
                2,
            )

        return sensors_values

    def _decode_legacy_frame(self, device_data: list[int]) -> dict[int, float]:
        sensors_values = dict(self._sensor_defaults)

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

            if sensor == 6:
                decoded_value = round(decoded_value * 10, 2)

            sensors_values[sensor] = decoded_value

        return sensors_values

    def format_console_line(
        self,
        device_index: str,
        sensors_values: Mapping[int, float],
        sleep_duration_sec: int | None = None,
    ) -> str:
        current_time = datetime.now().strftime("%d/%m/%Y %H:%M:%S")
        defaults = self._sensor_defaults
        sleep_text = "?" if sleep_duration_sec is None else str(int(sleep_duration_sec))
        return (
            f"{BOLD}{MAGENTA}[{current_time}] {GREEN}ID: {device_index} {RESET}| T: {sensors_values.get(1, defaults[1])}°C"
            f" {GRAY}| H: {sensors_values.get(2, defaults[2])}% {RESET}| L: {sensors_values.get(3, defaults[3])}% {GRAY}|"
            f" {GRAY} CO2: {sensors_values.get(6, defaults[6])}ppm {RESET}|"
            f" GT: {sensors_values.get(4, defaults[4])}°C {RESET}| GH: {sensors_values.get(5, defaults[5])}% |"
            f" Sleep: {sleep_text}s {RESET}| {RED}B: {sensors_values.get(254, defaults[254])}V{RESET}"
        )

    def build_doc_payload(
        self,
        device_index: str,
        device_id: int,
        sensors_values: Mapping[int, float],
        sleep_duration_sec: int | None = None,
    ) -> dict[str, float | int]:
        path = f"/doc/{device_index}"
        defaults = self._sensor_defaults
        sleep_value = DEFAULT_SLEEP_TIME if sleep_duration_sec is None else int(sleep_duration_sec)
        return {
            f"{path}/humidity": sensors_values.get(2, defaults[2]),
            f"{path}/temperature": sensors_values.get(1, defaults[1]),
            f"{path}/co2": sensors_values.get(6, defaults[6]),
            f"{path}/luminosite": sensors_values.get(3, defaults[3]),
            f"{path}/gnd_temperature": sensors_values.get(4, defaults[4]),
            f"{path}/gnd_humidity": sensors_values.get(5, defaults[5]),
            f"{path}/batterie": sensors_values.get(254, defaults[254]),
            f"{path}/sleep_duration_sec": sleep_value,
            f"{path}/id": device_id,
        }
    
    def decode(self, device_data: list[int]) -> dict[int, float]:
        """Decode device data, handling mixed types and UUID string prefixes."""
        if not device_data:
            return dict(self._sensor_defaults)
        
        try:
            # Filter to only integers and floats, skip all strings (including numeric UUID chars)
            clean_data = [int(x) if isinstance(x, float) else x for x in device_data if isinstance(x, (int, float))]
            if not clean_data:
                return dict(self._sensor_defaults)
            device_data = clean_data
        except (ValueError, TypeError, AttributeError):
            return dict(self._sensor_defaults)

        if len(device_data) >= self.LEGACY_PAYLOAD_SIZE and int.from_bytes(bytes(device_data[0:2]), "little") == self.COMPANY_ID:
            return self._decode_fixed_frame(device_data)

        return self._decode_legacy_frame(device_data)
