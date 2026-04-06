from abc import ABC
from ast import literal_eval


class Device(ABC):
    COMPANY_ID = 0x0059
    PAYLOAD_SIZE = 25

    def __init__(self, line) -> None:
        """Create a new device from a scan record or legacy tuple."""
        self.__name = ""
        self.__addr = ""
        self.__data = {}
        self.__id = -1
        self.__index = "00"
        self.__node_id = -1
        self.__liste_int_data = []

        if isinstance(line, str):
            try:
                line = literal_eval(line)
            except Exception:
                return

        if not isinstance(line, tuple) or len(line) != 3:
            return

        self.__name = line[0]
        self.__addr = line[1]
        self.__data = line[2]
        self.__index = self.__name[-2:] if len(self.__name) >= 2 else "00"

        if not self.__data:
            return

        # Extract company ID (UUID) and service data bytes from the dict
        company_id, byte_data = next(iter(self.__data.items()))
        
        # Convert company ID to little-endian bytes and prepend to payload
        if isinstance(company_id, int):
            company_id_bytes = [company_id & 0xFF, (company_id >> 8) & 0xFF]
        else:
            # If it's already bytes, use as-is
            company_id_bytes = list(company_id)
        
        payload = company_id_bytes + list(byte_data)
        self.__liste_int_data = payload

        if len(payload) >= self.PAYLOAD_SIZE and payload[0] == (self.COMPANY_ID & 0xFF):
            company_id = int.from_bytes(bytes(payload[0:2]), byteorder="little")
            if company_id != self.COMPANY_ID:
                return

            self.__node_id = int.from_bytes(bytes(payload[2:4]), byteorder="little")
            self.__index = f"{self.__node_id:02d}"
            self.__id = payload[4]
            return

        if len(payload) >= 2:
            self.__id = payload[1]

    @property
    def index(self) -> str:
        return self.__index

    @property
    def id(self) -> int:
        return self.__id

    @property
    def addr(self) -> str:
        return self.__addr

    @property
    def name(self) -> str:
        return self.__name

    @property
    def data(self) -> list[int]:
        return self.__liste_int_data

    def __eq__(self, __value: object) -> bool:
        if not isinstance(__value, Device):
            return False

        return self.__addr == __value.addr and self.__name == __value.name
