from queue import Queue
from abc import ABC

class Device(ABC):

    def __init__(self, line) -> None:
        """
        Create a new device from the data
        
        Args:
            data (str): The data from the scan (format is a Tuples: ('name', 'addr', service_data))

            name: LRIMa test 1
            addr: 00:00:00:00:00:00
            sercice_data: {'0000cdab-0000-1000-8000-00805f9b34fb': b'\\x00\\x95\\x01\\x160\\x02\\x18\\x02\\x03\\x1c6\\x04\\x16\\x15\\x05\\x00\\x00\\xfe\\x02\\\\'}
            
        """
        #GET NAME ADDR AND DATA
        self.__name = line[0]
        self.__addr = line[1]
        self.__data = line[2]
        
        #INIT THE ID
        self.__id = -1

        #GET INDEX OF DEVICE
        self.__index = self.__name[-1] #Get last char of the name
        
        #GET UUID AND DATA
        self.__uuid, self.__byte_data = next(iter(self.__data.items()))

        # CHECK IF ITS THE RIGHT SERVICE
        # Extract the first section before the hyphen
        first_section_uuid = self.__uuid.split('-')[0]

        # Check if 'a', 'b', 'c', and 'd' are present in the first section
        validity_uuid = all(letter in first_section_uuid for letter in ['a', 'b', 'c', 'd'])
        if not validity_uuid:
            return None 
        
        # CONVERT THE DATA FROM BYTES TO INT
        self.__liste_int_data = list(self.__byte_data)

        # CHECK IF THE FIRST BYTE IS 0
        if self.__liste_int_data[0] != 0:
            return None 
        
        # SET THE ID
        self.__id=self.__liste_int_data[1]
    
    @property
    def index(self) -> str:
        """Get the index of the sensor"""
        return self.__index

    @property
    def id(self) -> int:
        '''Get the current id'''
        return self.__id

    @property
    #CHANGE ONCE WE FIGURE OUT WHY 0000
    def addr(self) -> str:
        '''Get the mac address of the Device'''
        return self.__name

    @property
    def name(self) -> str:
        '''Get the name of the device'''
        return self.__name

    @property
    def data(self) -> int:
        '''Get the data'''
        return self.__liste_int_data

    def __eq__(self, __value: object) -> bool:
        """Compare if the two devices are identical"""
        if not isinstance(object, Device):
            return False

        return self.__addr == object.addr and self.__name == object.name
