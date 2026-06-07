from abc import ABC
from ast import literal_eval


class Device(ABC):
    COMPANY_ID = 0x0059
    PAYLOAD_SIZE = 29
    LEGACY_PAYLOAD_SIZE = 25

    def __init__(self, line) -> None:
        """Create a new device from a scan record or legacy tuple."""
        self.__name = ""
        self.__addr = ""
        self.__data = {}
        self.__id = -1
        self.__index = "00"
        self.__node_id = -1
        self.__sleep_duration_sec = None
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
            # Service-data keys can be UUID strings; always use configured company ID bytes.
            company_id_bytes = [self.COMPANY_ID & 0xFF, (self.COMPANY_ID >> 8) & 0xFF]
        
        # Ensure byte_data is converted to a list of integers
        if isinstance(byte_data, (bytes, bytearray)):
            byte_list = list(byte_data)
        elif isinstance(byte_data, str):
            # If it's a string, try to interpret it as hex or ASCII
            try:
                # Try to interpret as hex string (e.g., "436f00002009...")
                byte_list = [int(byte_data[i:i+2], 16) for i in range(0, len(byte_data), 2)]
            except (ValueError, IndexError):
                # Fallback: treat as ASCII bytes
                byte_list = [ord(c) for c in byte_data]
        elif isinstance(byte_data, list):
            # Mixed list payloads: keep numeric entries only and skip UUID string chars.
            byte_list = []
            for b in byte_data:
                if isinstance(b, int):
                    byte_list.append(b)
                elif isinstance(b, (float, bool)):
                    byte_list.append(int(b))
                # else: skip string UUID characters like '0', '-', 'f', etc.
        else:
            byte_list = []
        
        # Prepend company ID only when it's not already in the payload bytes.
        if len(byte_list) >= 2 and byte_list[0] == company_id_bytes[0] and byte_list[1] == company_id_bytes[1]:
            payload = byte_list
        else:
            payload = company_id_bytes + byte_list
        
        self.__liste_int_data = payload

        if len(payload) >= self.LEGACY_PAYLOAD_SIZE and payload[0] == (self.COMPANY_ID & 0xFF):
            company_id = int.from_bytes(bytes(payload[0:2]), byteorder="little")
            if company_id != self.COMPANY_ID:
                return

            self.__node_id = int.from_bytes(bytes(payload[2:4]), byteorder="little")
            self.__index = f"{self.__node_id:02d}"
            self.__id = payload[4]

            if len(payload) >= self.PAYLOAD_SIZE:
                self.__sleep_duration_sec = int.from_bytes(bytes(payload[5:9]), byteorder="little")
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
        # Ensure we only return actual integer data, filtering out any remaining string characters
        # This handles edge cases where UUID strings might still be present
        return [x for x in self.__liste_int_data if isinstance(x, int)]

    @property
    def node_id(self) -> int:
        return self.__node_id

    @property
    def sleep_duration_sec(self):
        return self.__sleep_duration_sec

    def __eq__(self, __value: object) -> bool:
        if not isinstance(__value, Device):
            return False

        return self.__addr == __value.addr and self.__name == __value.name
