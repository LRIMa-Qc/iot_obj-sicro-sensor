"""Shared configuration and constants for the central service."""

REBOOT_AFTER_INACTIVE_SECONDS = 60 * 30  # 30 min
DEFAULT_SLEEP_TIME = 30
LAST_RECEIVED_TIME_FILE = "last_received_time.txt"
OFFLINE_CSV_FILE = "data.csv"

ERROR_LED_SEQUENCES = {
    "bluetooth_adapter_not_found": [2, 2, 2],
    "bluetooth_scan_failed": [0.5, 0.5, 0.5, 0.5],
    "disconnect_backend": [1, 1, 1],
    "inactivity_watchdog": [3, 3, 3, 3],
    "resource_not_ready": [0.2, 0.2, 0.2, 0.2, 0.2],
}

SENSOR_DEFAULTS = {
    1: -99,  # temp
    2: 0,  # hum
    6: 0,  # co2 (stored as ppm)
    3: 0,  # lum
    4: -99,  # gnd temp
    5: 0,  # gnd hum
    254: 0,  # bat
}

SENSOR_CSV_ORDER = (1, 2, 6, 3, 4, 5, 254)

DOWNLINK_ADV_NAME = "LRIMa Central"
DOWNLINK_ADV_DEVICE_ID = 0x0059

MAGENTA = "\033[95m"
GRAY = "\033[90m"
GREEN = "\033[92m"
RESET = "\033[0m"
RED = "\033[31m"
BOLD = "\033[1m"
YELLOW = "\033[33m"
