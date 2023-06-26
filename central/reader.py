from threading import Thread
from queue import Queue
from time import sleep
import serial, re

from device import Device

class Reader():

    def __init__(self, port, baudrate, send_data_cb, send_logs_cb) -> None:
        self.__ser = serial.Serial(port, baudrate)
        self.__send_data_cb = send_data_cb
        self.__send_logs_cb = send_logs_cb
        self.__input_buffer = Queue() 
        self.__devices = {}
        self.__sleep_time = 0.01

        self.__read_thread = Thread(target=self.__read, daemon=True)
        self.__read_thread.start()

        self.__input_buffer_parser_thread = Thread(target=self.__input_buffer_parser)
        self.__input_buffer_parser_thread.start()

    def __read(self):
        while(True):
            line = ""
            err = ""
            if  self.__ser .in_waiting > 0:
                try:
                    line =  self.clean_str(self.__ser .readline().decode('utf-8').rstrip())
                except UnicodeDecodeError as e:
                    line =  self.clean_str(self.__ser .readline().decode('utf-8', 'replace').rstrip())
                    error_position = e.args[2]
                    err = f"[Error] Decoding error occurred at {error_position} for line: {line}"
                    
            if line == "": # Check if the line is empty
                sleep(self.__sleep_time)
                continue

            if err != "": # Check if there is an error
                self.__send_logs_cb(err) # Send the error
            
            if line[0] == "[" : # Check if the line is a log
                self.__send_logs_cb(line)
                sleep(self.__sleep_time)
                continue
            
            if not self.__is_valid(line): # Check if the data is valid
                sleep(self.__sleep_time)
                continue

            # Add the data to the input buffer so it's treated in order
            self.__input_buffer.put(line)

    
    def __input_buffer_parser(self) -> None:
        '''Parse the input buffer'''
        while True:
            if  self.__input_buffer.empty(): # Check if the input buffer is empty
                sleep(self.__sleep_time)
                continue
 
            line = self.__input_buffer.get(0) # Get the data from the input buffer

            print("\033[32mDATA: {}\033[0m".format(line))

            device = Device(line) # Create a new device from the data

            if device.id == -1: # Check if the device is valid
                self.__send_logs_cb(f"[Error] The device is not valid for line: {line}")
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
        '''Check if the data is valid in this format ({name,addr,data})'''
        if data[0] != "{" or data[-1] != "}": # Check if the data has the right format
            self.__send_logs_cb(f"[Error] The data is not in the right format for line: {data}")
            return False
        
        if data.count("{") > 1 or data.count("}") > 1: # Check if there is more than one {} in the data
            self.__send_logs_cb(f"[Error] There is more than one {{}} in the data for line: {data}")
            return False
        
        if data.count(",") != 2: # Check if there is the right amount of commas
            self.__send_logs_cb(f"[Error] There is not the right amount of commas for line: {data}")
            return False
        
        return True
    

    def clean_str(self, string) -> str:
        '''Remove all ANSI escape sequences and the prompt'''
        string = re.sub(r'\x1b\[.*?[@-~]', '', string)  # Remove all ANSI escape sequences
        string = re.sub(r'uart:~\$ ', '', string)  # Remove the prompt
        string = string.replace('^[[1;32mua', '')  # Remove the specified string

        return string
            

            
