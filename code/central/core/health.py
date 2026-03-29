"""Health monitoring and restart policy."""

from __future__ import annotations

import os
import sys
import time
from collections.abc import Callable
from typing import Any

from core.storage import LastReceivedTimeStore


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

    def run(self) -> None:
        if self.sensor_iot.connected_to_alivecode and self.on_connected_start is not None:
            self.on_connected_start()

        while True:
            last_received_time = self.timestamp_store.read_and_normalize()

            # Debug
            print(time.time() - last_received_time)

            if (time.time() - last_received_time) > self.reboot_after_inactive:
                print("Restarting bluetooth service, no data received for " f"{self.reboot_after_inactive} seconds")
                os.system("sudo systemctl restart bluetooth")
                os.system("pm2 restart all")
                sys.exit()

            time.sleep(60)
