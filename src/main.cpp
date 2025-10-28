/*
 * main.cpp
 * Main project file for ESP32 ThingsBoard Client
 * (Refactored for Simple Deep Sleep with "Pull" OTA on wake)
 */

#include <Arduino.h>
#include "config.h" // <-- FIX: Changed back to .h
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

// --- Global State ---
bool attributes_received = false; // Flag to track if we've processed attributes

// --- Forward Declarations ---
// (Declaring functions so setup() can use them)
void initSensors();
bool connectAll();
void sendTelemetryData();
void goToSleep();
void onMqttMessage(char* topic, byte* payload, unsigned int length);


// --- Setup ---
// Runs ONCE on every wake-up (from deep sleep or power-on)
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); } // Wait for serial

  Serial.println("\n--- WAKING UP ---");
  
  attributes_received = false; // Reset the flag on every boot

  initSensors();

  // Connect to Wi-Fi & ThingsBoard.
  // This function now waits until attributes are received or it times out.
  if (connectAll()) {
    
    // If the code reaches this point, it means connectAll() finished.
    // The OTA check will have already happened inside onMqttMessage.
    
    // We only send telemetry if an OTA update wasn't triggered.
    Serial.println("Proceeding to send telemetry...");

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
    // (This is where you would add data to a "failed send" queue if you implement one)
  }

  // Go back to sleep
  goToSleep();
}


// --- Main Loop ---
// This will never be reached, as setup() always ends in deep sleep.
void loop() {
}


// --- Helper Functions ---

/**
 * @brief Initializes all I2C sensors.
 */
void initSensors() {
  Wire.begin();

  if (batt.begin(&Wire, LC709203F_APA_500MAH)) {
    Serial.println("Battery monitor initialized.");
  } else {
    Serial.println("WARNING: Battery monitor not found!");
  }

  if (sht.begin()) {
    Serial.println("SHT30 sensor initialized.");
    // --- MODIFIED: Added delay *before* the dummy read ---
    Serial.println("Priming SHT30 sensor (waiting 50ms)...");
    delay(50); // Give sensor time to power up
    sht.getTemperature(); // First read (often fails, this should help)
    sht.getHumidity();    // First read (often fails)
  } else {
    Serial.println("WARNING: SHT30 sensor not found!");
  }

  Serial.println("Chimney Probe Initialized...Delaying First Read");
  delay(500); // Wait for MAX6675 to stabilize
}

/**
 * @brief Connects to Wi-Fi and ThingsBoard, and listens for OTA.
 * @return true if connection was successful, false otherwise.
 */
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
    
    // --- FIX: Wait for subscriptions to be acknowledged (SUBACK) ---
    Serial.println("Waiting for subscriptions to settle...");
    conn.loop(); // Process incoming SUBACKs
    delay(500);  // Give 500ms for SUBACKs to arrive and be processed
    conn.loop(); // Process them

    // Now, explicitly request attributes from the server
    conn.requestAttributes();
    
    // --- Give the MQTT client time to SEND the request ---
    Serial.println("Waiting for attribute request to send...");
    conn.loop(); // Process outgoing messages
    delay(100);  // Wait for packet to leave

    
    // CRITICAL: Wait for the attributes message to be received.
    // onMqttMessage will set attributes_received = true.
    Serial.println("Listening for firmware version (waiting for attributes response)...");
    
    unsigned long listen_start = millis();
    // Wait for the attributes_received flag, or for a 5-second timeout
    while (attributes_received == false && (millis() - listen_start < 5000)) {
      conn.loop(); // This is what triggers onMqttMessage
      delay(10);   // Small delay to prevent busy-looping
    }
    
    if (attributes_received) {
        Serial.println("Firmware check complete (attributes response received).");
    } else {
        Serial.println("Timeout: No attributes response received. Proceeding anyway...");
        // This is not a critical error, it just means we couldn't check for OTA this cycle.
    }
    
    return true;
  } else {
    Serial.println("Failed to connect to ThingsBoard!");
    return false;
  }
}

