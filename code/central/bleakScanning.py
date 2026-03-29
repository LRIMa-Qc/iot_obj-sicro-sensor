import asyncio
from threading import Thread
from time import sleep, time
from queue import Queue, Full, Empty
import subprocess
import os
import sys
from collections import OrderedDict
from bleak import BleakClient, BleakScanner, BleakError, BleakGATTProtocolError
from device import Device

# --- Constants ---
DEFAULT_SLEEP_TIME = 0.005
INPUT_BUFFER_MAX_SIZE = 500
WRITE_QUEUE_MAX_SIZE = 200
DISCOVERED_DEVICES_MAX_SIZE = 500
WRITE_TRACK_TTL_SECONDS = 60 * 60
LAST_RECEIVED_TIME_WRITE_INTERVAL_SECONDS = 5
SCAN_DURATION_SECONDS = 5
WRITE_DELAY_SECONDS = 0.1
BLEAK_CLIENT_TIMEOUT = 5
CHARACTERISTIC_VALUE_MIN = 1
CHARACTERISTIC_VALUE_MAX = 65535  # uint16 max value
COOLDOWN_MIN_SECONDS = 10
LAST_RECEIVED_TIME_FILE = "./last_received_time.txt"
INVALID_BLUETOOTH_ADDRESS = "00:00:00:00:00:00"
LRIMA_NAME_PREFIX = "LRIMa"
LRIMA_CONN_SUBSTRING = "LRIMa conn"
SERVICE_UUID_SUBSTRING = "afbe"
CHAR_UUID_SUBSTRING = "faeb"
# ------------------


