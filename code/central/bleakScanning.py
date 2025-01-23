import asyncio
import re
import subprocess
from bleak import BleakClient, BleakScanner
from threading import Thread
from time import sleep
from queue import Queue
import os
from device import Device
import time

# Reboot the device if no data was received for 24 hours
REBOOT_AFTER_INACTIVE = 60 * 60 * 24 # 24 hours

class BleakScanning():
    def __init__(self, send_data_cb, send_logs_cb, log_all: bool, adapter: str = "") -> None:
        self.__send_data_cb = send_data_cb
        self.__sleep_time = 0.01
        self.__input_buffer = Queue() 
        self.__devices = {}
        # Last time data was received
        self.last_received_time = time.time()
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
            if self.last_received_time is not None and (time.time() - self.last_received_time) > REBOOT_AFTER_INACTIVE:
                print("Rebooting the device because no data was received for 24 hours")
                os.system("sudo reboot")
            
            if  self.__input_buffer.empty(): # Check if the input buffer is empty
                sleep(self.__sleep_time)
                continue
            line = self.__input_buffer.get(False) # Get the data from the input buffer

            # print("\033[32mDATA: {}\033[0m".format(line))
            device = Device(line)

            if device.id == -1: # Check if the device is valid
                if (self.__log_all) : self.__send_logs_cb(f"[Error] The device is not valid for line: {line}")
                sleep(self.__sleep_time)
                continue

            if device.addr not in self.__devices: # Check if the device is already in the list
                self.__devices[device.addr] = device # Add the device to the list
                self.__send_data_cb(device) # Send the data
                sleep(self.__sleep_time)
                continue

            # Check if the device has not the same id as last time
            # if device.id > self.__devices[device.addr].id or abs(device.id - self.__devices[device.addr].id) > 5:
            if device.id != self.__devices[device.addr].id:
                self.__send_data_cb(device) # Send the data
                self.__devices[device.addr] = device # Update the device
                sleep(self.__sleep_time)
                continue

            sleep(self.__sleep_time)
            continue

    def __is_valid(self, data) -> bool:
        if not isinstance(data, tuple):
            if (self.__log_all) : self.__send_logs_cb(f"[Error] The data is not in the right format (need to be a tuple) for line: {data}")
            return False

        if len(data) != 3:
            if (self.__log_all) :self.__send_logs_cb(f"[Error] The data is not in the right format (need to be with exactly 3 elements) for line: {data}")
            return False
        
        return True
    
    async def _start_scan(self):
        scanner = BleakScanner(detection_callback=self.detection_callback, adapter=self.__adapter, cb=dict(use_bdaddr=True))
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
            if self.last_received_time is not None:
                print(time.time() - self.last_received_time)
            if self.last_received_time is not None and (time.time() - self.last_received_time) > REBOOT_AFTER_INACTIVE:
                print("Rebooting the device because no data was received for 24 hours")
                os.system("sudo reboot")
            
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

            # Regular expression pattern to match the adapter details
            adapters_pattern = re.compile(r'^hci\d+:\s+Type:.*?Bus: (\w+)', re.MULTILINE)

            adapters = adapters_pattern.findall(output) # list [USB, UART]


            # Regular expression pattern to find "hci" followed by a number
            hci_pattern = r'hci\d+'

            # Find all matches in the text
            hci_identifiers = re.findall(hci_pattern, output) # list [hci1, hci0]

            for key, adapter in enumerate(adapters):
                if adapter == "USB":
                    return hci_identifiers[key]
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
                                    current_value_in_device = int.from_bytes(await client.read_gatt_char(characteristic.uuid), byteorder='little')
                                    # print(device.name + ' : current sleep value : ' + str(current_value_in_device))
                                    print(f'{device.name:<14} {" - current sleep value :":<20} {str(current_value_in_device)}')

                                    if value < 1 or value > 65535: #65535 is the max value for a uint16
                                        print("Invalid value")
                                        continue

                                    if current_value_in_device != value:
                                        await client.write_gatt_char(characteristic.uuid, value.to_bytes(2, byteorder="little"), response=True)
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
            
            self.last_received_time = time.time()
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
            
