import time
import threading
import os
import sys
from datetime import datetime
from aliot.aliot_obj import AliotObj
from device import Device
from bleakScanning import BleakScanning

# Reboot the device if no data was received for 1 hour
REBOOT_AFTER_INACTIVE = 60 * 5 # 5min
# REBOOT_AFTER_INACTIVE = 4 * 60 * 60 * 1  # 4heure

sensor_iot = AliotObj("central")


def handle_change_sleep(data):
    if data is not None and "/doc/sleep_time" in data:
        reader.new_sleep_value = data["/doc/sleep_time"]
    else:
        reader.new_sleep_value = sensor_iot.get_doc("/doc/sleep_time") or 30  # type: ignore
    print(f"New sleep time: {reader.new_sleep_value}")


def send_data(device: Device):
    device_data = device.data
    sensors_values = {
        1: 99.99,  # temp
        2: 99.99,  # hum
        3: 99.99,  # lum
        4: 99.99,  # gnd temp
        5: 99.99,  # gnd hum
        254: 99.99,  # bat
    }

    data_mapping = {1: (3, 4), 2: (6, 7), 3: (9, 10), 4: (12, 13), 5: (15, 16), 254: (18, 19)}

    for sensor, (index1, index2) in data_mapping.items():
        whole_val = device_data[index1]
        decimal_val = device_data[index2]

        # Set the negative value
        if decimal_val > 99:
            decimal_val = decimal_val - 100
            whole_val = whole_val * -1

        sensors_values[sensor] = round(whole_val + (decimal_val / 100), 2)

    # print("Values received from device " + str(device.index))
    # print(f"\tTemperature: {sensors_values[1]}")
    # print(f"\tHumidity: {sensors_values[2]}")
    # print(f"\tLuminosity: {sensors_values[3]}")
    # print(f"\tGround temperature: {sensors_values[4]}")
    # print(f"\tGround humidity: {sensors_values[5]}")
    # print(f"\tBattery: {sensors_values[254]}")
    # datetime object containing current date and time
    current_time = datetime.now().strftime("%d/%m/%Y %H:%M:%S")

    # Colors
    MAGENTA = "\033[95m"
    GRAY = "\033[90m"
    GREEN = "\033[92m"
    RESET = "\033[0m"
    RED = "\033[31m"
    BOLD = "\033[1m"

    # dd/mm/YY H:M:S
    print(
        f"{BOLD}{MAGENTA}[{current_time}] {GREEN}ID: {str(device.index)} {RESET}| T: {sensors_values[1]}°C"
        f" {GRAY}| H: {sensors_values[2]}% {RESET}| L: {sensors_values[3]}% {GRAY}|"
        f" GT: {sensors_values[4]}°C {RESET}| GH: {sensors_values[5]}% | {RED}B: {sensors_values[254]}V{RESET}"
    )

    path = f"/doc/{device.index}"
    doc_json = {
        f"{path}/humidity": sensors_values[2],
        f"{path}/temperature": sensors_values[1],
        f"{path}/luminosite": sensors_values[3],
        f"{path}/gnd_temperature": sensors_values[4],
        f"{path}/gnd_humidity": sensors_values[5],
        f"{path}/batterie": sensors_values[254],
        f"{path}/id": device.id,
    }

    if sensor_iot.connected_to_alivecode:
        sensor_iot.update_doc(doc_json)
    else:
        print("Not connected to alivecode, saving to CSV")
        with open("data.csv", "a", encoding="utf-8") as f:
            f.write(
                f"{time.time()},"
                f"{device.index},"
                f"{sensors_values[1]},"
                f"{sensors_values[2]},"
                f"{sensors_values[3]},"
                f"{sensors_values[4]},"
                f"{sensors_values[5]},"
                f"{sensors_values[254]}\n"
            )


def send_logs(msg: str):
    data = {"date": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), "text": msg}

    print("\033[33m" + f"LOG: {data['date']} - {data['text']}" + "\033[0m")

    if sensor_iot.connected_to_alivecode:
        sensor_iot.update_component("log", data)

def start():
    """Main function"""

    # Set the sleep time by getting the value from the doc
    if sensor_iot.connected_to_alivecode:
        handle_change_sleep(None)

    # Loop to check last received time
    # if no data was received for 1 hour, reboot the device
    while True:
        last_received_time = None
        with open("last_received_time.txt", "r", encoding="utf-8") as file:
            last_received_time = float(file.read())

        # Debug
        if last_received_time is not None:
            print(time.time() - last_received_time)

        # Check if no data was received for 1 hour
        if last_received_time is not None and (time.time() - last_received_time) > REBOOT_AFTER_INACTIVE:
            print(f"Restarting bluetooth service, no data received for {REBOOT_AFTER_INACTIVE} seconds")
            os.system("sudo systemctl restart bluetooth")
            os.system("pm2 restart all")
            sys.exit() # quit

        # Sleep for 1 minute
        time.sleep(60)


if __name__ == "__main__":
    print("\n" * 5)
    print(f'Starting the program at {datetime.now().strftime("%d/%m/%Y %H:%M:%S")}')
    time.sleep(2)
    print("Started")

    # Setup bleak (to scan for sensors and receive data from them)
    reader = BleakScanning(send_data, send_logs, False)

    # Save the current time to a file
    with open("./last_received_time.txt", "w", encoding="utf-8") as f:
        f.truncate(0)
        f.write(f"{time.time()}")

    # Setup aliot
    sensor_iot.on_start(callback=start)
    sensor_iot.listen_doc(["/doc/sleep_time"], handle_change_sleep)
    # Start aliot
    aliot_thread = threading.Thread(target=sensor_iot.run, kwargs={"retry_time": 10})
    aliot_thread.start()

    # Start the reader thread
    reader_thread = threading.Thread(target=reader.start_scanning)
    reader_thread.start()
