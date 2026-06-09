"""Health monitoring and restart policy."""

from __future__ import annotations

import subprocess
import sys
import time
from collections.abc import Callable
from typing import Any

from core.storage import LastReceivedTimeStore
from core.error_handling import ErrorHandler
from core.config import ERROR_LED_SEQUENCES

class InactivityWatchdog:
    """Restart services if no data was received for too long."""

    def __init__(
        self,
        sensor_iot: Any,
        timestamp_store: LastReceivedTimeStore,
        reboot_after_inactive: int,
        on_connected_start: Callable[[], None] | None = None,
    ) -> None:
        self.sensor_iot = sensor_iot
        self.timestamp_store = timestamp_store
        self.reboot_after_inactive = reboot_after_inactive
        self.on_connected_start = on_connected_start
        self._error_handler = ErrorHandler()

    def run(self) -> None:
        if self.sensor_iot.connected_to_alivecode and self.on_connected_start is not None:
            self.on_connected_start()

        while True:
            last_received_time = self.timestamp_store.read_and_normalize()

            # Debug
            print(time.time() - last_received_time)

            if (time.time() - last_received_time) > self.reboot_after_inactive:
                self._error_handler.log_and_restart_service(
                    f"Restarting bluetooth service, no data received for {self.reboot_after_inactive} seconds", ERROR_LED_SEQUENCES["inactivity_watchdog"]
                )

            time.sleep(60)

    def _run_restart_command(self, command: list[str]) -> None:
        try:
            subprocess.run(command, check=True, timeout=10)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
            print(f"Restart command failed {command}: {exc}")
