import asyncio
from bleak import BleakScanner
from threading import Thread
from time import sleep
from queue import Queue

from device import Device

class BleakScanning():
    def __init__(self, send_data_cb, send_logs_cb, log_all: bool) -> None:
        self.__send_data_cb = send_data_cb
        self.__sleep_time = 0.01
        self.__input_buffer = Queue() 
        self.__devices = {}
        self.__log_all = log_all
        self.__send_logs_cb = send_logs_cb
        self.__read_thread = Thread(target=self.__read, daemon=True)
        self.__read_thread.start()
        self.__input_buffer_parser_thread = Thread(target=self.__input_buffer_parser)
        self.__input_buffer_parser_thread.start()

    def __input_buffer_parser(self) -> None:
        '''Parse the input buffer'''
        while True:
            if  self.__input_buffer.empty(): # Check if the input buffer is empty
                sleep(self.__sleep_time)
                continue
            line = self.__input_buffer.get(0) # Get the data from the input buffer

            print("\033[32mDATA: {}\033[0m".format(line))
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
            if device.id > self.__devices[device.addr].id or abs(device.id - self.__devices[device.addr].id) > 5:
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
    
    async def __read(self):
        while True:
            print("start bleak")
            scanner = BleakScanner()
            scanner.register_detection_callback(self.detection_callback)
            await scanner.start()
            await asyncio.sleep(10.0)  # Scan for 10 seconds
            await scanner.stop()

    def detection_callback(self, device, advertisement_data):
        if device.name is not None and "LRIMa" in device.name:
            line = (device.name, device.address, advertisement_data.service_data)
            if self.__is_valid(line): # Check if the data is valid
                self.__input_buffer.put(line)

    def start_scanning(self):
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        
        try:
            loop.run_until_complete(self.__read())
        finally:
            loop.close()
        
