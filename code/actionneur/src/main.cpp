//=============================================================================================================================
//=============================================================================================================================
//! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !
//! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !

// WARNING: If you plan to use this code with more than 1 valve, you need to
// have a dedicated 5V power supply for the valves

//! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !
//! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !
//=============================================================================================================================
//=============================================================================================================================

#include "SECRET.h"
#include <AliotObject.h>
#include <ArduinoJson.h>
#include <cstring>

#ifdef ESP32
#include <Preferences.h>
#endif

long lastMillis = 0;
long delayMillis = 500; // 0.5 second

bool isSetupDone = false;

// Serial monitor speed
const int BAUD_RATE = 115200;

// Path for the document
const char *docPath = "/doc/";

// Type def for the valves
typedef struct {
  const char *actionId;
  const uint8_t pin;
  const bool isPinInverted;
} Valve;

// Array of valves
const Valve valves[] = // Changed to an array
    {{.actionId = "valve_1", .pin = 25, .isPinInverted = false},
     {.actionId = "valve_2", .pin = 26, .isPinInverted = false},
     {.actionId = "valve_3", .pin = 27, .isPinInverted = false},
     {.actionId = "valve_4", .pin = 14, .isPinInverted = false},
     {.actionId = "valve_5", .pin = 18, .isPinInverted = false}};

// Crate AliotObject instance
AliotObject aliotObj = AliotObject();

// NVS storage for remotely-updated wifi credentials
#ifdef ESP32
Preferences preferences;
#endif
const char *WIFI_PREF_NAMESPACE = "wifi_cfg";
const char *WIFI_PREF_KEY_SSID = "ssid";
const char *WIFI_PREF_KEY_PASS = "password";

// Currently active wifi credentials (loaded from NVS, falling back to
// SECRET.h on first boot)
String currentSsid;
String currentPassword;

// Set when a wifi_config action has updated currentSsid/currentPassword but
// they haven't been confirmed working (i.e. persisted) yet. Persisting only
// after a confirmed reconnect keeps bad credentials from ever overwriting a
// known-good stored network on NVS.
bool pendingWifiSave = false;

// Convert string to a bool
bool stringToBool(const char *str) { return std::strcmp(str, "true") == 0; }

void setValveState(const Valve &valve, bool state) {
  if (valve.isPinInverted) {
    digitalWrite(valve.pin, !state);
    aliotObj.updateDoc(createDict<bool>(
        Pair<bool>((std::string(docPath) + valve.actionId).c_str(), !state)));
  } else {
    digitalWrite(valve.pin, state);
    aliotObj.updateDoc(createDict<bool>(
        Pair<bool>((std::string(docPath) + valve.actionId).c_str(), state)));
  }
}

// Function to be called when valve 1 state is changed on ALIVEcode
bool callbackValve1(const char *data) {

  Serial.print("New valve 1 state:");
  Serial.println(data);

  // Set valve 1 state
  setValveState(valves[0], stringToBool(data));

  return true;
}

// Function to be called when valve 2 state is changed on ALIVEcode
bool callbackValve2(const char *data) {
  Serial.print("New valve 2 state:");
  Serial.println(data);

  // Set valve 2 state
  setValveState(valves[1], stringToBool(data));

  return true;
}

// Function to be called when valve 3 state is changed on ALIVEcode
bool callbackValve3(const char *data) {
  Serial.print("New valve 3 state:");
  Serial.println(data);

  // Set valve 3 state
  setValveState(valves[2], stringToBool(data));

  return true;
}

// Function to be called when valve 4 state is changed on ALIVEcode
bool callbackValve4(const char *data) {
  Serial.print("New valve 4 state:");
  Serial.println(data);

  // Set valve 4 state
  setValveState(valves[3], stringToBool(data));

  return true;
}

// Function to be called when valve 5 state is changed on ALIVEcode
bool callbackValve5(const char *data) {
  Serial.print("New valve 5 state:");
  Serial.println(data);

  // Set valve 5 state
  setValveState(valves[4], stringToBool(data));

  return true;
}

// Load wifi credentials from NVS, falling back to the hardcoded SECRET.h
void loadWifiCredentials() {
#ifdef ESP32
  preferences.begin(WIFI_PREF_NAMESPACE, true);
  currentSsid = preferences.getString(WIFI_PREF_KEY_SSID, SSID);
  currentPassword = preferences.getString(WIFI_PREF_KEY_PASS, PASSWORD);
  preferences.end();
#else
  currentSsid = SSID;
  currentPassword = PASSWORD;
#endif
  Serial.print("Using wifi SSID: ");
  Serial.println(currentSsid);
}

