import asyncio
from bleak import BleakScanner


async def discover_devices():
    print("start discover_devices")
    devices = await BleakScanner.discover(timeout=60)
    for device in devices:
        print(f"Device: {device.name}, Address: {device.address}")


asyncio.run(discover_devices())
