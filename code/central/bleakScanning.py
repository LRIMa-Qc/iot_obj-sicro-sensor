import asyncio
import subprocess
import os
from bleak import BleakClient, BleakScanner
from threading import Thread
from time import sleep, time
from queue import Queue
from device import Device


class BleakScanning():
    def __init__(self, send_data_cb, send_logs_cb, log_all: bool, adapter: str = "") -> None:
        self.__send_data_cb = send_data_cb
        self.__sleep_time = 0.01
        self.__input_buffer = Queue()
        self.__devices = {}
        if adapter is None or adapter == "":
            self.__adapter = self.get_usb_bluetooth_adapters()
        else:
            self.__adapter = adapter
        print("Adapter :", self.__adapter)
        # self.updated_devices = []
        self.new_sleep_value = None

        self.scanning = False
        self.__log_all = log_all
        self.__send_logs_cb = send_logs_cb
        # self.__read_thread = Thread(target=self.__read, daemon=True)
        # self.__read_thread.start()
        self.__input_buffer_parser_thread = Thread(target=self.__input_buffer_parser)
        self.__input_buffer_parser_thread.start()

    def __input_buffer_parser(self) -> None:
        '''Parse the input buffer'''
        while True:
            if  self.__input_buffer.empty(): # Check if the input buffer is empty
                sleep(self.__sleep_time)
                continue

            if self.__input_buffer.qsize() > 500:
                self.__send_logs_cb(f"[Warning] Buffer trop grand ({self.__input_buffer.qsize()}), suppression des anciennes entrées.")
                while self.__input_buffer.qsize() > 250:  # Garde au moins 250 éléments récents
                    self.__input_buffer.get(False)

                sleep(self.__sleep_time)  # Attendre un peu avant de reprendre
                continue

            line = self.__input_buffer.get(False)  # Get the data from the input buffer

            # print("\033[32mDATA: {}\033[0m".format(line))
            device = Device(line)

            if device.id == -1:  # Check if the device is valid
                if (self.__log_all): self.__send_logs_cb(f"[Error] The device is not valid for line: {line}")
                sleep(self.__sleep_time)
                continue

            if device.addr not in self.__devices:  # Check if the device is already in the list
                self.__devices[device.addr] = device  # Add the device to the list
                self.__send_data_cb(device)  # Send the data
                sleep(self.__sleep_time)
                continue

            # Check if the device has not the same id as last time
            # if device.id > self.__devices[device.addr].id or abs(device.id - self.__devices[device.addr].id) > 5:
            if device.id != self.__devices[device.addr].id:
                self.__send_data_cb(device)  # Send the data
                self.__devices[device.addr] = device  # Update the device
                sleep(self.__sleep_time)
                continue

            sleep(self.__sleep_time)
            continue

    def __is_valid(self, data) -> bool:
        if not isinstance(data, tuple):
            if (self.__log_all): self.__send_logs_cb(
                f"[Error] The data is not in the right format (need to be a tuple) for line: {data}")
            return False

        if len(data) != 3:
            if (self.__log_all): self.__send_logs_cb(
                f"[Error] The data is not in the right format (need to be with exactly 3 elements) for line: {data}")
            return False

        return True

    async def _start_scan(self):
        scanner = BleakScanner(detection_callback=self.detection_callback, adapter=self.__adapter,
                               cb=dict(use_bdaddr=True))
        try:
            async with scanner:
                await scanner.stop()
                await scanner.start()
                await asyncio.sleep(30.0)
                await scanner.stop()
        except Exception as e:
            print(f"An error occurred during BLE scanning: {e}")
            os.system("sudo reboot")

    async def __read(self):
        while True:
            try:
                if not self.scanning:
                    self.scanning = True
                    await self._start_scan()
                    self.scanning = False
            except Exception as e:
                print(f"An error occurred during BLE scanning: {e}")

            await asyncio.sleep(1.0)

    def get_usb_bluetooth_adapters(self):
        try:
            # Execute the hciconfig command to get details about Bluetooth adapters
            result = subprocess.run(['hciconfig'], stdout=subprocess.PIPE, text=True)
            output = result.stdout

            # Function to parse adapter details into JSON-like format
            def parse_to_json(data):
                devices = []
                current_device = {}
                for line in data.splitlines():
                    line = line.strip()

                    if not line:  # Blank line indicates end of current device
                        if current_device:
                            devices.append(current_device)
                            current_device = {}
                    elif line.startswith("hci"):  # Start of a new device
                        if current_device:
                            devices.append(current_device)
                        current_device = {"device": line.split(":")[0]}

                        # Extract type and usb fields
                        type_value = line.split(":")[2].strip().split()[0]
                        usb_value = line.split(":")[3].strip()
                        current_device["type"] = type_value
                        current_device["bus"] = usb_value

                    elif "BD Address" in line:
                        # Handle BD Address fields
                        parts = line.split("  ")  # Split by double spaces
                        for part in parts:
                            if ":" in part:
                                key, value = map(str.strip, part.split(":", 1))
                                current_device[key.lower().replace(" ", "_")] = value
                    elif "MTU" in line:
                        # Handle ACL and SCO MTU fields
                        parts = line.split("  ")
                        for part in parts:
                            if ":" in part:
                                key, value = map(str.strip, part.split(":", 1))
                                current_device[key.replace(" ", "_").lower()] = value
                    elif line.startswith("RX bytes") or line.startswith("TX bytes"):
                        # Handle RX and TX bytes with details
                        parts = line.split()
                        for part in parts:
                            if ":" in part:
                                key, value = map(str.strip, part.split(":", 1))
                                current_device[key.lower()] = value
                    else:
                        # Handle status or any uncategorized information
                        current_device["status"] = line

                if current_device:  # Add the last device if not already added
                    devices.append(current_device)

                return devices

            # Parse output and filter for "UP RUNNING" status
            devices_json = parse_to_json(output)
            up_running_usb_adapters = [
                device["device"] for device in devices_json
                if device.get("status") == "UP RUNNING" and device.get("bus") == "USB"
            ]

            return up_running_usb_adapters[0]  # List of USB hci names that are UP RUNNING
        except Exception as e:
            print(f"An error occurred while getting USB Bluetooth adapters: {e}")
            return None

    async def writeCharacteristics(self, device, value):
        try:
            async with BleakClient(device) as client:
                svcs = client.services

                for service in svcs:
                    if "afbe" in service.uuid:
                        # print(service)
                        for characteristic in service.characteristics:
                            if "faeb" in characteristic.uuid:
                                current_value_in_device = int.from_bytes(
                                    await client.read_gatt_char(characteristic.uuid), byteorder='little')
                                # print(device.name + ' : current sleep value : ' + str(current_value_in_device))
                                print(
                                    f'{device.name:<14} {" - current sleep value :":<20} {str(current_value_in_device)}')

                                if value < 1 or value > 65535:  # 65535 is the max value for a uint16
                                    print("Invalid value")
                                    continue

                                if current_value_in_device != value:
                                    await client.write_gatt_char(characteristic.uuid,
                                                                 value.to_bytes(2, byteorder="little"), response=True)
                                    # print(device.name + ' :     new sleep value : ' + str(value))
                                    print(f'{device.name:<14} {" - new sleep value :":<20} {str(value)}')



        except Exception as e:
            # print('ERROR in trying to read/write characteristics fun: writeCharacteristics')
            # print(type(e))
            pass

    def detection_callback(self, device, advertisement_data):
        if device.name is not None and "LRIMa" in device.name and "LRIMa conn" not in device.name:
            # print(f"Device: {device.name}, Address: {device.address}, Data: {advertisement_data.service_data}")
            line = (device.name, device.address, advertisement_data.service_data)
            if self.__is_valid(line):
                self.__input_buffer.put(line)

            if self.new_sleep_value is not None:
                asyncio.create_task(self.writeCharacteristics(device, self.new_sleep_value))

            with open('./last_received_time.txt', 'w') as f:
                f.write(f"{time()}")
        # elif device.name is not None and "LRIMa conn" in device.name:
        # print(f"LRIMACONN Device: {device.name}, Address: {device.address}")
        # if device.name not in self.updated_devices:
        # asyncio.run(self.writeCharacteristics(device, self.new_sleep_value))
        ####asyncio.create_task(self.writeCharacteristics(device, self.new_sleep_value))

        # self.updated_devices.append(device.name)

    def start_scanning(self):
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)

        try:
            loop.run_until_complete(self.__read())
            # loop.create_task(self.__read())
            # asyncio.create_task(self.__read())
        finally:
            loop.close()