// Persist new wifi credentials to NVS so they survive reboot. No-op (aside
// from logging) on platforms without Preferences support.
void saveWifiCredentials(const String &ssid, const String &password) {
#ifdef ESP32
  preferences.begin(WIFI_PREF_NAMESPACE, false);
  preferences.putString(WIFI_PREF_KEY_SSID, ssid);
  preferences.putString(WIFI_PREF_KEY_PASS, password);
  preferences.end();
  Serial.println("Wifi credentials saved to NVS");
#else
  Serial.println("Preferences not available on this platform; wifi credentials "
                 "not persisted");
#endif
}

// Function to be called when new wifi credentials are pushed from ALIVEcode.
// Expects `data` to be a JSON string: {"ssid": "...", "password": "..."}
bool callbackWifiConfig(const char *data) {
  Serial.print("New wifi config received: ");
  Serial.println(data);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, data);
  if (err) {
    Serial.print("Failed to parse wifi_config payload: ");
    Serial.println(err.c_str());
    return false;
  }
  if (!doc.containsKey("ssid") || !doc.containsKey("password")) {
    Serial.println("wifi_config payload missing 'ssid' or 'password' key");
    return false;
  }

  currentSsid = doc["ssid"].as<String>();
  currentPassword = doc["password"].as<String>();

  // Don't persist yet: if these credentials are bad, setupWiFi() will time
  // out and ESP.restart(), and we want the next boot to fall back to
  // whatever is still saved (or SECRET.h), not retry this bad value forever.
  // onReconnect() persists it once the new network is confirmed working.
  pendingWifiSave = true;

  aliotObj.setupConfig(AUTH_TOKEN, OBJECT_ID, currentSsid.c_str(),
                       currentPassword.c_str());
  // Force a reconnect; aliotObj.loop()'s existing self-healing path picks up
  // the new config on its next iteration
  WiFi.disconnect();

  Serial.println("Switching to new wifi network...");
  return true;
}

// Update all valves from ALIVEcode
void updateValveFromServer() {
  for (int i = 0; i < sizeof(valves) / sizeof(valves[0]);
       i++) { // Updated loop condition
    // Get valve state from ALIVEcode
    String res = aliotObj.getDoc(String(docPath) + String(valves[i].actionId));
    bool state = stringToBool(res.c_str());

    // Set valve state
    setValveState(valves[i], state);

    Serial.println(valves[i].actionId + String(" state updated (state: ") +
                   res + ")");

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

  // Only now that the current credentials have proven to work do we persist
  // them, so a bad wifi_config update never overwrites a known-good NVS
  // entry.
  if (pendingWifiSave) {
    saveWifiCredentials(currentSsid, currentPassword);
    pendingWifiSave = false;
  }

  updateValveFromServer();
}

void setup() {
  Serial.begin(BAUD_RATE);

  Serial.println("Starting...");

  loadWifiCredentials();
  aliotObj.setupConfig(AUTH_TOKEN, OBJECT_ID, currentSsid.c_str(),
                       currentPassword.c_str());

  aliotObj.setReconnectCallback(onReconnect);

  // Start connection process and listen for events
  aliotObj.run();

  lastMillis = millis(); // Set lastMillis to current millis

  // Add callback for valve 1
  aliotObj.onActionRecv(valves[0].actionId,
                        callbackValve1); // Corrected actionID to actionId

  // Add callback for valve 2
  aliotObj.onActionRecv(valves[1].actionId,
                        callbackValve2); // Corrected actionID to actionId

  // Add callback for valve 3
  aliotObj.onActionRecv(valves[2].actionId,
                        callbackValve3); // Corrected actionID to actionId

  // Add callback for valve 4
  aliotObj.onActionRecv(valves[3].actionId,
                        callbackValve4); // Corrected actionID to actionId

  // Add callback for valve 5
  aliotObj.onActionRecv(valves[4].actionId,
                        callbackValve5); // Corrected actionID to actionId

  // Add callback for remote wifi credential updates
  aliotObj.onActionRecv("wifi_config", callbackWifiConfig);

  // Setup valve pin
  for (int i = 0; i < sizeof(valves) / sizeof(valves[0]); i++) {
    pinMode(valves[i].pin, OUTPUT);
  }

  Serial.println("Setup done");
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
