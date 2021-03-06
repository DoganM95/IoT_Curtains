// Configurations
#include "./Configuration/Blynk.h"
#include "./Configuration/Wifi.h"

// Libraries
#include <BlynkSimpleEsp32.h>  // Part of Blynk by Volodymyr Shymanskyy
#include <WiFi.h>              // Part of WiFi Built-In by Arduino
#include <WiFiClient.h>        // Part of WiFi Built-In by Arduino
#include <math.h>

// Configurations
const unsigned int pressDuration = 500;  // time the gpio should be HIGH, before it goes back to LOW

// GPIO pins
const unsigned short int openCurtainsPin = 26;   // Put a pulldown resistor on this one (10 kOhm from gpio -> GND)
const unsigned short int closeCurtainsPin = 25;  // Put a pulldown resistor on this one (10 kOhm from gpio -> GND)

// Connection State
String IpAddress = "";
String MacAddress = "";

// Limits
const int wifiHandlerThreadStackSize = 10000;
const int blynkHandlerThreadStackSize = 10000;

// Counters
unsigned long long wifiReconnectCounter = 0;
unsigned long long blynkReconnectCounter = 0;

// Timeouts
int wifiConnectionTimeout = 10000;
int blynkConnectionTimeout = 10000;
int blynkConnectionStabilizerTimeout = 5000;
ushort cycleDelayInMilliSeconds = 100;

// Task Handles
TaskHandle_t wifiConnectionHandlerThreadFunctionHandle;
TaskHandle_t blynkConnectionHandlerThreadFunctionHandle;

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  pinMode(openCurtainsPin, OUTPUT);
  pinMode(closeCurtainsPin, OUTPUT);

  xTaskCreatePinnedToCore(wifiConnectionHandlerThreadFunction, "Wifi Connection Handling Thread", wifiHandlerThreadStackSize, NULL, 20, &wifiConnectionHandlerThreadFunctionHandle, 1);
  xTaskCreatePinnedToCore(blynkConnectionHandlerThreadFunction, "Blynk Connection Handling Thread", blynkHandlerThreadStackSize, NULL, 20, &blynkConnectionHandlerThreadFunctionHandle, 1);
}

// ----------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------

void loop() {
  UpdateIpAddressInBlynk();
  UpdateMacAddressInBlynk();
  Blynk.run();
}

// ----------------------------------------------------------------------------
// FUNCTIONS
// ----------------------------------------------------------------------------

// Blynk Functions

BLYNK_CONNECTED() {  // Restore hardware pins according to current UI config
  Blynk.syncAll();
}

BLYNK_WRITE(V1) {  // Open curtains
  int pinValue = param.asInt();
  digitalWrite(openCurtainsPin, 1);
  delay(pressDuration);
  digitalWrite(openCurtainsPin, 0);
}

BLYNK_WRITE(V2) {  // Close curtains
  int pinValue = param.asInt();
  digitalWrite(closeCurtainsPin, 1);
  delay(pressDuration);
  digitalWrite(closeCurtainsPin, 0);
}

// General functions

void WaitForWifi(uint cycleDelayInMilliSeconds) {
  while (WiFi.status() != WL_CONNECTED) {
    delay(cycleDelayInMilliSeconds);
  }
}

void WaitForBlynk(int cycleDelayInMilliSeconds) {
  while (!Blynk.connected()) {
    delay(cycleDelayInMilliSeconds);
  }
}

void wifiConnectionHandlerThreadFunction(void* params) {
  uint time;
  while (true) {
    if (!WiFi.isConnected()) {
      try {
        Serial.printf("Connecting to Wifi: %s\n", WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PW);  // initial begin as workaround to some espressif library bug
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PW);
        WiFi.setHostname("Desklight (ESP32, Blynk)");
        time = 0;
        while (WiFi.status() != WL_CONNECTED) {
          if (time >= wifiConnectionTimeout || WiFi.isConnected()) break;
          delay(cycleDelayInMilliSeconds);
          time += cycleDelayInMilliSeconds;
        }
      } catch (const std::exception e) {
        Serial.printf("Error occured: %s\n", e.what());
      }
      if (WiFi.isConnected()) {
        Serial.printf("Connected to Wifi: %s\n", WIFI_SSID);
        wifiReconnectCounter = 0;
      }
    }
    delay(1000);
    Serial.printf("Wifi Connection Handler Thread current stack size: %d , current Time: %d\n", wifiHandlerThreadStackSize - uxTaskGetStackHighWaterMark(NULL), xTaskGetTickCount());
  };
}

void blynkConnectionHandlerThreadFunction(void* params) {
  uint time;
  while (true) {
    if (!Blynk.connected()) {
      Serial.printf("Connecting to Blynk: %s\n", BLYNK_USE_LOCAL_SERVER == true ? BLYNK_SERVER : "Blynk Cloud Server");
      if (BLYNK_USE_LOCAL_SERVER)
        Blynk.config(BLYNK_AUTH, BLYNK_SERVER, BLYNK_PORT);
      else
        Blynk.config(BLYNK_AUTH);
      Blynk.connect();  // Connects using the chosen Blynk.config
      uint time = 0;
      while (!Blynk.connected()) {
        if (time >= blynkConnectionTimeout || Blynk.connected()) break;
        delay(cycleDelayInMilliSeconds);
        time += cycleDelayInMilliSeconds;
      }
      if (Blynk.connected()) {
        Serial.printf("Connected to Blynk: %s\n", BLYNK_USE_LOCAL_SERVER ? BLYNK_SERVER : "Blynk Cloud Server");
        delay(blynkConnectionStabilizerTimeout);
      }
    }
    delay(1000);
    Serial.printf("Blynk Connection Handler Thread current stack size: %d , current Time: %d\n", blynkHandlerThreadStackSize - uxTaskGetStackHighWaterMark(NULL), xTaskGetTickCount());
  }
}

void UpdateIpAddressInBlynk() {
  if (IpAddress != WiFi.localIP().toString()) {
    IpAddress = WiFi.localIP().toString();
    Blynk.virtualWrite(V10, IpAddress);
  }
}

void UpdateMacAddressInBlynk() {
  if (MacAddress != WiFi.macAddress()) {
    MacAddress = WiFi.macAddress();
    Blynk.virtualWrite(V11, MacAddress);
  }
}