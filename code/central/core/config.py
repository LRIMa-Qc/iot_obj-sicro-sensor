"""Shared configuration and constants for the central service."""

REBOOT_AFTER_INACTIVE_SECONDS = 60 * 30  # 30 min
DEFAULT_SLEEP_TIME = 30
LAST_RECEIVED_TIME_FILE = "last_received_time.txt"
OFFLINE_CSV_FILE = "data.csv"
ALIOT_MAX_REQUESTS_PER_SEC = 20
ALIOT_FLUSH_INTERVAL_SEC = 1 / ALIOT_MAX_REQUESTS_PER_SEC + 0.01  # small margin below the limit

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