class BleakScanning:
    def __init__(self, send_data_cb, send_logs_cb, log_all: bool, adapter: str = "") -> None:
        self.__send_data_cb = send_data_cb
        self.__send_logs_cb = send_logs_cb
        self.__log_all = log_all

        self.__sleep_time = DEFAULT_SLEEP_TIME
        self.__input_buffer = Queue(maxsize=INPUT_BUFFER_MAX_SIZE)
        self.__devices = {}
        self.discovered_devices = OrderedDict()
        self.last_write_time = {}
        self._pending_write_tasks = set()
        self._last_received_time_write = 0.0
        self.new_sleep_value = None
        self.scanning = False

        self.__adapter = adapter or self.get_usb_bluetooth_adapters()
        if not self.__adapter:
            raise ValueError("No USB Bluetooth adapter found.")
        print(f"Using adapter: {self.__adapter}")

        self.__input_buffer_parser_thread = Thread(target=self.__input_buffer_parser, daemon=True)
        self.__input_buffer_parser_thread.start()

        self.write_lock = asyncio.Lock()
        self.write_queue = asyncio.Queue(maxsize=WRITE_QUEUE_MAX_SIZE)
        self.scan_lock = asyncio.Lock()

    def __input_buffer_parser(self) -> None:
        while True:
            if self.__input_buffer.empty():
                sleep(self.__sleep_time)
                continue

            try:
                line = self.__input_buffer.get(False)
            except Empty:
                sleep(self.__sleep_time)
                continue

            device = Device(line)

            if device.id == -1:
                if self.__log_all:
                    self.__send_logs_cb(f"[Error] Invalid device for line: {line}")
                sleep(self.__sleep_time)
                continue

            existing = self.__devices.get(device.addr)
            if not existing or device.id != existing.id:
                self.__devices[device.addr] = device
                self.__send_data_cb(device)

            sleep(self.__sleep_time)

    def __is_valid(self, data) -> bool:
        if not isinstance(data, tuple) or len(data) != 3:
            if self.__log_all:
                self.__send_logs_cb(f"[Error] Invalid data format: {data}")
            return False
        return True

    def get_usb_bluetooth_adapters(self):
        try:
            result = subprocess.run(["hciconfig"], stdout=subprocess.PIPE, text=True, check=True)
            output = result.stdout

            def parse_to_json(data):
                devices, current_device = [], {}
                for line in data.splitlines():
                    line = line.strip()
                    if not line:
                        if current_device:
                            devices.append(current_device)
                            current_device = {}
                        continue
                    if line.startswith("hci"):
                        if current_device:
                            devices.append(current_device)
                        parts = line.split(":")
                        current_device = {
                            "device": parts[0],
                            "type": parts[2].strip().split()[0],
                            "bus": parts[3].strip(),
                        }
                        continue
                    if "BD Address" in line:
                        for part in line.split("  "):
                            if ":" in part:
                                k, v = map(str.strip, part.split(":", 1))
                                current_device[k.lower().replace(" ", "_")] = v
                        continue
                    if "MTU" in line or line.startswith(("RX bytes", "TX bytes")):
                        for part in line.split():
                            if ":" in part:
                                k, v = map(str.strip, part.split(":", 1))
                                current_device[k.lower()] = v
                        continue
                    current_device["status"] = line
                if current_device:
                    devices.append(current_device)
                return devices

            devices = parse_to_json(output)
            usb_adapters = [d["device"] for d in devices if d.get("status") == "UP RUNNING" and d.get("bus") == "USB"]
            return usb_adapters[0] if usb_adapters else None
        except Exception as e:
            print(f"Error getting USB Bluetooth adapters: {e}")
            return None

    async def __read(self):
        while True:
            # Scan phase
            await self.__start_scan_until_write()
            # Write phase
            await self.perform_batch_writes()

    async def __start_scan_until_write(self):
        async with self.scan_lock:
            try:
                # Build scanner kwargs - use bluez parameter for Bleak 3.0+
                scanner_kwargs = {
                    "detection_callback": self.detection_callback,
                    "cb": {"use_bdaddr": True},  # use_bdaddr is a workaround for macOS
                }
                if self.__adapter:
                    scanner_kwargs["bluez"] = {"adapter": self.__adapter}

                scanner = BleakScanner(**scanner_kwargs)

                async with scanner:
                    await scanner.stop()  # Ensure clean start
                    await scanner.start()
                    self.scanning = True

                    # Wait for write_queue not empty
                    while True:

                        if not self.write_queue.empty():
                            break
                        await asyncio.sleep(self.__sleep_time)
                await scanner.stop()
            except (BleakError, asyncio.TimeoutError) as e:
                print(f"[Scan Error] {e}")
                self.scanning = False
                self._run_restart_command(["sudo", "systemctl", "restart", "bluetooth"])
                self._run_restart_command(["pm2", "restart", "all"])
                sys.exit()  # quit
            finally:
                self.scanning = False
                await asyncio.sleep(0.1)  # Small delay to ensure scanner stops properly

    async def perform_batch_writes(self):
        if self.write_queue.empty():
            return

        while not self.write_queue.empty():
            device, value = await self.write_queue.get()
            await self._write_characteristic(device, value)
            await asyncio.sleep(WRITE_DELAY_SECONDS)  # Small delay to avoid overwhelming the device

    async def _write_characteristic(self, device, value):
        async with self.write_lock:
            bleak_device = self.discovered_devices.get(device.address)
            if not bleak_device:
                print(f"[Cache Miss] Device not found: {device.name} [{device.address}]")
                self.write_queue.task_done()
                return

            if not (CHARACTERISTIC_VALUE_MIN <= value <= CHARACTERISTIC_VALUE_MAX):
                print(f"[Invalid Value] {device.name} [{device.address}]: {value}")
                self.write_queue.task_done()
                return

            try:
                async with BleakClient(bleak_device, timeout=BLEAK_CLIENT_TIMEOUT) as client:
                    for service in client.services:
                        if SERVICE_UUID_SUBSTRING not in service.uuid:
                            continue

                        for char in service.characteristics:
                            if CHAR_UUID_SUBSTRING not in char.uuid:
                                continue

                            current_value_in_device = int.from_bytes(
                                await client.read_gatt_char(char.uuid), byteorder="little"
                            )

                            if current_value_in_device == value:
                                print(f"[Unchanged sleep] {device.name} [{device.address}]: {current_value_in_device}")
                                break

                            await client.write_gatt_char(char.uuid, value.to_bytes(2, "little"), response=True)
                            print(
                                f"[Updated sleep] {device.name} [{device.address}]: {current_value_in_device} → {value}"
                            )
                        break
            except asyncio.TimeoutError:
                print(f"[Connection Timeout] {device.name} [{device.address}]")
            except EOFError:
                print(f"[Connection Close Error] {device.name} [{device.address}]")
            except BleakGATTProtocolError as e:
                # GATT protocol errors in Bleak 3.0+ are wrapped in BleakGATTProtocolError
                print(f"[GATT Protocol Error] {device.name} [{device.address}]: Code {e.code} - {e!r}")
            except (BleakError, ValueError) as e:
                print(f"[Write Failed] {device.name} [{device.address}]: {e!r}")

    async def write_characteristics(self, device, value):

        # Prevent multiple writes queued for the same device
        already_queued = any(d.address == device.address for d, _ in self.write_queue._queue)
        if already_queued:
            print(f"[Write Already Queued] {device.name} [{device.address}]")
            return

        if self.write_queue.full():
            print(f"[Write Queue Full] Skipping write for {device.name} [{device.address}]")
            return

        await self.write_queue.put((device, value))

    def _track_write_task(self, coro):
        task = asyncio.create_task(coro)
        self._pending_write_tasks.add(task)

        def _finalize(done_task):
            self._pending_write_tasks.discard(done_task)
            try:
                done_task.result()
            except Exception as exc:
                print(f"[Write Task Error] {exc!r}")

        task.add_done_callback(_finalize)

    def _update_discovered_device_cache(self, device):
        address = device.address
        self.discovered_devices[address] = device
        self.discovered_devices.move_to_end(address)

        while len(self.discovered_devices) > DISCOVERED_DEVICES_MAX_SIZE:
            removed_addr, _ = self.discovered_devices.popitem(last=False)
            self.last_write_time.pop(removed_addr, None)

    def _prune_write_tracking(self, now):
        stale_addresses = [
            address for address, ts in self.last_write_time.items() if now - ts > WRITE_TRACK_TTL_SECONDS
        ]
        for address in stale_addresses:
            self.last_write_time.pop(address, None)

    def _write_last_received_timestamp(self, now):
        if (now - self._last_received_time_write) < LAST_RECEIVED_TIME_WRITE_INTERVAL_SECONDS:
            return

        self._last_received_time_write = now
        with open(LAST_RECEIVED_TIME_FILE, "w", encoding="utf-8") as f:
            f.truncate(0)
            f.write(f"{now}")

    def _run_restart_command(self, command):
        try:
            subprocess.run(command, check=True, timeout=10)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
            print(f"[Restart Command Failed] {command}: {exc}")

    def detection_callback(self, device, advertisement_data):
        if device.address == INVALID_BLUETOOTH_ADDRESS:
            return

        name = device.name
        if not name or LRIMA_NAME_PREFIX not in name or LRIMA_CONN_SUBSTRING in name:
            return

        self._update_discovered_device_cache(device)

        # Parse the advertisement data
        line = (name, device.address, advertisement_data.service_data)
        if self.__is_valid(line):
            try:
                self.__input_buffer.put_nowait(line)
            except Full:
                try:
                    self.__input_buffer.get_nowait()
                except Empty:
                    pass

                try:
                    self.__input_buffer.put_nowait(line)
                except Full:
                    pass

        self.handle_change_sleep(device)
        self._write_last_received_timestamp(time())

    def handle_change_sleep(self, device):
        if self.new_sleep_value is None:
            return

        now = time()
        cooldown = max(COOLDOWN_MIN_SECONDS, self.__sleep_time / 2)
        last = self.last_write_time.get(device.address, 0)

        if now - last < cooldown:
            print(
                f"[Write Skipped] Cooldown active for {device.name} "
                f"[{device.address}] ({now - last:.1f}s since last attempt)"
            )
            return

        self._prune_write_tracking(now)
        self.last_write_time[device.address] = now

        self._track_write_task(self.write_characteristics(device, self.new_sleep_value))

    def start_scanning(self):
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        try:
            loop.run_until_complete(self.__read())
        finally:
            pending = asyncio.all_tasks(loop)
            for task in pending:
                task.cancel()
            if pending:
                loop.run_until_complete(asyncio.gather(*pending, return_exceptions=True))
            loop.close()
