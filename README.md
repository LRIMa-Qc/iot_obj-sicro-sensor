# iot_obj-sicro-sensor

Authors : LRIMa (Laboratoire de Recherche Informatique de Maisonneuve)

## Repo composition

```bash
code/
├── actioneur       (esp32 code)
├── broadcaster     (sensor code [nrf52833])
├── central         (python BLE scanning and ALIVEcode integration)
└── central-nrf     (BLE receiver [nrf52840dongle])
schematic/
├── lrima-main/     (Main board KiCad project)
└── lrima-ground/   (Ground board KiCad project)
models/
└── sensor-casing/  (3D CAD models for enclosure)
```

### Physical device

The schematics and PCB were designed using [KiCad](https://www.kicad.org/) EDA software
There is a protective layer that needs to be applied on the ground part of the pcb to prevent corrosion

## Building

#### Tools needed

- VS Code
- [nRF Connect](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop)
- [nRF Connect VS Code extension](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code)
- nRF toolchain `v3.2.4`
- nRF SDK `v3.2.4`
- J-Link
- Python 3.8+ (for central application)

#### Broadcaster

##### Preparing the environment

Before building the broadcaster program, you **must** make sure that you changed the `CONFIG_BLE_USER_DEFINED_MAC_ADDR` and the `CONFIG_BLE_USER_DEFINED_NAME` so that they are unique. Note that the `CONFIG_BLE_USER_DEFINED_NAME` must end with a number (ex : 01). If you need mac address, you can use this [mac address generator](https://dnschecker.org/mac-address-generator.php).

##### Building

The broadcaster uses the **sysbuild** system from NCS v3.2.4. To build the program for the nrf52833 using the [nRF Connect VS Code extension](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code):

1. Open the broadcaster folder in VS Code
2. Use the nRF Connect extension to build using the `lrima_greenhouse_nrf52833` board
3. Flash using the [extension flashing options](https://nrfconnect.github.io/vscode-nrf-connect/get_started/quick_debug.html#how-to-flash-an-application)

Note: If flashing fails, use the `erase and flash` option to perform a full erase first.

###### J-Link setup

![J-Link setup](doc/img/J-Link-Pin-2.jpg)
![J-Link setup 2](doc/img/J-Link-Pin-3.jpg)
![J-Link setup 3](doc/img/J-Link-Pin.png)

#### Firmware over the air update (DFU)

The board supports over-the-air firmware updates. To prepare an update:

1. Build the application as described above
2. Locate the signed image in the sysbuild app domain: `build/broadcaster/zephyr/zephyr.signed.bin`
3. Transfer this file to your phone
4. Press and hold button1 until the LED is on, then release
5. You will have up to 60 seconds (configurable via `CONFIG_BLE_DFU_ADV_DURATION_SEC`) to start the update
6. Use the [nRF Connect mobile app](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-mobile) to connect to the device (named with your `CONFIG_BLE_USER_DEFINED_NAME`)
7. Go to the `DFU` tab and select `zephyr.signed.bin`
8. Press `Upload` and monitor the progress

![Button1](doc/img/Button1.png)

#### Device Functionality

The broadcaster uses a single BLE advertising workflow. It sends sensor data uplink and then opens a short scan window to receive a downlink sleep update when needed. The user can wake up the device at any time by pressing button1. There is never a connection established, and all communication is done through advertisements and scan responses.

##### Broadcaster

The broadcaster collects sensor data and transmits it BLE advertisements:

**Supported sensors:**

- Temperature/Humidity (SHT4X)
- Soil temperature/moisture/conductivity (STCC4)
- CO2 concentration

**Transmission behavior:**

- Sends data at intervals specified by `CONFIG_SENSOR_SLEEP_DURATION_SEC`
- Uses BLE advertising for the duration specified in `CONFIG_BLE_ADV_DURATION_SEC`
- Publishes a 29-byte service payload containing company ID, node ID, sequence, sleep duration, sensor values, and a presence bitmap
- Opens an active scan window after uplink to receive sleep updates sent by the central
- Beacon payload structure is shown below:

![Broadcaster data.](doc/img/broadcaster-data.png)

> **Note:** The diagram above predates the latest payload format. See the broadcaster documentation for the full 29-byte layout and presence bitmap details.

### Central-nrf

The central-nrf firmware runs on the nRF52840 dongle to receive BLE broadcasts from the broadcaster.

**Building:**

1. Use the [nRF Connect VS Code extension](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code) to build for `nrf52840dongle_nrf52840` board
2. Flash using the nRF Connect for Desktop programmer tools
3. Put the device in [DFU mode](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fug_nrf52840_dongle%2FUG%2Fnrf52840_Dongle%2Fprogramming.html)
4. Flash the `build/zephyr/zephyr.hex` file

### Central (Python Application)

The central application handles BLE scanning, data collection, and integration with ALIVEcode.

**Core modules:**

- `core/storage.py` - Persistent data management
- `core/payload.py` - Sensor data parsing and validation
- `core/aliot_sync.py` - ALIVEcode cloud integration
- `core/downlink_advertiser.py` - BlueZ D-Bus advertiser for sleep-time downlink frames
- `core/health.py` - System health monitoring
- `core/config.py` - Configuration management

**Setup and running:**

1. Plug the Central-nrf dongle into a computer or Raspberry Pi
2. Configure the application:
   - Copy `config.ini.example` to `config.ini`
   - Set your `object_id` and `auth_token` from your [ALIVEcode.ca](https://ALIVEcode.ca) IoT project

3. Install dependencies:

   ```bash
   # For Raspberry Pi 4
   pip install -r requirements_pi4.txt

   # For Raspberry Pi 5
   pip install -r requirements_pi5.txt
   ```

   (Using a Python virtual environment is recommended)

   `dbus-next` is required because downlink sleep updates are emitted through BlueZ D-Bus advertising APIs.

4. Run the application:

   ```bash
   python main.py
   ```

5. Power on the broadcaster devices

The application will automatically scan for and connect to broadcaster devices, collecting sensor data and synchronizing with ALIVEcode.
It publishes each node under `/doc/<device_index>` including `/doc/<device_index>/sleep_duration_sec`, and uses `/doc/sleep_time` as the desired target for downlink sleep updates.
