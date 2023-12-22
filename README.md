# iot_obj-sicro-sensor

## Repo composition
```bash
code/
├── broadcaster  (sensor code [nrf52833])
├── central      (python serial to ALIVEcode)
└── central-nrf  (BLE reciver [nrf52840dongle])
schematics/
├── SICRO-Main-easyEDA.zip (Main Schematic)
└── SICRO-Ground-easyEDA.zip (Ground part Schematic)
```
### Physical device
The schematics and PCB were made using [EasyEDA Pro](https://pro.easyeda.com/) site
There is a protective layer that needs to be applied on the ground part of the pcb to prevent corrosion

## Building

#### Tools needed
- VS Code
- nRF Connect
- nRF toolkit `v3.5`
- J-Link

#### Broadcaster
Before building the broadcaster program, you **must** make sure that you changed the `CONFIG_BLE_USER_DEFINED_MAC_ADDR` and the `CONFIG_BT_DEVICE_NAME` so that they are unique. Note that the `CONFIG_BT_DEVICE_NAME` must end with a number. If you need mac address, you can use this [mac address generator](https://dnschecker.org/mac-address-generator.php).

To build the program for the nrf52833 you need to use the [NRFconnect](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code) extension in vscode and then you just have to [follow the instructions](https://nrfconnect.github.io/vscode-nrf-connect/get_started/build_app_ncs.html#how-to-build-an-application) to build it using the `lrima_greenhouse_nrf52833` board. After you need to flash it using the [option available in the extension](https://nrfconnect.github.io/vscode-nrf-connect/get_started/quick_debug.html#how-to-flash-an-application).

#### J-Link setup
![J-Link setup](doc/img/J-Link-Pin.png)

### Central-nrf
To build the program for the nrf52840dongle you need to use the [NRFconnect](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code) extension in vscode and then you just have to follow the [instrution](https://nrfconnect.github.io/vscode-nrf-connect/get_started/build_app_ncs.html#how-to-build-an-application) to build it using the `lrima_greenhouse_nrf52833` board. After you need to flash it using the nrf programer tools available in the nrf Connect for desktop programs. You will need to put the board in [DFU mode](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fug_nrf52840_dongle%2FUG%2Fnrf52840_Dongle%2Fprogramming.html). The file need for the flashing will be under the following path `build\zephyr\zephyr.hex`.

### Running

To run the sensor you will need first to plug the Central-nrf in a computer or raspberry pi and then you will need to setup the config.ini file from the `central` folder. In the config.ini you will have to specify the `object id` and the `auth token` that can be found in your IoT project on [ALIVEcode.ca](ALIVEcode.ca)

Afterward you have to install the dependency using the `pip install -r requirements.txt` command. (Using a venv is suggested.)
When it's done, you can run the project using the command. `python main.py`
When the python program is running, you can now power on the broadcaster.
