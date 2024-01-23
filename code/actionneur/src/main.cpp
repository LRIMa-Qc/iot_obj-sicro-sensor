#include <AliotObject.h>
#include <cstring>
#include "SECRET.h"

// Path for the document
const char* docPath = "/doc/";

// Entity ID for valve 1
const char* IDvalve1 = "valve_1";
const uint8_t pinValve1 = 2;

// Crate AliotObject instance 
AliotObject aliotObj = AliotObject();

// Conveet string to a bool
bool stringToBool(const char* str) {
    return std::strcmp(str, "true") == 0;
}

bool calbackValve1(const char* data) {
    Serial.print("New valve 1 state:");
    Serial.println(data);

    digitalWrite(pinValve1, stringToBool(data));

    return true;
}

void setupValve1() {
    Serial.println("Setting up valve 1...");

    // Add callback for valve 1
    aliotObj.onActionRecv(IDvalve1, calbackValve1);

    // Get valve 1 state
    String res = aliotObj.getDoc(String(docPath) + String(IDvalve1));
    bool state = stringToBool(res.c_str());

    // Setup valve 1 pin
    pinMode(pinValve1, OUTPUT);
    
    // Set valve 1 state
    digitalWrite(pinValve1, state);

    Serial.println("Valve 1 setup done (state: " + res + ")");
}

void setup() {
    Serial.begin(115200);

    // Start connection process and listen for events
    aliotObj.run();

    // Setup valve 1
    setupValve1();
}

void loop() {
    aliotObj.loop();

    // Noting to do here
}