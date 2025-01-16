# Broadcaster

## Flashing the board

### Tools needed
- VS Code
- [nRF Connect](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop)
- [nRF Command line tool](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download)
- [nRF Connect VS Code extension](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code)
- nRF toolchian `v2.3.0`
- nRF SDK `v.2.3.0`
- J-Link

### Preparing the environment
Before building the broadcaster program, you **must** make sure that you changed the `CONFIG_BLE_USER_DEFINED_MAC_ADDR` and the `CONFIG_BLE_USER_DEFINED_NAME` so that they are unique. Note that the `CONFIG_BLE_USER_DEFINED_NAME` must end with a number (ex : 01). If you need mac address, you can use this [mac address generator](https://dnschecker.org/mac-address-generator.php).

### Building and flashing
To build the program for the nrf52833 you need to use the [NRFconnect](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-VS-Code) extension in vscode and then you just have to [follow the instructions](https://nrfconnect.github.io/vscode-nrf-connect/get_started/build_app_ncs.html#how-to-build-an-application) to build it using the `lrima_greenhouse_nrf52833` board. After you need to flash it using the [option available in the extension](https://nrfconnect.github.io/vscode-nrf-connect/get_started/quick_debug.html#how-to-flash-an-application). Note : If the board is giving out an error when flashing try these steps :
1. Check that the J-Link is detected by the computer (Drivers might be needed)
2. Check that the board has power (Use the led test button)
3. Check that the J-Link is properly connected to the board (You can try to press the reset button on the board, the J-Link led should be red when the button is pressed)
4. Try to flash the board using the `erase and flash` option
5. Verify the soldering of the board

#### J-Link setup
![J-Link setup](/doc/img/J-Link-Pin.png)

### Firmware over the air update (DFU)
The board comes with the option to send over the air updates (ota). To prepare an update you just need to build the program like in the previous section. You need after to send the `app_update.bin` file that is located in the `build\zephyr` folder to your phone. You then need to keep press the button1 (see picture) until the led is on and then release it. You will now have 60 seconds, or more if you change the value of `CONFIG_BLE_DFU_ADV_DURATION_SEC` to start the flashing process. To do so, you will need to use the [nRF Connect mobile app](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-mobile) and connect to the device with the name specified in `CONFIG_BLE_USER_DEFINED_NAME`. You will then need to go in the `DFU` tab and  select the `app_update.bin` file. Finally, press the `Upload` button. The flashing process will start and you will be able to see the progress in the app. When the flashing is done, the board will reboot and the new program will start. 
Notes : If the led is turing off before you can start the flashing process, you can keep the button pressed and the sensor will keep advertising util you release the button.

![Button1](/doc/img/Button1.png)

## Device Functionality
The device is split in two parts, the broadcaster and the connectable. The broadcaster is the part that will send the data to the central. The connectable is the part that will be used to update the firmware over the air and to change the sleep time of the device. The two parts are combined in one Bluetooth device.

The user can wake up the device at any time by pressing the button1. 

### Broadcaster
The broadcaster will send the data at the interval specified in `CONFIG_SENSOR_SLEEP_DURATION_SEC` or at the interval set by the user using the connectable part. The broadcaster will send a beacon using the BLE extended advertising protocol (`BLE 5.0`) for the duration specified in `CONFIG_BLE_ADV_DURATION_SEC`. The beacon will contain the following information:
![Broadcaster data.](/doc/img/broadcaster-data.png)

### Connectable
The connectable part will only be active if `CONFIG_SENSOR_SLEEP_MODIFICATION_ENABLED=y` and will be visible for the same time as the broadcaster. However, if you are connected to the device, it will stay active until you disconnect or the inactivity timer is reached (`CONFIG_BLE_CONN_TIMEOUT_SEC`). The connectable part will advertise using `GATT` a service with the following UUID `0xAFBE` and with the following characteristics:
- `0xAFBF` : This characteristic will allow you to `read/write` the `sleep time` of the device. The value is a 16 bit unsigned integer that represents the number of seconds that the device will sleep. The value is stored in little endian format. The value is stored in the flash memory of the device and will be loaded at boot. The value will be reset to the default value if the device is reset.
The DFU mode won't be visible if the user hasn't pressed the button1 before the device start advertising for the duration specified in `CONFIG_BLE_DFU_ADV_DURATION_SEC`.
