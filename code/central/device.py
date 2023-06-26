from queue import Queue
from abc import ABC

class Device(ABC):

    def __init__(self, line) -> None:
        """
        Create a new device from the data
        
        Args:
            data (str): The data from the scan (format: {name,addr,service_data})
        """
        line = line.strip("{}") # Remove the first and last char
        val = line.split(",") # Split the string into a list

        self.__name = val[0]
        self.__addr = val[1]
        self.__data = val[2].split("-") # Split the data into a list
        self.__id = -1

        self.__index = self.__name[-1] #Get last char of the name
        
        # Check if it's the right service
        if self.__data[0:2] != ['ab', 'cd']:
            return None
        
        self.__data = self.__data[2:] # Remove the service id
        self.__data = [int(d, 16) for d in self.__data] # Convert the data from hex to int
       
        if self.__data[0] != 0: # Check if the first byte is 0
            return None

        self.__id = self.__data[1] # Set the id
    
    @property
    def index(self) -> str:
        """Get the index of the sensor"""
        return self.__index

    @property
    def id(self) -> int:
        '''Get the current id'''
        return self.__id

    @property
    def addr(self) -> str:
        '''Get the mac address of the Device'''
        return self.__addr

    @property
    def name(self) -> str:
        '''Get the name of the device'''
        return self.__name

    @property
    def data(self) -> int:
        '''Get the data'''
        return self.__data

    def getDataQueue(self) -> Queue:
        """Get the queue with the data (the id is removed)"""
        if len(self.__data) <= 2:
            return Queue(0)
        
        queue = Queue(0)

        # Loop for each ellement
        for d in self.__data[2:]: # Remove the id
            queue.put(d)

        return queue


    def __eq__(self, __value: object) -> bool:
        """Compare if the two devices are identical"""
        if not isinstance(object, Device):
            return False

        return self.__addr == object.addr and self.__name == object.name
