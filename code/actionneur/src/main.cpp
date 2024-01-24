//=============================================================================================================================
//=============================================================================================================================
//! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !

// WARNING: If you plan to use this code with more than 1 valve, you need to have a dedicated 5V power supply for the valves

//! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !
//=============================================================================================================================
//=============================================================================================================================


#include <AliotObject.h>
#include <cstring>
#include "SECRET.h"

long lastMillis = 0;
long delayMillis = 500; // 0.5 second

bool isSetupDone = false;

// Path for the document
const char* docPath = "/doc/";

// Type def for the valves
typedef struct {
    const char* actionId;
    const uint8_t pin;
    const bool isPinInverted;
} Valve;

// Array of valves
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

// Crate AliotObject instance 
AliotObject aliotObj = AliotObject();

// Convert string to a bool
bool stringToBool(const char* str) {
    return std::strcmp(str, "true") == 0;
}

void setValveState(const Valve& valve, bool state) {  // Pass by reference
    if (valve.isPinInverted) digitalWrite(valve.pin, !state);
    else digitalWrite(valve.pin, state);
}

// Function to be called when valve 1 state is changed on ALIVEcode
bool callbackValve1(const char* data) {
    
    Serial.print("New valve 1 state:");
    Serial.println(data);

    // Set valve 1 state
    setValveState(valves[0], stringToBool(data));

    return true;
}

// Function to be called when valve 2 state is changed on ALIVEcode
bool callbackValve2(const char* data) {
    Serial.print("New valve 2 state:");
    Serial.println(data);

    // Set valve 2 state
    setValveState(valves[1], stringToBool(data));

    return true;
}

// Update all valves from ALIVEcode
void updateValveFromServer() {
    for (int i = 0; i < sizeof(valves) / sizeof(valves[0]); i++) {  // Updated loop condition
        // Get valve state from ALIVEcode
        String res = aliotObj.getDoc(String(docPath) + String(valves[i].actionId));
        bool state = stringToBool(res.c_str());

        // Set valve state
        setValveState(valves[i], state);

        Serial.println(valves[i].actionId + String(" state updated (state: ") + res + ")");

        // sleep for 0.5 sec
        lastMillis = millis();
        while (millis() - lastMillis < delayMillis) {
            aliotObj.loop();
        }
    }
}

// Called when the wifi is connected again
void onReconnect() {
    Serial.println("Reconnected to ALIVEcode");
    updateValveFromServer();
}

void setup() {
    Serial.begin(115200);

    aliotObj.setupConfig(AUTH_TOKEN, OBJECT_ID, SSID, PASSWORD);

    aliotObj.setReconnectCallback(onReconnect);

    // Start connection process and listen for events
    aliotObj.run();

    lastMillis = millis(); // Set lastMillis to current millis

    // Add callback for valve 1
    aliotObj.onActionRecv(valves[0].actionId, callbackValve1);  // Corrected actionID to actionId

    // Add callback for valve 2
    aliotObj.onActionRecv(valves[1].actionId, callbackValve2);  // Corrected actionID to actionId

    // Setup valve pin
    for (int i = 0; i < sizeof(valves) / sizeof(valves[0]); i++) {
        pinMode(valves[i].pin, OUTPUT);
    }
}

void loop() {
    aliotObj.loop();

    // Wait for delayMillis to make sure the connection is established
    if (millis() - lastMillis > delayMillis && !isSetupDone) {
        // Get valve state from ALIVEcode
        updateValveFromServer();

        isSetupDone = true;
    }

    // Nothing to do here
}