/**
 * @brief Gathers all sensor data and sends it to ThingsBoard.
 */
void sendTelemetryData() {
  // Create a JSON payload for telemetry
  JsonDocument doc;
  
  // Battery Data
  doc["batt_voltage"] = batt.getVoltage();
  doc["batt_percent"] = batt.getPercentage();

  // SHT30 "External" Data
  float extTemp = sht.getTemperature();
  float extHum = sht.getHumidity();
  
  if (!isnan(extTemp)) {
    doc["ext_temp"] = extTemp;
  }
  if (!isnan(extHum)) {
    doc["ext_hum"] = extHum;
  }

  // Connection Data
  doc["rssi"] = conn.getRSSI();

  // Chimney Probe
  float chimneyTemp = chimney.getTemperature();
  if (!isnan(chimneyTemp)) {
    doc["chimney_temp"] = chimneyTemp;
  }

  char json_payload[300];
  serializeJson(doc, json_payload);
  
  conn.sendTelemetryJson(json_payload);
}

/**
 * @brief Configures the ESP32 for timer-based wake-up and enters deep sleep.
 */
void goToSleep() {
  Serial.printf("--- GOING TO SLEEP for %llu seconds ---\n\n", SLEEP_DURATION_S);
  Serial.flush(); // Ensure all serial messages are sent

  // Configure the timer wake-up source
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_US); 

  // Enter deep sleep
  esp_deep_sleep_start();
}


/**
 * @brief MQTT Message Callback
 * This is triggered when the attributes message arrives on connect.
 */
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  Serial.println("---");
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.print("Message: ");
  Serial.println(message);

  // --- MODIFIED: Check for the attribute RESPONSE topic ---
  // (We use strstr to see if the topic *starts with* the response prefix)
  if (strstr(topic, "v1/devices/me/attributes/response/") != NULL) {
    
    attributes_received = true; // --- SET THE FLAG ---
    
    JsonDocument doc;
    deserializeJson(doc, message);

    // Check for firmware info in the response payload
    // The response is {"shared":{"fw_version":"1.1.5", ...}}
    // So we need to check doc["shared"]["fw_version"]
    if (doc["shared"]["fw_version"].is<const char*>()) {
      const char* fw_title = doc["shared"]["fw_title"];
      const char* fw_version_from_server = doc["shared"]["fw_version"];
      
      // --- START DEBUGGING ---
      Serial.println("--- OTA DEBUG ---");
      Serial.printf("Server version: >%s<\n", fw_version_from_server);
      Serial.printf("Device version: >%s<\n", FIRMWARE_VERSION);
      
      // Print the raw bytes of each string to check for hidden chars
      Serial.print("Server bytes: ");
      for(int i=0; i<strlen(fw_version_from_server); i++) { Serial.printf("%02X ", fw_version_from_server[i]); }
      Serial.println();
      
      Serial.print("Device bytes: ");
      // <-- FIX: Changed iVScode< to i<
      for(int i=0; i<strlen(FIRMWARE_VERSION); i++) { Serial.printf("%02X ", FIRMWARE_VERSION[i]); }
      Serial.println();
      // --- END DEBUGGING ---
      
      Serial.printf("Received firmware info. Title: %s, Version: %s\n", fw_title, fw_version_from_server);

      // Compare with our current version
      if (strcmp(fw_version_from_server, FIRMWARE_VERSION) != 0) {
        Serial.println("New firmware detected! Starting OTA update process...");
        // This function will download, apply, and restart the device.
        // The code will not proceed to sendTelemetryData() or goToSleep().
        // <-- FIX: Changed performOtoUpdate to performOtaUpdate
        conn.performOtaUpdate(fw_title, fw_version_from_server);
      } else {
        Serial.println("Firmware is already up to date.");
      }
    } else {
      Serial.println("Attributes response received, but 'fw_version' not found.");
    }
  }
}

