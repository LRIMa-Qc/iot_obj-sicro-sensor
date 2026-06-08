import time
import threading
from datetime import datetime
from aliot.aliot_obj import AliotObj
from device import Device
from bleakScanning import BleakScanning
from core.aliot_sync import AliotSyncManager
from core.config import REBOOT_AFTER_INACTIVE_SECONDS, ERROR_LED_SEQUENCES
from core.health import InactivityWatchdog
from core.payload import SensorPayloadDecoder
from core.storage import LastReceivedTimeStore, OfflineCsvBuffer
from core.led_control import LedControl
from core.error_handling import ErrorHandler

sensor_iot = AliotObj("central")
decoder = SensorPayloadDecoder()
csv_buffer = OfflineCsvBuffer()
timestamp_store = LastReceivedTimeStore()
sync_manager = AliotSyncManager(sensor_iot, decoder, csv_buffer)
reader: BleakScanning | None = None


def handle_change_sleep(data):
    sync_manager.handle_change_sleep(data, reader)


def send_data(device: Device):
    sensors_values = decoder.decode(device.data)
    print(decoder.format_console_line(str(device.index), sensors_values, device.sleep_duration_sec))
    sync_manager.sync_device(device, sensors_values)


def send_logs(msg: str):
    sync_manager.send_logs(msg)


def on_connect_backedn():
    """Watchdog callback started by Aliot startup event."""
    watchdog = InactivityWatchdog(
        sensor_iot=sensor_iot,
        timestamp_store=timestamp_store,
        reboot_after_inactive=REBOOT_AFTER_INACTIVE_SECONDS,
        on_connected_start=lambda: handle_change_sleep(None),
    )
    watchdog.run()


def on_disconnect_backend():
    error_handler = ErrorHandler()
    error_handler.log_no_restart("Disconnected from backend. Will keep trying to reconnect...", ERROR_LED_SEQUENCES["disconnect_backend"])


if __name__ == "__main__":
    led_control = LedControl()
    led_control._set_manual_led_mode()
    led_control._led_on() # On before receiving data
    del led_control
    
    print("\n" * 5)
    print(f'Starting the program at {datetime.now().strftime("%d/%m/%Y %H:%M:%S")}')
    time.sleep(2)
    print("Started")

    # Setup bleak (to scan for sensors and receive data from them)
    reader = BleakScanning(send_data, send_logs, False)

    # Save the current time to a file
    timestamp_store.write(time.time())

    # Setup aliot
    sensor_iot.on_start(callback=on_connect_backedn)
    sensor_iot.on_end(callback=lambda: print("Aliot is stopping..."))
    sensor_iot.listen_doc(["/doc/sleep_time"], handle_change_sleep)
    # Start aliot
    aliot_thread = threading.Thread(target=sensor_iot.run, kwargs={"retry_time": 10})
    aliot_thread.start()

    # Start the reader thread
    reader_thread = threading.Thread(target=reader.start_scanning)
    reader_thread.start()
