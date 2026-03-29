"""Shared configuration and constants for the central service."""

REBOOT_AFTER_INACTIVE_SECONDS = 60 * 30  # 30 min
DEFAULT_SLEEP_TIME = 30
LAST_RECEIVED_TIME_FILE = "last_received_time.txt"
OFFLINE_CSV_FILE = "data.csv"

SENSOR_DEFAULTS = {
    1: 99.99,  # temp
    2: 99.99,  # hum
    6: 99.99,  # co2 (stored as ppm)
    3: 99.99,  # lum
    4: 99.99,  # gnd temp
    5: 99.99,  # gnd hum
    254: 99.99,  # bat
}

SENSOR_CSV_ORDER = (1, 2, 6, 3, 4, 5, 254)

MAGENTA = "\033[95m"
GRAY = "\033[90m"
GREEN = "\033[92m"
RESET = "\033[0m"
RED = "\033[31m"
BOLD = "\033[1m"
YELLOW = "\033[33m"
