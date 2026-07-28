# Actionneur

## Description

This folder contains the code for the esp32 that controls relays from ALIVEcode.

## How to use

### Seting up the secrets

You must setup the `SECRET.h` file witht the `SSID` and `PASSWORD` of the wifi network you want to connect to.
Also you need to set the `AUTH_TOKEN` and `OBJECT_ID` from the device page on ALIVEcode.
The `SECRET.h` file should look like this:

```c++
/**
 * Fill in the credentials below
 * For the Wifi and the Aliot credentials
*/

// Setup wifi credentials
#define SSID "SSID"
#define PASSWORD "PASSWORD"

// Setup aliot credentials
#define AUTH_TOKEN "AUTH_TOKEN"
#define OBJECT_ID "OBJECT_ID"
```

### Setting up the relays

You need to setup the relays in the `main.cpp` file.

#### Defining the relays

The relays are defined in the `valves` array. like this:

```c++
const Valve valves[] = // Changed to an array
    {
        {
            .actionId = "valve_1",
            .pin = 2,
            .isPinInverted = true
        },
        {
            .actionId = "valve_2",
            .pin = 4,
            .isPinInverted = true
        }
    };
```

_Note : You can not have more relay than the value of `MAX_ACTION_COUNT` in `Aliot-C` (the default value is 5)_

- The `actionId` field is the id of the action _and_ the field value on ALIVEcode.
- The `pin` field is the pin number on the esp32.
- The `isPinInverted` field is used to invert the pin state. (If the relay is active when the pin is low, set this to `true`)

#### Adding the callbacks

For each relay you need to add the following bit of code:
`main.cpp` callback function:

```c++
// Function to be called when valve 1 state is changed on ALIVEcode
bool callbackValve1(const char* data) {

    Serial.print("New valve 1 state:");
    Serial.println(data);

    // Set valve 1 state
    setValveState(valves[0], stringToBool(data));

    return true;
}

void setup() {
    // ...

    // Add the callback to the valve 1
    addCallback(valves[0].actionId, callbackValve1);

    // ...
}
```

### Adding controls on AliveCode

1. Create a toggle component
2. Set the toggle to be controlled by the document with `actionID`
   ![controlled image](doc/controlled.png)
3. Set the Id of the action to the `actionID`
   ![ActionID image](doc/actionID.png)
4. Repeat 1-3 for each relay in the list

### Updating the wifi credentials remotely

The device exposes a `wifi_config` action that lets you push new wifi credentials from the ALIVEcode dashboard without re-flashing the device.

1. Create an action component with `actionID` set to `wifi_config`.
2. Configure the action's payload value to be a JSON string of the form:

   ```json
   { "ssid": "MyNetwork", "password": "MyPassword" }
   ```

3. When received, the device saves the new credentials to non-volatile storage (NVS) and switches to the new network. The `SSID`/`PASSWORD` values in `SECRET.h` are only used as a fallback on first boot, before any `wifi_config` action has ever been received; afterwards, the stored credentials always take precedence.

> **Warning**: if the new credentials are invalid, the device will fail to connect, restart, and retry with the same (bad) saved credentials on every subsequent boot — there is currently no automatic fallback to the last-known-good network. Double check the SSID/password before sending this action.

_Note: credential persistence relies on the ESP32 `Preferences` (NVS) API. On `esp8266`/`native` builds the `wifi_config` action is still registered and will switch networks for the current session, but nothing is persisted — the hardcoded `SECRET.h` credentials are used again on the next boot._

### Installing the libraries

The project uses platformio to manage the libraries. You can install it from [here](https://platformio.org/install/ide?install=vscode).
After installing platformio, you need to install the libraries. You can do this by opening the project in vscode and clicking on the `PlatformIO: Build` button in the bottom left corner. (The library installation should start automatically)

#### Libraries used

- [ArduinoJson](https://arduinojson.org/)
- [Aliot-C](https://github.com/LRIMa-Qc/aliot-c)
- [WiFiClientSecure](https://www.arduino.cc/en/Reference/WiFiClientSecure)
- [ArduinoHttpClient](https://www.arduino.cc/reference/arduinohttpclient/)

### Uploading

#### Connection

First plug the esp32 to your computer using a FTDI Mini Usb to Serial adapter [https://www.amazon.ca/dp/B0CBBKXVQV](https://www.amazon.ca/dp/B0CBBKXVQV)

![Branchements #1](doc/branchement1.jpg)
![Branchements #2](doc/branchement2.jpg)

See the table below for the connections:

| Relais | FTDI |
| ------ | ---- |
| RX     | TX   |
| TX     | RX   |
| GND    | GND  |

#### Uploading the code

You can upload the code to the esp32 by clicking on the `PlatformIO: Upload` button ( ➡️ ) in the bottom left corner of vscode. (You need to have the esp32 connected to your computer)

#### Relay board specific

1. You need to set the board in `Boot` mode to upload the code.
   > (You can do this by shorting the `BOOT` header before connecting the esp32 to power)
2. After the board is powered, you can unshort the `BOOT` header.
3. You need to make sure that all the `DIP` switches are set to the relay side.
   > (If you don't do it, the board won't flash properly. This is due to the fact that `PIN 12` must be set to `low`)

### Monitoring the serial output

You can monitor the serial output by clicking on the `PlatformIO: Serial Monitor` button ( 🔌 ) in the bottom left corner of vscode. (You need to have the esp32 connected to your computer)

### Connecting components (ex: Fans, Lights, etc.)

![Connection components](doc/schema_actionneur.png)

Connect into the COM (middle) the voltage source (anything between 0V to 220V) and the NO (Normally Open) the component you want to control. You can connect the NC (Normally Closed) to the component if you want to invert the relay state.

In simpler terms:

1. When the relay is activated, the power from the COM will be sent to the NO.
2. When the relay is deactivated, the power from the COM will be sent to the NC.
