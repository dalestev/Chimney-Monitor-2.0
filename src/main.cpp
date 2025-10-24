/*
 * main.cpp
 * Main project file for ESP32 ThingsBoard Client
 * (Refactored for Simple Deep Sleep with "Pull" OTA on wake)
 */

#include <Arduino.h>
#include "config.h"
#include "ConnectionManager.h"
#include <ArduinoJson.h>
#include "BatteryManager.h"
#include "ShtManager.h"
#include "ChimneyProbe.h"

// --- Global Objects ---
ConnectionManager conn(TB_MQTT_HOST, TB_MQTT_PORT, TB_HTTP_HOST);
BatteryManager batt;
ShtManager sht;
ChimneyProbe chimney;

// --- Forward Declarations ---
void initSensors();
bool connectAll();
void sendTelemetryData();
void goToSleep();
void onMqttMessage(char* topic, byte* payload, unsigned int length);


// --- Setup ---
// Runs ONCE on every wake-up (from deep sleep or power-on)
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("\n--- WAKING UP ---");

  initSensors();

  // Connect to Wi-Fi & ThingsBoard.
  // This function also pauses to listen for the automatic attribute update.
  if (connectAll()) {
    
    // If the code reaches this point, it means connectAll() finished
    // AND onMqttMessage() did NOT trigger an OTA update.
    
    Serial.println("Firmware check complete, no new version. Sending telemetry...");

    // Send this boot's attributes (fw_state, etc.)
    JsonDocument doc;
    doc["fw_title"] = FIRMWARE_TITLE;
    doc["fw_version"] = FIRMWARE_VERSION;
    doc["fw_state"] = "IDLE";
    char attr_payload[150];
    serializeJson(doc, attr_payload);
    conn.sendAttributes(attr_payload);
    conn.loop(); // Give a moment to send
    delay(100);

    // Send sensor data
    sendTelemetryData();
    Serial.println("Telemetry sent.");
    conn.loop(); // Give MQTT time to send
    delay(200);

  } else {
    Serial.println("Failed to connect. Sleeping anyway.");
    // (This is where you would add data to a "failed send" queue)
  }

  // Go back to sleep
  goToSleep();
}


// --- Main Loop ---
// This will never be reached.
void loop() {
}


// --- Helper Functions ---

void initSensors() {
  Wire.begin();

  if (batt.begin(&Wire, LC709203F_APA_500MAH)) {
    Serial.println("Battery monitor initialized.");
  } else {
    Serial.println("WARNING: Battery monitor not found!");
  }

  if (sht.begin()) {
    Serial.println("SHT30 sensor initialized.");
    Serial.println(sht.getTemperature());
  } else {
    Serial.println("WARNING: SHT30 sensor not found!");
  }

  Serial.println("Chimney Probe Initialized...Delaying First Read");
  delay(500); // Wait for MAX6675 to stabilize
}

bool connectAll() {
  conn.connectWifi(WIFI_SSID, WIFI_PASS);
  
  // Set the callback *before* connecting
  conn.setCallback(onMqttMessage); 
  
  conn.connectThingsboard(TB_DEVICE_TOKEN); 

  // Wait for connection
  unsigned long connect_start = millis();
  while (!conn.isConnected() && (millis() - connect_start < 15000)) {
    // Wait up to 15 seconds to connect
    conn.loop();
    delay(100);
  }

  if (conn.isConnected()) {
    Serial.println("Wi-Fi and ThingsBoard connected.");
    
    // CRITICAL: We must now listen for the attribute update
    // that ThingsBoard sends on connect.
    Serial.println("Listening for firmware version...");
    conn.loop(); // Process incoming messages
    delay(1000); // Wait 1 second for the message to arrive
    conn.loop(); // Process it (this is when onMqttMessage will run)
    
    return true;
  } else {
    Serial.println("Failed to connect to ThingsBoard!");
    return false;
  }
}

void sendTelemetryData() {
  // Create a JSON payload for telemetry
  JsonDocument telemetry_doc;
  
  // Battery Data
  telemetry_doc["batt_voltage"] = batt.getVoltage();
  telemetry_doc["batt_percent"] = batt.getPercentage();

  // SHT30 "External" Data
  telemetry_doc["ext_temp"] = sht.getTemperature();
  telemetry_doc["ext_hum"] = sht.getHumidity();

  // Connection Data
  telemetry_doc["rssi"] = conn.getRSSI();

  // Chimney Probe
  float chimneyTemp = chimney.getTemperature();
  if (!isnan(chimneyTemp)) {
    telemetry_doc["chimney_temp"] = chimneyTemp;
  }

  char json_payload[300];
  serializeJson(telemetry_doc, json_payload);
  
  conn.sendTelemetryJson(json_payload);
}

void goToSleep() {
  Serial.printf("--- GOING TO SLEEP for %llu seconds ---\n\n", SLEEP_DURATION_S);
  Serial.flush(); // Ensure all serial messages are sent

  // Configure the timer wake-up source
  // (You need to define SLEEP_DURATION_S in config.h)
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_US); 

  // Enter deep sleep
  esp_deep_sleep_start();
}


// --- MQTT Message Callback ---
// (This is unchanged from your original code)
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  Serial.println("---");
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.print("Message: ");
  Serial.println(message);

  if (strcmp(topic, "v1/devices/me/attributes") == 0) {
    JsonDocument doc;
    deserializeJson(doc, message);

    if (doc["fw_version"].is<const char*>()) {
      const char* fw_title = doc["fw_title"];
      const char* fw_version = doc["fw_version"];
      
      Serial.printf("Received new firmware info. Title: %s, Version: %s\n", fw_title, fw_version);

      if (strcmp(fw_version, FIRMWARE_VERSION) != 0) {
        Serial.println("New firmware detected! Starting OTA update process...");
        // Note: performOtaUpdate will restart the device.
        conn.performOtaUpdate(fw_title, fw_version);
      } else {
        Serial.println("Firmware is already up to date.");
      }
    }
  }
}